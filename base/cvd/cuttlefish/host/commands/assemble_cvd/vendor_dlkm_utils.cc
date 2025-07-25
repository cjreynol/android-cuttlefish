//
// Copyright (C) 2023 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <fcntl.h>

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <fmt/format.h>

#include "cuttlefish/common/libs/fs/shared_buf.h"
#include "cuttlefish/common/libs/fs/shared_fd.h"
#include "cuttlefish/common/libs/utils/contains.h"
#include "cuttlefish/common/libs/utils/disk_usage.h"
#include "cuttlefish/common/libs/utils/environment.h"
#include "cuttlefish/common/libs/utils/files.h"
#include "cuttlefish/common/libs/utils/subprocess.h"
#include "cuttlefish/host/commands/assemble_cvd/boot_image_utils.h"
#include "cuttlefish/host/commands/assemble_cvd/kernel_module_parser.h"
#include "cuttlefish/host/libs/config/config_utils.h"
#include "cuttlefish/host/libs/config/known_paths.h"

namespace cuttlefish {

namespace {

constexpr size_t RoundDown(size_t a, size_t divisor) {
  return a / divisor * divisor;
}

constexpr size_t RoundUp(size_t a, size_t divisor) {
  return RoundDown(a + divisor, divisor);
}

template <typename Container>
bool WriteLinesToFile(const Container& lines, const char* path) {
  android::base::unique_fd fd(
      open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0640));
  if (!fd.ok()) {
    PLOG(ERROR) << "Failed to open " << path;
    return false;
  }
  for (const auto& line : lines) {
    if (!android::base::WriteFully(fd, line.data(), line.size())) {
      PLOG(ERROR) << "Failed to write to " << path;
      return false;
    }
    const char c = '\n';
    if (write(fd.get(), &c, 1) != 1) {
      PLOG(ERROR) << "Failed to write to " << path;
      return false;
    }
  }
  return true;
}

// Generate a filesystem_config.txt for all files in |fs_root|
Result<bool> WriteFsConfig(const char* output_path, const std::string& fs_root,
                           const std::string& mount_point) {
  android::base::unique_fd fd(
      open(output_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644));
  if (!fd.ok()) {
    PLOG(ERROR) << "Failed to open " << output_path;
    return false;
  }
  if (!android::base::WriteStringToFd(
          " 0 0 755 selabel=u:object_r:rootfs:s0 capabilities=0x0\n", fd)) {
    PLOG(ERROR) << "Failed to write to " << output_path;
    return false;
  }
  auto res = WalkDirectory(fs_root, [&fd, &output_path, &mount_point,
                                     &fs_root](const std::string& file_path) {
    const auto filename = file_path.substr(
        fs_root.back() == '/' ? fs_root.size() : fs_root.size() + 1);
    std::string fs_context = " 0 0 644 capabilities=0x0\n";
    if (DirectoryExists(file_path)) {
      fs_context = " 0 0 755 capabilities=0x0\n";
    }
    if (!android::base::WriteStringToFd(
            mount_point + "/" + filename + fs_context, fd)) {
      PLOG(ERROR) << "Failed to write to " << output_path;
      return false;
    }
    return true;
  });
  if (!res.ok()) {
    return false;
  }
  return true;
}

std::vector<std::string> GetRamdiskModules(
    const std::vector<std::string>& all_modules) {
  static constexpr auto kRamdiskModules = {
      "failover.ko",
      "nd_virtio.ko",
      "net_failover.ko",
      "virtio_blk.ko",
      "virtio_console.ko",
      "virtio_dma_buf.ko",
      "virtio-gpu.ko",
      "virtio_input.ko",
      "virtio_mmio.ko",
      "virtio_net.ko",
      "virtio_pci.ko",
      "virtio_pci_legacy_dev.ko",
      "virtio_pci_modern_dev.ko",
      "virtio-rng.ko",
      "vmw_vsock_virtio_transport.ko",
      "vmw_vsock_virtio_transport_common.ko",
      "vsock.ko",
      // TODO(b/176860479) once virt_wifi is deprecated fully,
      // these following modules can be loaded in second stage init
      "libarc4.ko",
      "rfkill.ko",
      "cfg80211.ko",
      "mac80211.ko",
      "mac80211_hwsim.ko",
  };
  std::vector<std::string> ramdisk_modules;
  for (const auto& mod_path : all_modules) {
    if (Contains(kRamdiskModules, android::base::Basename(mod_path))) {
      ramdisk_modules.emplace_back(mod_path);
    }
  }
  return ramdisk_modules;
}

// Filter the dependency map |deps| to only contain nodes in |allow_list|
std::map<std::string, std::vector<std::string>> FilterDependencies(
    const std::map<std::string, std::vector<std::string>>& deps,
    const std::set<std::string>& allow_list) {
  std::map<std::string, std::vector<std::string>> new_deps;
  for (const auto& [mod_name, children] : deps) {
    if (!allow_list.count(mod_name)) {
      continue;
    }
    new_deps[mod_name].clear();
    for (const auto& child : children) {
      if (!allow_list.count(child)) {
        continue;
      }
      new_deps[mod_name].emplace_back(child);
    }
  }
  return new_deps;
}

std::map<std::string, std::vector<std::string>> FilterOutDependencies(
    const std::map<std::string, std::vector<std::string>>& deps,
    const std::set<std::string>& block_list) {
  std::map<std::string, std::vector<std::string>> new_deps;
  for (const auto& [mod_name, children] : deps) {
    if (block_list.count(mod_name)) {
      continue;
    }
    new_deps[mod_name].clear();
    for (const auto& child : children) {
      if (block_list.count(child)) {
        continue;
      }
      new_deps[mod_name].emplace_back(child);
    }
  }
  return new_deps;
}

// Update dependency map by prepending '/system/lib/modules' to modules which
// have been relocated to system_dlkm partition
std::map<std::string, std::vector<std::string>> UpdateGKIModulePaths(
    const std::map<std::string, std::vector<std::string>>& deps,
    const std::set<std::string>& gki_modules) {
  std::map<std::string, std::vector<std::string>> new_deps;
  auto&& GetNewModName = [gki_modules](auto&& mod_name) {
    if (gki_modules.count(mod_name)) {
      return "/system/lib/modules/" + mod_name;
    } else {
      return mod_name;
    }
  };
  for (const auto& [old_mod_name, children] : deps) {
    const auto mod_name = GetNewModName(old_mod_name);
    new_deps[mod_name].clear();

    for (const auto& child : children) {
      new_deps[mod_name].emplace_back(GetNewModName(child));
    }
  }
  return new_deps;
}

// Write dependency map to modules.dep file
bool WriteDepsToFile(
    const std::map<std::string, std::vector<std::string>>& deps,
    const std::string& output_path) {
  std::stringstream ss;
  for (const auto& [key, val] : deps) {
    ss << key << ":";
    for (const auto& dep : val) {
      ss << " " << dep;
    }
    ss << "\n";
  }
  if (!android::base::WriteStringToFile(ss.str(), output_path)) {
    PLOG(ERROR) << "Failed to write modules.dep to " << output_path;
    return false;
  }
  return true;
}

// Parse modules.dep into an in-memory data structure, key is path to a kernel
// module, value is all dependency modules
std::map<std::string, std::vector<std::string>> LoadModuleDeps(
    const std::string& filename) {
  std::map<std::string, std::vector<std::string>> dependency_map;
  const auto dep_str = android::base::Trim(ReadFile(filename));
  const auto dep_lines = android::base::Split(dep_str, "\n");
  for (const auto& line : dep_lines) {
    const auto mod_name = line.substr(0, line.find(":"));
    auto deps = android::base::Tokenize(line.substr(mod_name.size() + 1), " ");
    dependency_map[mod_name] = std::move(deps);
  }

  return dependency_map;
}

// Recursively compute all modules which |start_nodes| depend on
template <typename StringContainer>
std::set<std::string> ComputeTransitiveClosure(
    const StringContainer& start_nodes,
    const std::map<std::string, std::vector<std::string>>& dependencies) {
  std::deque<std::string> queue(start_nodes.begin(), start_nodes.end());
  std::set<std::string> visited;
  while (!queue.empty()) {
    const auto cur = queue.front();
    queue.pop_front();
    if (visited.find(cur) != visited.end()) {
      continue;
    }
    visited.insert(cur);
    const auto it = dependencies.find(cur);
    if (it == dependencies.end()) {
      continue;
    }
    for (const auto& dep : it->second) {
      queue.emplace_back(dep);
    }
  }
  return visited;
}

// Generate a file_context.bin file which can be used by selinux tools to assign
// selinux labels to files
Result<void> GenerateFileContexts(const std::string& output_path,
                                  std::string_view mount_point,
                                  std::string_view file_label) {
  const std::string file_contexts_txt = output_path + ".txt";
  SharedFD fd = SharedFD::Open(file_contexts_txt,
                               O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  CF_EXPECTF(fd->IsOpen(), "Can't open '{}': {}", output_path, fd->StrError());

  std::string line = fmt::format("{}(/.*)?         u:object_r:{}:s0\n",
                                 mount_point, file_label);
  CF_EXPECT_EQ(WriteAll(fd, line), line.size(), fd->StrError());

  int exit_code = Execute({
      HostBinaryPath("sefcontext_compile"),
      "-o",
      output_path,
      file_contexts_txt,
  });

  CF_EXPECT_EQ(exit_code, 0);

  return {};
}

Result<void> AddVbmetaFooter(const std::string& output_image,
                             const std::string& partition_name) {
  // TODO(b/335742241): update to use Avb
  std::string avbtool_path = AvbToolBinary();
  // Add host binary path to PATH, so that avbtool can locate host util binaries
  // such as 'fec'
  std::string env_path =
      StringFromEnv("PATH", "") + ":" + android::base::Dirname(avbtool_path);
  Command avb_cmd =
      Command(AvbToolBinary())
          // Must unset an existing environment variable in order to modify it
          .UnsetFromEnvironment("PATH")
          .AddEnvironmentVariable("PATH", env_path)
          .AddParameter("add_hashtree_footer")
          // Arbitrary salt to keep output consistent
          .AddParameter("--salt")
          .AddParameter("62BBAAA0", "E4BD99E783AC")
          .AddParameter("--hash_algorithm")
          .AddParameter("sha256")
          .AddParameter("--image")
          .AddParameter(output_image)
          .AddParameter("--partition_name")
          .AddParameter(partition_name);

  CF_EXPECT_EQ(avb_cmd.Start().Wait(), 0,
               "Failed to add avb footer to image " << output_image);

  return {};
}

}  // namespace

// Steps for building a vendor_dlkm.img:
// 1. Generate filesystem_config.txt , which contains standard linux file
// permissions, we use 0755 for directories, and 0644 for all files
// 2. Write file_contexts, which contains all selinux labels
// 3. Call  sefcontext_compile to compile file_contexts
// 4. call mkuserimg_mke2fs to build an image, using filesystem_config and
// file_contexts previously generated
// 5. call avbtool to add hashtree footer, so that init/bootloader can verify
// AVB chain
Result<void> BuildDlkmImage(const std::string& src_dir, const bool is_erofs,
                            const std::string& partition_name,
                            const std::string& output_image) {
  CF_EXPECT(!is_erofs,
            "Building DLKM image in EROFS format is currently not supported!");

  const std::string mount_point = "/" + partition_name;
  const std::string fs_config = output_image + ".fs_config";
  CF_EXPECT(WriteFsConfig(fs_config.c_str(), src_dir, mount_point));

  const std::string file_contexts_bin = output_image + ".file_contexts";
  if (partition_name == "system_dlkm") {
    CF_EXPECT(GenerateFileContexts(file_contexts_bin.c_str(), mount_point,
                                   "system_dlkm_file"));
  } else {
    CF_EXPECT(GenerateFileContexts(file_contexts_bin.c_str(), mount_point,
                                   "vendor_file"));
  }

  // We are using directory size as an estimate of final image size. To avoid
  // any rounding errors, add 16M of head room.
  const auto fs_size =
      RoundUp(CF_EXPECT(GetDiskUsageBytes(src_dir)) + 16 * 1024 * 1024, 4096);
  LOG(INFO) << mount_point << " src dir " << src_dir << " has size "
            << fs_size / 1024 << " KB";

  Command mkfs_cmd =
      Command(MkuserimgMke2fsBinary())
          // Arbitrary UUID/seed, just to keep output consistent between runs
          .AddParameter("--mke2fs_uuid")
          .AddParameter("cb09b942-ed4e-46a1-81dd-7d535bf6c4b1")
          .AddParameter("--mke2fs_hash_seed")
          .AddParameter("765d8aba-d93f-465a-9fcf-14bb794eb7f4")
          // Arbitrary date, just to keep output consistent
          .AddParameter("-T")
          .AddParameter(900979200000)
          // selinux permission to keep selinux happy
          .AddParameter("--fs_config")
          .AddParameter(fs_config)

          .AddParameter(src_dir)
          .AddParameter(output_image)
          .AddParameter("ext4")
          .AddParameter(mount_point)
          .AddParameter(fs_size)
          .AddParameter(file_contexts_bin);

  CF_EXPECT_EQ(mkfs_cmd.Start().Wait(), 0,
               "Failed to build vendor_dlkm ext4 image");
  CF_EXPECT(AddVbmetaFooter(output_image, partition_name));

  return {};
}

Result<void> RepackSuperWithPartition(const std::string& superimg_path,
                                      const std::string& image_path,
                                      const std::string& partition_name) {
  int exit_code = Execute({
      HostBinaryPath("lpadd"),
      "--replace",
      superimg_path,
      partition_name + "_a",
      "google_vendor_dynamic_partitions_a",
      image_path,
  });
  CF_EXPECT_EQ(exit_code, 0);

  return {};
}

Result<void> BuildVbmetaImage(const std::string& image_path,
                              const std::string& vbmeta_path) {
  CF_EXPECT(!image_path.empty());
  CF_EXPECTF(FileExists(image_path), "'{}' does not exist", image_path);

  std::unique_ptr<Avb> avbtool = GetDefaultAvb();
  CF_EXPECT(avbtool->MakeVbMetaImage(vbmeta_path, {}, {image_path},
                                     {"--padding_size", "4096"}));
  return {};
}

std::vector<std::string> Dedup(std::vector<std::string>&& vec) {
  std::sort(vec.begin(), vec.end());
  vec.erase(unique(vec.begin(), vec.end()), vec.end());
  return vec;
}

bool SplitRamdiskModules(const std::string& ramdisk_path,
                         const std::string& ramdisk_stage_dir,
                         const std::string& vendor_dlkm_build_dir,
                         const std::string& system_dlkm_build_dir) {
  const auto vendor_modules_dir = vendor_dlkm_build_dir + "/lib/modules";
  const auto system_modules_dir = system_dlkm_build_dir + "/lib/modules";
  auto ret = EnsureDirectoryExists(vendor_modules_dir);
  CHECK(ret.ok()) << ret.error().FormatForEnv();
  ret = EnsureDirectoryExists(system_modules_dir);
  UnpackRamdisk(ramdisk_path, ramdisk_stage_dir);
  auto res = FindFile(ramdisk_stage_dir.c_str(), "modules.load");
  if (!res.ok()) {
    LOG(ERROR) << "Failed to find modules.dep file in input ramdisk "
               << ramdisk_path;
    return false;
  }
  const auto module_load_file = android::base::Trim(res.value());
  if (module_load_file.empty()) {
    LOG(ERROR) << "Failed to find modules.dep file in input ramdisk "
               << ramdisk_path;
    return false;
  }
  LOG(INFO) << "modules.load location " << module_load_file;
  const auto module_list =
      Dedup(android::base::Tokenize(ReadFile(module_load_file), "\n"));
  const auto module_base_dir = android::base::Dirname(module_load_file);
  const auto deps = LoadModuleDeps(module_base_dir + "/modules.dep");
  const auto ramdisk_modules =
      ComputeTransitiveClosure(GetRamdiskModules(module_list), deps);
  std::set<std::string> vendor_dlkm_modules;
  std::set<std::string> system_dlkm_modules;

  // Move non-ramdisk modules to vendor_dlkm
  for (const auto& module_path : module_list) {
    if (ramdisk_modules.count(module_path)) {
      continue;
    }

    const auto module_location =
        fmt::format("{}/{}", module_base_dir, module_path);
    if (!FileExists(module_location)) {
      continue;
    }
    if (IsKernelModuleSigned(module_location)) {
      const auto system_dlkm_module_location =
          fmt::format("{}/{}", system_modules_dir, module_path);
      auto res = EnsureDirectoryExists(
          android::base::Dirname(system_dlkm_module_location));
      CHECK(res.ok()) << res.error().FormatForEnv();
      auto ret = RenameFile(module_location, system_dlkm_module_location);
      CHECK(ret.ok()) << ret.error().FormatForEnv();
      system_dlkm_modules.emplace(module_path);
    } else {
      const auto vendor_dlkm_module_location =
          fmt::format("{}/{}", vendor_modules_dir, module_path);
      auto res = EnsureDirectoryExists(
          android::base::Dirname(vendor_dlkm_module_location));
      CHECK(res.ok()) << res.error().FormatForEnv();
      auto ret = RenameFile(module_location, vendor_dlkm_module_location);
      CHECK(ret.ok()) << ret.error().FormatForEnv();
      vendor_dlkm_modules.emplace(module_path);
    }
  }
  for (const auto& gki_module : system_dlkm_modules) {
    for (const auto& dep : deps.at(gki_module)) {
      if (vendor_dlkm_modules.count(dep)) {
        LOG(ERROR) << "GKI module " << gki_module
                   << " depends on vendor_dlkm module " << dep;
        return false;
      }
    }
  }
  LOG(INFO) << "There are " << ramdisk_modules.size() << " ramdisk modules, "
            << vendor_dlkm_modules.size() << " vendor_dlkm modules, "
            << system_dlkm_modules.size() << " system_dlkm modules.";

  // transfer blocklist in whole to the vendor dlkm partition. It currently
  // only contains one module that is loaded during second stage init.
  // We can split the blocklist at a later date IF it contains modules in
  // different partitions.
  const auto initramfs_blocklist_path = module_base_dir + "/modules.blocklist";
  if (FileExists(initramfs_blocklist_path)) {
    const auto vendor_dlkm_blocklist_path =
        fmt::format("{}/{}", vendor_modules_dir, "modules.blocklist");
    auto ret = RenameFile(initramfs_blocklist_path, vendor_dlkm_blocklist_path);
    CHECK(ret.ok()) << ret.error().FormatForEnv();
  }

  // Write updated modules.dep and modules.load files
  CHECK(WriteDepsToFile(FilterDependencies(deps, ramdisk_modules),
                        module_base_dir + "/modules.dep"));
  CHECK(WriteLinesToFile(ramdisk_modules, module_load_file.c_str()));

  CHECK(WriteDepsToFile(
      UpdateGKIModulePaths(FilterOutDependencies(deps, ramdisk_modules),
                           system_dlkm_modules),
      vendor_modules_dir + "/modules.dep"));
  CHECK(WriteLinesToFile(vendor_dlkm_modules,
                         (vendor_modules_dir + "/modules.load").c_str()));

  CHECK(WriteDepsToFile(FilterDependencies(deps, system_dlkm_modules),
                        system_modules_dir + "/modules.dep"));
  CHECK(WriteLinesToFile(system_dlkm_modules,
                         (system_modules_dir + "/modules.load").c_str()));
  PackRamdisk(ramdisk_stage_dir, ramdisk_path);
  return true;
}

bool FileEquals(const std::string& file1, const std::string& file2) {
  if (FileSize(file1) != FileSize(file2)) {
    return false;
  }
  std::array<uint8_t, 1024 * 16> buf1{};
  std::array<uint8_t, 1024 * 16> buf2{};
  auto fd1 = SharedFD::Open(file1, O_RDONLY);
  auto fd2 = SharedFD::Open(file2, O_RDONLY);
  auto bytes_remain = FileSize(file1);
  while (bytes_remain > 0) {
    const auto bytes_to_read = std::min<size_t>(bytes_remain, buf1.size());
    if (fd1->Read(buf1.data(), bytes_to_read) != bytes_to_read) {
      LOG(ERROR) << "Failed to read from " << file1;
      return false;
    }
    if (!fd2->Read(buf2.data(), bytes_to_read)) {
      LOG(ERROR) << "Failed to read from " << file2;
      return false;
    }
    if (memcmp(buf1.data(), buf2.data(), bytes_to_read)) {
      return false;
    }
  }
  return true;
}

bool MoveIfChanged(const std::string& src, const std::string& dst) {
  if (FileEquals(src, dst)) {
    return false;
  }
  const auto ret = RenameFile(src, dst);
  if (!ret.ok()) {
    LOG(ERROR) << ret.error().FormatForEnv();
    return false;
  }
  return true;
}

}  // namespace cuttlefish
