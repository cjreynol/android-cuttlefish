/*
 * Copyright (C) 2022 The Android Open Source Project
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
#include "cuttlefish/host/commands/cvd/cli/parser/instance/cf_boot_configs.h"

#include <string>
#include <vector>

#include "cuttlefish/common/libs/utils/base64.h"
#include "cuttlefish/common/libs/utils/result.h"
#include "cuttlefish/host/commands/assemble_cvd/flags_defaults.h"
#include "cuttlefish/host/commands/cvd/cli/parser/cf_configs_common.h"
#include "cuttlefish/host/commands/cvd/cli/parser/load_config.pb.h"

namespace cuttlefish {

using cvd::config::EnvironmentSpecification;
using cvd::config::Instance;

static bool EnableBootAnimation(const Instance& instance) {
  const auto& boot = instance.boot();
  if (boot.has_enable_bootanimation()) {
    return instance.boot().enable_bootanimation();
  } else {
    return CF_DEFAULTS_ENABLE_BOOTANIMATION;
  }
}

static Result<std::string> BootCfgArgs(const Instance& instance) {
  const auto& boot = instance.boot();
  std::string args;
  if (boot.has_extra_bootconfig_args()) {
    args = instance.boot().extra_bootconfig_args();
  } else {
    args = CF_DEFAULTS_EXTRA_BOOTCONFIG_ARGS;
  }
  std::string encoded;
  CF_EXPECT(EncodeBase64(args.data(), args.size(), &encoded));
  return encoded;
}

Result<std::vector<std::string>> GenerateBootFlags(
    const EnvironmentSpecification& cfg) {
  return std::vector<std::string>{
      GenerateInstanceFlag("enable_bootanimation", cfg, EnableBootAnimation),
      CF_EXPECT(
          ResultInstanceFlag("extra_bootconfig_args_base64", cfg, BootCfgArgs)),
  };
}

}  // namespace cuttlefish
