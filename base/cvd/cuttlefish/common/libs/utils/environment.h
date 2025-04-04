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
#pragma once

#include <optional>
#include <string>

namespace cuttlefish {

// TODO CJR - move environment variable constants here
//  so that if we want to remove all usages there is a compilation connection
// TODO CJR - consider moving some of the `host/libs/config/config_utils.h/cpp`
//  helpers here, like `DefaultHostArtifactsPath`
// or maybe I move `IsValidHostOutArtifactsPath` and `GetHostToolPath` from
//  `.../start/main.cc` to that file as a good middle-ground dependency

std::optional<std::string> StringFromEnv(const std::string& varname);

std::string StringFromEnv(const std::string& varname,
                          const std::string& defval);

}  // namespace cuttlefish
