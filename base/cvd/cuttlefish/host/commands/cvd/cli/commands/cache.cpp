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

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

#include "common/libs/utils/result.h"
#include "host/commands/cvd/cli/commands/server_handler.h"
#include "host/commands/cvd/cli/types.h"
#include "host/commands/cvd/utils/common.h"

namespace cuttlefish {

namespace {
constexpr char kSummaryHelpText[] = "Used to manage the files cached by cvd";

// TODO CJR: pick something other than subcommand, since `CommandRequest`
// already uses that terminology for the `cvd` commands like `cache`
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

enum class CacheSubcommand { Where, Size, Cleanup, Empty };

std::string GetCacheDirectory() {
  const std::string cache_base_path = PerUserDir() + "/cache";
  return cache_base_path;
}

Result<CacheSubcommand> ToCacheSubcommand(const std::string& key) {
  // TODO CJR: find the example where we did something like this in the config
  // file validation
  //    maybe we declared it static?  Or something with leaving it unreferenced
  //    to be cleaned up at program end?
  std::unordered_map<std::string, CacheSubcommand> string_mapping{
      {"where", CacheSubcommand::Where},
      {"size", CacheSubcommand::Size},
      {"cleanup", CacheSubcommand::Cleanup},
      {"empty", CacheSubcommand::Empty},
  };
  auto lookup = string_mapping.find(key);
  CF_EXPECT(lookup != string_mapping.end(), "Unable to find subcommand");
  return lookup->second;
}

Result<void> RunWhere(std::string_view cache_directory) {
  LOG(INFO) << fmt::format("Cache located at: {}", cache_directory);
  return {};
}

Result<void> RunSize(std::string_view cache_directory) {
  // TODO CJR:  rebase after merging new helpers
  // long cache_size = GetDiskUsageGigabytes(cache_directory);
  long cache_size = 1;
  LOG(INFO) << fmt::format("Cache size at {} is approximately {}",
                           cache_directory, cache_size);
  return {};
}

Result<void> RunCleanup() {
  LOG(INFO) << "placeholder cleanup";
  return {};
}

Result<void> RunEmpty() {
  LOG(INFO) << "placeholder empty";
  return {};
}

class CvdCacheCommandHandler : public CvdServerHandler {
 public:
  Result<void> Handle(const CommandRequest& request) override;
  cvd_common::Args CmdList() const override { return {"cache"}; }
  Result<std::string> SummaryHelp() const override;
  bool ShouldInterceptHelp() const override { return true; }
  Result<std::string> DetailedHelp(std::vector<std::string>&) const override;
};

// TODO(chadreynolds): implement handling
Result<void> CvdCacheCommandHandler::Handle(const CommandRequest& request) {
  CF_EXPECT(CanHandle(request));
  std::vector<std::string> cache_arguments = request.SubcommandArguments();
  CF_EXPECT(cache_arguments.empty() == false,
            "cvd cache requires at least a subcommand argument.  Run `cvd help "
            "cache` for details.");

  std::string subcommand = cache_arguments.front();
  cache_arguments.erase(cache_arguments.begin());
  LOG(INFO) << fmt::format("\n\nTODO CJR\n\ninvocation:\n{} {} {}",
                           request.Subcommand(), subcommand,
                           fmt::join(cache_arguments, " "));
  CacheSubcommand cache_subcommand =
      CF_EXPECTF(ToCacheSubcommand(subcommand),
                 "Provided \"{}\" is not a valid cache subcommand", subcommand);
  std::string cache_directory = GetCacheDirectory();
  switch (cache_subcommand) {
    case CacheSubcommand::Where:
      RunWhere(cache_directory);
      break;
    case CacheSubcommand::Size:
      RunSize(cache_directory);
      break;
    case CacheSubcommand::Cleanup:
      // TODO CJR: cache_directory and allowed_size flag (defaulted or not)
      RunCleanup();
      break;
    case CacheSubcommand::Empty:
      // TODO CJR: cache_directory
      RunEmpty();
      break;
  }

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