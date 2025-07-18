//
// Copyright (C) 2020 The Android Open Source Project
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

#pragma once

#include <memory>

#include <tss2/tss2_tcti.h>

#include "cuttlefish/host/commands/secure_env/tpm.h"

namespace cuttlefish {

/*
 * Exposes a TSS2_TCTI_CONTEXT for interacting with a TPM device node.
 */
class DeviceTpm : public Tpm {
public:
  DeviceTpm(const std::string& path);
  ~DeviceTpm() = default;

  TSS2_TCTI_CONTEXT* TctiContext() override;
private:
  std::unique_ptr<TSS2_TCTI_CONTEXT, void(*)(TSS2_TCTI_CONTEXT*)> tpm_;
};

}  // namespace cuttlefish
