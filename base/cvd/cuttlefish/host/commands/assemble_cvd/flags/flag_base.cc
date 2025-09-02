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

#include "cuttlefish/host/commands/assemble_cvd/flags/flag_base.h"

#include <string>

namespace cuttlefish {

template struct FlagValues<bool>;
template struct FlagValues<int>;
template struct FlagValues<std::string>;

template <typename T>
T FlagBase<T>::ForIndex(const std::size_t index) const {
  if (index < values_.size()) {
    return values_[index];
  } else {
    return default_value_;
  }
}

template <typename T>
FlagBase<T>::FlagBase(FlagValues<T> flag_values)
    : default_value_(std::move(flag_values.default_value)),
      values_(std::move(flag_values.values)) {}

template class FlagBase<bool>;
template class FlagBase<int>;
template class FlagBase<std::string>;

}  // namespace cuttlefish
