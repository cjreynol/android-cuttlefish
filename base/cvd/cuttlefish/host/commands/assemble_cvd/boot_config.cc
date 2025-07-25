/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cuttlefish/host/commands/assemble_cvd/boot_config.h"

#include <fstream>
#include <sstream>
#include <string>

#include <sys/stat.h>

#include <android-base/logging.h>
#include <android-base/strings.h>
#include <gflags/gflags.h>

#include "cuttlefish/common/libs/utils/files.h"
#include "cuttlefish/common/libs/utils/result.h"
#include "cuttlefish/common/libs/utils/size_utils.h"
#include "cuttlefish/common/libs/utils/subprocess.h"
#include "cuttlefish/host/commands/assemble_cvd/bootconfig_args.h"
#include "cuttlefish/host/libs/avb/avb.h"
#include "cuttlefish/host/libs/config/ap_boot_flow.h"
#include "cuttlefish/host/libs/config/boot_flow.h"
#include "cuttlefish/host/libs/config/cuttlefish_config.h"
#include "cuttlefish/host/libs/config/kernel_args.h"
#include "cuttlefish/host/libs/vm_manager/crosvm_manager.h"

using cuttlefish::vm_manager::CrosvmManager;

namespace cuttlefish {
namespace {

// The ordering of tap devices we're passing to crosvm / qemu is important
// Ethernet tap device is the second one (eth1) we're passing ATM
static constexpr char kUbootPrimaryEth[] = "eth1";

void WritePausedEntrypoint(const std::string& entrypoint,
                           const CuttlefishConfig::InstanceSpecific& instance,
                           std::ostream& env) {
  if (instance.pause_in_bootloader()) {
    env << "if test $paused -ne 1; then paused=1; else " << entrypoint << "; fi";
  } else {
    env << entrypoint;
  }

  env << '\0';
}

void WriteAndroidEnvironment(
    std::ostream& env,
    const CuttlefishConfig::InstanceSpecific& instance) {
  WritePausedEntrypoint("run bootcmd_android", instance, env);

  if (!instance.boot_slot().empty()) {
    env << "android_slot_suffix=_" << instance.boot_slot() << '\0';
  }
  env << '\0';
}

void WriteEFIEnvironment(const CuttlefishConfig::InstanceSpecific& instance,
                         std::optional<std::uint16_t> partition_num,
                         std::ostream& env) {
  std::string partition_str =
      partition_num ? fmt::format("setenv devplist {:x};", *partition_num) : "";
  WritePausedEntrypoint(
      partition_str +
          "load virtio 0:${devplist} ${loadaddr} efi/boot/bootaa64.efi "
          "&& bootefi ${loadaddr} ${fdtcontroladdr}; "
          "load virtio 0:${devplist} ${loadaddr} efi/boot/bootia32.efi && "
          "bootefi ${loadaddr} ${fdtcontroladdr};"
          "load virtio 0:${devplist} ${loadaddr} efi/boot/bootriscv64.efi && "
          "bootefi ${loadaddr} ${fdtcontroladdr}",
      instance, env);
}

size_t WriteEnvironment(const CuttlefishConfig::InstanceSpecific& instance,
                        const BootFlow& flow, const std::string& kernel_args,
                        const std::string& env_path) {
  std::ostringstream env;

  env << "ethprime=" << kUbootPrimaryEth << '\0';
  if (!kernel_args.empty()) {
    env << "uenvcmd=setenv bootargs \"$cbootargs " << kernel_args << "\" && ";
  } else {
    env << "uenvcmd=setenv bootargs \"$cbootargs\" && ";
  }

  switch (flow) {
    case BootFlow::Android:
      WriteAndroidEnvironment(env, instance);
      break;
    case BootFlow::AndroidEfiLoader:
      WriteEFIEnvironment(instance, 1, env);
      break;
    case BootFlow::ChromeOs:
      WriteEFIEnvironment(instance, 2, env);
      break;
    case BootFlow::ChromeOsDisk:
      WriteEFIEnvironment(instance, 12, env);
      break;
    case BootFlow::Fuchsia:
    case BootFlow::Linux:
      WriteEFIEnvironment(instance, {}, env);
      break;
  }

  std::string env_str = env.str();
  std::ofstream file_out(env_path.c_str(), std::ios::binary);
  file_out << env_str;

  if (!file_out.good()) {
    return 0;
  }

  return env_str.length();
}

std::unordered_map<std::string, std::string> ReplaceKernelBootArgs(
    const std::unordered_map<std::string, std::string>& args) {
  std::unordered_map<std::string, std::string> ret;
  std::transform(std::begin(args), std::end(args),
                 std::inserter(ret, ret.end()), [](const auto& kv) {
                   const auto& k = kv.first;
                   const auto& v = kv.second;
                   return std::make_pair(
                       android::base::StringReplace(k, " kernel.", " ", true),
                       v);
                 });
  return ret;
}

Result<void> PrepareBootEnvImage(
    const CuttlefishConfig& config,
    const CuttlefishConfig::InstanceSpecific& instance,
    const std::string& image_path, const BootFlow& flow) {
  auto tmp_boot_env_image_path = image_path + ".tmp";
  auto uboot_env_path = instance.PerInstancePath("mkenvimg_input");
  auto kernel_cmdline =
      android::base::Join(KernelCommandLineFromConfig(config, instance), " ");
  // If the bootconfig isn't supported in the guest kernel, the bootconfig
  // args need to be passed in via the uboot env. This won't be an issue for
  // protect kvm which is running a kernel with bootconfig support.
  if (!instance.bootconfig_supported()) {
    auto bootconfig_args =
        CF_EXPECT(BootconfigArgsFromConfig(config, instance));

    // "androidboot.hardware" kernel parameter has changed to "hardware" in
    // bootconfig and needs to be replaced before being used in the kernel
    // cmdline.
    auto bootconfig_hardware_it = bootconfig_args.find("hardware");
    if (bootconfig_hardware_it != bootconfig_args.end()) {
      bootconfig_args["androidboot.hardware"] = bootconfig_hardware_it->second;
      bootconfig_args.erase(bootconfig_hardware_it);
    }

    // TODO(b/182417593): Until we pass the module parameters through
    // modules.options, we pass them through bootconfig using
    // 'kernel.<key>=<value>' But if we don't support bootconfig, we need to
    // rename them back to the old cmdline version
    bootconfig_args = ReplaceKernelBootArgs(bootconfig_args);

    kernel_cmdline +=
        " " + CF_EXPECT(BootconfigArgsString(bootconfig_args, " "));
  }

  CF_EXPECTF(WriteEnvironment(instance, flow, kernel_cmdline, uboot_env_path),
             "Unable to write out plaintext env '{}'", uboot_env_path);

  auto mkimage_path = HostBinaryPath("mkenvimage_slim");
  Command cmd(mkimage_path);
  cmd.AddParameter("-output_path");
  cmd.AddParameter(tmp_boot_env_image_path);
  cmd.AddParameter("-input_path");
  cmd.AddParameter(uboot_env_path);
  int success = cmd.Start().Wait();
  CF_EXPECTF(success == 0,
             "Unable to run mkenvimage_slim. Exited with status {}", success);

  const off_t boot_env_size_bytes =
      AlignToPowerOf2(kMaxAvbMetadataSize + 4096, PARTITION_SIZE_SHIFT);

  std::unique_ptr<Avb> avbtool = GetDefaultAvb();
  CF_EXPECT(avbtool->AddHashFooter(tmp_boot_env_image_path, "uboot_env",
                                   boot_env_size_bytes));

  if (!FileExists(image_path) ||
      ReadFile(image_path) != ReadFile(tmp_boot_env_image_path)) {
    CF_EXPECT(RenameFile(tmp_boot_env_image_path, image_path),
              "Unable to delete the old env image");
    LOG(DEBUG) << "Updated bootloader environment image.";
  } else {
    RemoveFile(tmp_boot_env_image_path);
  }

  return {};
}

}  // namespace

Result<BootloaderEnvPartition> BootloaderEnvPartition::Create(
    const CuttlefishConfig& config,
    const CuttlefishConfig::InstanceSpecific& instance) {
  CF_EXPECT(PrepareBootEnvImage(
      config, instance, instance.uboot_env_image_path(), instance.boot_flow()));
  return BootloaderEnvPartition();
}

Result<std::optional<ApBootloaderEnvPartition>>
ApBootloaderEnvPartition::Create(
    const CuttlefishConfig& config,
    const CuttlefishConfig::InstanceSpecific& instance) {
  if (instance.ap_boot_flow() != APBootFlow::Grub) {
    return std::nullopt;
  }
  CF_EXPECT(PrepareBootEnvImage(
      config, instance, instance.ap_uboot_env_image_path(), BootFlow::Linux));
  return ApBootloaderEnvPartition();
}

} // namespace cuttlefish
