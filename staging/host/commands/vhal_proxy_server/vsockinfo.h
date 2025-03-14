/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <array>
#include <optional>
#include <sstream>
#include <string>

namespace android::hardware::automotive::utils {

using PropertyList = std::initializer_list<std::string>;

struct VsockConnectionInfo {
  unsigned cid = 0;
  unsigned port = 0;

  std::string str() const {
    std::stringstream ss;

    ss << "vsock:" << cid << ":" << port;
    return ss.str();
  }
};

}  // namespace android::hardware::automotive::utils
