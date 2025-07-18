/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include <iostream>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <android-base/logging.h>
#include <android-base/no_destructor.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include "cuttlefish/common/libs/utils/flag_parser.h"
#include "cuttlefish/common/libs/utils/result.h"
#include "cuttlefish/host/libs/command_util/runner/run_cvd.pb.h"
#include "cuttlefish/host/libs/command_util/util.h"
#include "cuttlefish/host/libs/config/cuttlefish_config.h"
#include "cuttlefish/host/libs/config/display.h"
#include "cuttlefish/host/libs/vm_manager/crosvm_display_controller.h"

namespace cuttlefish {
namespace {

static const char kAddUsage[] =
    R"(

Adds and connects a display to the given virtual device.

usage: cvd display add \\
        --display=width=1280,height=800 \\
        --display=width=1920,height=1080,refresh_rate_hz=60
)";

static const char kListUsage[] =
    R"(

Lists all of the displays currently connected to a given virtual device.

usage: cvd display list
)";

static const char kRemoveUsage[] =
    R"(

Disconnects and removes displays from the given virtual device.

usage: cvd display remove \\
        --display=<display id> \\
        --display=<display id> ...
)";

static const char kScreenshotUsage[] =
    R"(
Screenshots the contents of a given display.

Currently supported output formats: jpg, png, webp.

usage: cvd display screenshot <display id> <screenshot path>
)";

Result<int> GetInstanceNum(std::vector<std::string>& args) {
  int instance_num = 1;
  CF_EXPECT(
      ConsumeFlags({GflagsCompatFlag("instance_num", instance_num)}, args));
  return instance_num;
}

Result<int> DoHelp(std::vector<std::string>& args) {
  static const android::base::NoDestructor<
      std::unordered_map<std::string, std::string>>
      kSubCommandUsages({
          {"add", kAddUsage},
          {"list", kListUsage},
          {"remove", kRemoveUsage},
          {"screenshot", kScreenshotUsage},
      });

  const std::string& subcommand_str = args[0];
  auto subcommand_usage = kSubCommandUsages->find(subcommand_str);
  if (subcommand_usage == kSubCommandUsages->end()) {
    std::cerr << "Unknown subcommand '" << subcommand_str
              << "'. See `cvd display help`" << std::endl;
    return 1;
  }

  std::cout << subcommand_usage->second << std::endl;
  return 0;
}

Result<int> DoAdd(std::vector<std::string>& args) {
  const int instance_num = CF_EXPECT(GetInstanceNum(args));

  const auto display_configs = CF_EXPECT(ParseDisplayConfigsFromArgs(args));
  if (display_configs.empty()) {
    std::cerr << "Must provide at least 1 display to add. Usage:" << std::endl;
    std::cerr << kAddUsage << std::endl;
    return 1;
  }

  auto crosvm_display = CF_EXPECT(vm_manager::GetCrosvmDisplayController());
  return CF_EXPECT(crosvm_display.Add(instance_num, display_configs));
}

Result<int> DoList(std::vector<std::string>& args) {
  const int instance_num = CF_EXPECT(GetInstanceNum(args));
  auto crosvm_display = CF_EXPECT(vm_manager::GetCrosvmDisplayController());

  auto out = CF_EXPECT(crosvm_display.List(instance_num));
  std::cout << out << std::endl;

  return 0;
}

Result<int> DoRemove(std::vector<std::string>& args) {
  const int instance_num = CF_EXPECT(GetInstanceNum(args));

  std::vector<std::string> displays;
  const std::vector<Flag> remove_displays_flags = {
      GflagsCompatFlag(kDisplayFlag)
          .Help("Display id of a display to remove.")
          .Setter([&](const FlagMatch& match) -> Result<void> {
            displays.push_back(match.value);
            return {};
          }),
  };
  auto parse_res = ConsumeFlags(remove_displays_flags, args);
  if (!parse_res.ok()) {
    std::cerr << parse_res.error().FormatForEnv() << std::endl;
    std::cerr << "Failed to parse flags. Usage:" << std::endl;
    std::cerr << kRemoveUsage << std::endl;
    return 1;
  }

  if (displays.empty()) {
    std::cerr << "Must specify at least one display id to remove. Usage:"
              << std::endl;
    std::cerr << kRemoveUsage << std::endl;
    return 1;
  }

  auto crosvm_display = CF_EXPECT(vm_manager::GetCrosvmDisplayController());
  return CF_EXPECT(crosvm_display.Remove(instance_num, displays));
}

Result<int> DoScreenshot(std::vector<std::string>& args) {
  const int instance_num = CF_EXPECT(GetInstanceNum(args));

  auto config = cuttlefish::CuttlefishConfig::Get();
  if (!config) {
    return CF_ERR("Failed to get Cuttlefish config.");
  }

  int display_number = 0;
  std::string screenshot_path;

  std::vector<std::string> displays;
  const std::vector<Flag> screenshot_flags = {
      GflagsCompatFlag(kDisplayFlag, display_number)
          .Help("Display id of a display to screenshot."),
      GflagsCompatFlag("screenshot_path", screenshot_path)
          .Help("Path for the resulting screenshot file."),
  };
  auto parse_res = ConsumeFlags(screenshot_flags, args);
  if (!parse_res.ok()) {
    std::cerr << parse_res.error().FormatForEnv() << std::endl;
    std::cerr << "Failed to parse flags. Usage:" << std::endl;
    std::cerr << kScreenshotUsage << std::endl;
    return 1;
  }
  CF_EXPECT(!screenshot_path.empty(),
            "Must provide --screenshot_path. Usage:" << kScreenshotUsage);

  run_cvd::ExtendedLauncherAction extended_action;
  extended_action.mutable_screenshot_display()->set_display_number(
      display_number);
  extended_action.mutable_screenshot_display()->set_screenshot_path(
      screenshot_path);

  std::cout << "Requesting to save screenshot for display " << display_number
            << " to " << screenshot_path << "." << std::endl;

  auto socket = CF_EXPECT(
      GetLauncherMonitor(*config, instance_num, /*timeout_seconds=*/5));
  CF_EXPECT(RunLauncherAction(socket, extended_action, std::nullopt),
            "Failed to get success response from launcher.");
  return 0;
}

using DisplaySubCommand = Result<int> (*)(std::vector<std::string>&);

int DisplayMain(int argc, char** argv) {
  ::android::base::InitLogging(argv, android::base::StderrLogger);

  const std::unordered_map<std::string, DisplaySubCommand> kSubCommands = {
      {"add", DoAdd},                //
      {"list", DoList},              //
      {"help", DoHelp},              //
      {"remove", DoRemove},          //
      {"screenshot", DoScreenshot},  //
  };

  auto args = ArgsToVec(argc - 1, argv + 1);
  if (args.empty()) {
    args.push_back("help");
  }

  const std::string command_str = args[0];
  args.erase(args.begin());

  auto command_func_it = kSubCommands.find(command_str);
  if (command_func_it == kSubCommands.end()) {
    std::cerr << "Unknown display command: '" << command_str << "'."
              << std::endl;
    return 1;
  }

  auto result = command_func_it->second(args);
  if (!result.ok()) {
    std::cerr << result.error().FormatForEnv();
    return 1;
  }
  return result.value();
}

}  // namespace
}  // namespace cuttlefish

int main(int argc, char** argv) { return cuttlefish::DisplayMain(argc, argv); }
