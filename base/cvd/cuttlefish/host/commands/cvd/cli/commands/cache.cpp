/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include "host/commands/cvd/cli/commands/cache.h"

#include "common/libs/utils/result.h"
#include "host/commands/cvd/cli/commands/server_handler.h"
#include "host/commands/cvd/cli/types.h"

namespace cuttlefish {

namespace {
constexpr char kSummaryHelpText[] = "Used to manage the files cached by cvd";

constexpr char kDetailedHelpText[] =
    R"(usage: cvd cache <subcommand> 
* Note: currently only manage the files cached by cvd fetch invocations where `--enable_caching=true` (the default)

Example usage:
    cvd cache where - display the filepath of the cache
    cvd cache size - display the size of the cache
    cvd cache cleanup - caps the cache at the default size
    cvd cache cleanup --allowed_size_GB=<n> - caps the cache at the given size
    cvd cache empty - wipes out all files in the cache directory
)";

class CvdCacheCommandHandler : public CvdServerHandler {
 public:
  Result<void> Handle(const CommandRequest& request) override;
  cvd_common::Args CmdList() const override { return {"cache"}; }
  Result<std::string> SummaryHelp() const override;
  bool ShouldInterceptHelp() const override { return true; }
  Result<std::string> DetailedHelp(std::vector<std::string>&) const override;
};

Result<void> CvdCacheCommandHandler::Handle(const CommandRequest& request) {
  CF_EXPECT(CanHandle(request));
  // TODO(chadreynolds): implement handling
  LOG(INFO) << "cache command invoked";
  return {};
}

Result<std::string> CvdCacheCommandHandler::SummaryHelp() const {
  return kSummaryHelpText;
}

Result<std::string> CvdCacheCommandHandler::DetailedHelp(
    std::vector<std::string>&) const {
  return kDetailedHelpText;
}

}  // namespace

std::unique_ptr<CvdServerHandler> NewCvdCacheCommandHandler() {
  return std::unique_ptr<CvdServerHandler>(new CvdCacheCommandHandler());
}

}  // namespace cuttlefish