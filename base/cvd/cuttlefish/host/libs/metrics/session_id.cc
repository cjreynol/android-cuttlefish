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

#include "cuttlefish/host/libs/metrics/session_id.h"

#include <uuid/uuid.h>

#include "cuttlefish/common/libs/fs/shared_buf.h"
#include "cuttlefish/common/libs/fs/shared_fd.h"
#include "cuttlefish/common/libs/utils/files.h"

namespace cuttlefish {
namespace {

std::string GenerateUuid() {
  uuid_t uuid;
  uuid_generate_random(uuid);
  std::string uuid_str = "xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx";
  uuid_unparse(uuid, uuid_str.data());
  return uuid_str;
}

}  // namespace

std::string GenerateSessionId() { return GenerateUuid(); }

Result<std::string> ReadSessionId(const std::string& id_filepath) {
  return CF_EXPECT(ReadFileContents(id_filepath));
}

Result<void> WriteSessionId(const std::string& id_filepath,
                            std::string_view session_id) {
  CF_EXPECTF(!FileExists(id_filepath), "Session id file already exists: {}",
             id_filepath);
  SharedFD session_file_fd = SharedFD::Open(id_filepath, O_CREAT | O_WRONLY);
  CF_EXPECTF(session_file_fd->IsOpen(),
             "Unable to open session id file for writing.  Filename: {}",
             id_filepath);
  CF_EXPECT(WriteAll(session_file_fd, session_id));
  return {};
}

}  // namespace cuttlefish
