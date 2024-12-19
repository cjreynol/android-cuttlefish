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

#include "common/libs/utils/files.h"
#include "common/libs/utils/flag_parser.h"
#include "common/libs/utils/result.h"
#include "host/commands/cvd/cli/commands/server_handler.h"
#include "host/commands/cvd/cli/types.h"
#include "host/commands/cvd/utils/common.h"

namespace cuttlefish {

namespace {
constexpr int kDefaultCacheSizeGigabytes = 25;

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

constexpr char kSummaryHelpText[] = "Used to manage the files cached by cvd";

enum class CacheSubcommand { Where, Size, Cleanup, Empty };

struct CacheArguments {
  CacheSubcommand subcommand = CacheSubcommand::Where;
  int allowed_size_GB = kDefaultCacheSizeGigabytes;
};

std::string GetCacheDirectory() {
  const std::string cache_base_path = PerUserDir() + "/cache";
  return cache_base_path;
}

Result<CacheSubcommand> ToCacheSubcommand(const std::string& key) {
  const std::unordered_map<std::string, CacheSubcommand> kString_mapping{
      {"where", CacheSubcommand::Where},
      {"size", CacheSubcommand::Size},
      {"cleanup", CacheSubcommand::Cleanup},
      {"empty", CacheSubcommand::Empty},
  };
  auto lookup = kString_mapping.find(key);
  CF_EXPECT(lookup != kString_mapping.end(), "Unable to find subcommand");
  return lookup->second;
}

Result<CacheArguments> ProcessArguments(
    const std::vector<std::string>& subcommand_arguments) {
  CF_EXPECT(subcommand_arguments.empty() == false,
            "cvd cache requires at least a subcommand argument.  Run `cvd help "
            "cache` for details.");
  std::vector<std::string> cache_arguments = subcommand_arguments;
  std::string subcommand = cache_arguments.front();
  cache_arguments.erase(cache_arguments.begin());
  CacheArguments result{
      .subcommand = CF_EXPECTF(
          ToCacheSubcommand(subcommand),
          "Provided \"{}\" is not a valid cache subcommand", subcommand),
  };

  std::vector<Flag> flags;
  flags.emplace_back(GflagsCompatFlag("allowed_size_GB", result.allowed_size_GB)
                         .Help("Allowed size of the cache during cleanup "
                               "operation, in gigabytes."));
  flags.emplace_back(UnexpectedArgumentGuard());
  CF_EXPECT(ConsumeFlags(flags, cache_arguments));
  CF_EXPECTF(ConsumeFlags(flags, cache_arguments),
             "Failure processing arguments and flags: cvd {} {}", subcommand,
             fmt::join(cache_arguments, " "));

  return result;
}

void RunWhere(std::string_view cache_directory) {
  LOG(INFO) << fmt::format("Cache located at: {}", cache_directory);
}

Result<void> RunSize(const std::string& cache_directory) {
  // TODO CJR:  after merging new helpers
  // long cache_size = CF_EXPECT(GetDiskUsageGigabytes(cache_directory));
  // this version overflows
  int cache_size = GetDiskUsage(cache_directory);
  LOG(INFO) << fmt::format("Cache size at {} is {}", cache_directory,
                           cache_size);
  return {};
}

Result<void> RunCleanup(const std::string& cache_directory,
                        const int allowed_size_GB) {
  // TODO CJR:  after merging new helpers
  // long cache_size = CF_EXPECT(GetDiskUsageGigabytes(cache_directory));
  long cache_size = GetDiskUsage(cache_directory) / 1024 / 1024 / 1024;
  if (cache_size <= allowed_size_GB) {
    LOG(INFO) << fmt::format(
        "Cache at \"{}\" under max allowed size of {} gigabytes",
        cache_directory, allowed_size_GB);
    return {};
  }
  // TODO CJR cleaning up implementation
  LOG(INFO) << fmt::format(
      "Cache at \"{}\" has been cleaned up to max allowed size of {} gigabytes",
      cache_directory, allowed_size_GB);
  return {};
}

Result<void> RunEmpty(const std::string& cache_directory) {
  CF_EXPECTF(RemoveFile(cache_directory), "Unable to empty the cache at \"{}\"",
             cache_directory);
  CF_EXPECT(EnsureDirectoryExists(cache_directory));
  LOG(INFO) << fmt::format("Cache at \"{}\" has been emptied", cache_directory);
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

Result<void> CvdCacheCommandHandler::Handle(const CommandRequest& request) {
  CF_EXPECT(CanHandle(request));
  CacheArguments arguments =
      CF_EXPECT(ProcessArguments(request.SubcommandArguments()));
  std::string cache_directory = GetCacheDirectory();
  switch (arguments.subcommand) {
    case CacheSubcommand::Where:
      RunWhere(cache_directory);
      break;
    case CacheSubcommand::Size:
      CF_EXPECT(RunSize(cache_directory));
      break;
    case CacheSubcommand::Cleanup:
      CF_EXPECT(RunCleanup(cache_directory, arguments.allowed_size_GB));
      break;
    case CacheSubcommand::Empty:
      CF_EXPECT(RunEmpty(cache_directory), "Failure emptying cache");
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