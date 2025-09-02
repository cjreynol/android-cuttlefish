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

#include <cstdlib>
#include <string>
#include <vector>

#include "cuttlefish/common/libs/utils/result.h"

namespace cuttlefish {

class DataPolicyFlag {
 public:
  static Result<DataPolicyFlag> FromGlobalGflags();

  std::string ForIndex(std::size_t index) const;

 private:
  DataPolicyFlag(const std::string, std::vector<std::string>);

  std::string default_value_;
  std::vector<std::string> data_policy_values_;
};

}  // namespace cuttlefish
