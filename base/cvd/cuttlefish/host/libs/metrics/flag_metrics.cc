/*
 * Copyright (C) 2025 The Android Open Source Project
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

#include "cuttlefish/host/libs/metrics/flag_metrics.h"

#include <string>
#include <vector>

#include "cuttlefish/common/libs/utils/flag_parser.h"
#include "cuttlefish/common/libs/utils/result.h"

namespace cuttlefish {
namespace {

struct ExistenceFlagValues {
  std::string system_image;
  std::string boot_image;
  std::string bootloader;
  std::string composite_disk;
  std::string data_image;
  std::string metadata_image;
  std::string qemu_binary;
  std::string super_image;
  std::string vendor_boot_image;
};

std::vector<Flag> GetLaunchFlagsVector(LaunchFlags& launch_flags) {
  std::vector<Flag> flags;
  flags.emplace_back(GflagsCompatFlag("adb_mode", launch_flags.adb_mode));
  flags.emplace_back(GflagsCompatFlag("cpus", launch_flags.cpus));
  flags.emplace_back(GflagsCompatFlag("daemon", launch_flags.daemon));
  flags.emplace_back(GflagsCompatFlag("data_policy", launch_flags.data_policy));
  flags.emplace_back(
      GflagsCompatFlag("decompress_kernel", launch_flags.decompress_kernel));
  flags.emplace_back(GflagsCompatFlag("dpi", launch_flags.dpi));
  flags.emplace_back(GflagsCompatFlag("enable_tombstone_receiver",
                                      launch_flags.enable_tombstone_receiver));
  flags.emplace_back(GflagsCompatFlag("extra_kernel_cmdline",
                                      launch_flags.extra_kernel_cmdline));
  flags.emplace_back(GflagsCompatFlag("gpu_mode", launch_flags.gpu_mode));
  flags.emplace_back(GflagsCompatFlag("guest_enforce_security",
                                      launch_flags.guest_enforce_security));
  flags.emplace_back(GflagsCompatFlag("logcat_mode", launch_flags.logcat_mode));
  flags.emplace_back(
      GflagsCompatFlag("loop_max_part", launch_flags.loop_max_part));
  flags.emplace_back(GflagsCompatFlag("memory_mb", launch_flags.memory_mb));
  flags.emplace_back(
      GflagsCompatFlag("mobile_interface", launch_flags.mobile_interface));
  flags.emplace_back(
      GflagsCompatFlag("refresh_rate_hz", launch_flags.refresh_rate_hz));
  flags.emplace_back(GflagsCompatFlag("restart_subprocesses",
                                      launch_flags.restart_subprocesses));
  flags.emplace_back(
      GflagsCompatFlag("run_adb_connector", launch_flags.run_adb_connector));
  flags.emplace_back(
      GflagsCompatFlag("start_vnc_server", launch_flags.start_vnc_server));
  flags.emplace_back(
      GflagsCompatFlag("use_bootloader", launch_flags.use_bootloader));
  flags.emplace_back(GflagsCompatFlag("vm_manager", launch_flags.vm_manager));
  flags.emplace_back(GflagsCompatFlag("x_res", launch_flags.x_res));
  flags.emplace_back(GflagsCompatFlag("y_res", launch_flags.y_res));
  return flags;
}

Result<LaunchFlags> GetLaunchFlags(std::vector<std::string>& flags) {
  LaunchFlags launch_flags;
  std::vector<Flag> launch_flags_vector = GetLaunchFlagsVector(launch_flags);
  CF_EXPECT(ConsumeFlags(launch_flags_vector, flags));
  return launch_flags;
}

std::vector<Flag> GetExistenceFlagsValuesVector(ExistenceFlagValues& values) {
  std::vector<Flag> flags;
  flags.emplace_back(GflagsCompatFlag("system_image", values.system_image));
  flags.emplace_back(GflagsCompatFlag("boot_image", values.boot_image));
  flags.emplace_back(GflagsCompatFlag("bootloader", values.bootloader));
  flags.emplace_back(GflagsCompatFlag("composite_disk", values.composite_disk));
  flags.emplace_back(GflagsCompatFlag("data_image", values.data_image));
  flags.emplace_back(GflagsCompatFlag("metadata_image", values.metadata_image));
  flags.emplace_back(GflagsCompatFlag("qemu_binary", values.qemu_binary));
  flags.emplace_back(GflagsCompatFlag("super_image", values.super_image));
  flags.emplace_back(
      GflagsCompatFlag("vendor_boot_image", values.vendor_boot_image));
  return flags;
}

Result<ExistenceFlags> GetExistenceFlags(std::vector<std::string>& flags) {
  ExistenceFlagValues values;
  std::vector<Flag> existence_flags_vector =
      GetExistenceFlagsValuesVector(values);
  CF_EXPECT(ConsumeFlags(existence_flags_vector, flags));

  ExistenceFlags existence_flags;
  if (!values.system_image.empty()) {
    existence_flags.system_image = true;
  }
  if (!values.boot_image.empty()) {
    existence_flags.boot_image = true;
  }
  if (!values.bootloader.empty()) {
    existence_flags.bootloader = true;
  }
  if (!values.composite_disk.empty()) {
    existence_flags.composite_disk = true;
  }
  if (!values.data_image.empty()) {
    existence_flags.data_image = true;
  }
  if (!values.metadata_image.empty()) {
    existence_flags.metadata_image = true;
  }
  if (!values.qemu_binary.empty()) {
    existence_flags.qemu_binary = true;
  }
  if (!values.super_image.empty()) {
    existence_flags.super_image = true;
  }
  if (!values.vendor_boot_image.empty()) {
    existence_flags.vendor_boot_image = true;
  }
  return existence_flags;
}

}  // namespace

Result<FlagMetrics> GetFlagMetrics(const std::vector<std::string>& flags) {
  std::vector<std::string> flags_copy(flags);
  return FlagMetrics{
      .existence_flags = CF_EXPECT(GetExistenceFlags(flags_copy)),
      .launch_flags = CF_EXPECT(GetLaunchFlags(flags_copy)),
  };
}

}  // namespace cuttlefish
