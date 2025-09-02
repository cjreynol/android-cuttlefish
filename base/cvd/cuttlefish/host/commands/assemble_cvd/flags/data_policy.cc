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

#include "cuttlefish/host/commands/assemble_cvd/flags/data_policy.h"

#include <cstdlib>
#include <string>
#include <vector>

#include <android-base/strings.h>
#include <gflags/gflags.h>

#include "cuttlefish/common/libs/utils/result.h"
#include "cuttlefish/host/commands/assemble_cvd/flags_defaults.h"

DEFINE_string(data_policy, CF_DEFAULTS_DATA_POLICY,
              "How to handle userdata partition."
              " Either 'use_existing', 'create_if_missing', 'resize_up_to', or "
              "'always_create'.");

namespace cuttlefish {

Result<DataPolicyFlag> DataPolicyFlag::FromGlobalGflags() {
  gflags::CommandLineFlagInfo flag_info =
      gflags::GetCommandLineFlagInfoOrDie("data_policy");
  std::vector<std::string> string_values =
      android::base::Split(flag_info.current_value, ",");

  for (int i = 0; i < string_values.size(); i++) {
    if (string_values[i] == "unset" || string_values[i] == "\"unset\"") {
      string_values[i] = flag_info.default_value;
    }
  }
  return DataPolicyFlag(flag_info.default_value, std::move(string_values));
}

std::string DataPolicyFlag::ForIndex(std::size_t index) const {
  if (index < data_policy_values_.size()) {
    return data_policy_values_[index];
  } else {
    return default_value_;
  }
}

DataPolicyFlag::DataPolicyFlag(const std::string default_value,
                               std::vector<std::string> data_policy_values)
    : default_value_(std::move(default_value)),
      data_policy_values_(std::move(data_policy_values)) {}

}  // namespace cuttlefish
