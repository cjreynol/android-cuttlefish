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

#pragma once

#include <string>
#include <vector>

#include "cuttlefish/common/libs/utils/result.h"

namespace cuttlefish {

struct ExistenceFlags {
  bool system_image = false;
  bool boot_image = false;
  bool bootloader = false;
  bool composite_disk = false;
  bool data_image = false;
  bool metadata_image = false;
  bool qemu_binary = false;
  bool super_image = false;
  bool vendor_boot_image = false;
};

struct LaunchFlags {
  std::string adb_mode;
  std::string cpus;
  std::string daemon;
  std::string data_policy;
  std::string decompress_kernel;
  std::string dpi;
  std::string enable_tombstone_receiver;
  std::string extra_kernel_cmdline;
  std::string gpu_mode;
  std::string guest_enforce_security;
  std::string logcat_mode;
  std::string loop_max_part;
  std::string memory_mb;
  std::string mobile_interface;
  std::string refresh_rate_hz;
  std::string restart_subprocesses;
  std::string run_adb_connector;
  std::string start_vnc_server;
  std::string use_bootloader;
  std::string vm_manager;
  std::string x_res;
  std::string y_res;
};

struct FlagMetrics {
  ExistenceFlags existence_flags;
  LaunchFlags launch_flags;
};

Result<FlagMetrics> GetFlagMetrics(const std::vector<std::string>& flags);

}  // namespace cuttlefish
