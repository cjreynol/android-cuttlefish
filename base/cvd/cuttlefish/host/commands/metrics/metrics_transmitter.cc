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

#include <string>
#include <vector>

#include <android-base/logging.h>

#include "cuttlefish/common/libs/utils/flag_parser.h"
#include "cuttlefish/common/libs/utils/result.h"
#include "cuttlefish/host/libs/web/http_client/curl_global_init.h"
#include "cuttlefish/host/libs/web/http_client/curl_http_client.h"
#include "cuttlefish/host/libs/web/http_client/http_client.h"
#include "cuttlefish/host/libs/web/http_client/http_string.h"
#include "external_proto/clientanalytics.pb.h"
#include "external_proto/log_source_enum.pb.h"

namespace cuttlefish {
namespace {

using wireless_android_play_playlog::ClientInfo;
using wireless_android_play_playlog::LogEvent;
using wireless_android_play_playlog::LogRequest;
using wireless_android_play_playlog::LogSourceEnum::LogSource;

static constexpr LogSource kLogSourceId = LogSource::CUTTLEFISH_METRICS;
static constexpr char kLogSourceStr[] = "CUTTLEFISH_METRICS";
static constexpr ClientInfo::ClientType kCppClientType = ClientInfo::CPLUSPLUS;

enum class ClearcutEnvironment {
  Local,
  Staging,
  Prod,
};

std::string EnvironmentToString(ClearcutEnvironment environment) {
  switch (environment) {
    case ClearcutEnvironment::Local:
      return "local";
    case ClearcutEnvironment::Staging:
      return "staging";
    case ClearcutEnvironment::Prod:
      return "prod";
  }
}

struct MetricsFlags {
  ClearcutEnvironment environment = ClearcutEnvironment::Prod;
  std::string serialized_proto;
};

ClearcutEnvironment GflagsCompatFlag(const std::string& name,
                                     ClearcutEnvironment& value) {
  return GflagsCompatFlag(name)
      .Getter([&value]() { return EnvironmentToString(value); })
      .Setter([name, &value](const FlagMatch& match) {
        if (match.value == "local") {
          value = ClearcutEnvironment::Local;
        } else if (match.value == "staging") {
          value = ClearcutEnvironment::Staging;
        } else if (match.value == "prod") {
          value = ClearcutEnvironment::Prod;
        } else {
          CF_ERRF("Unexpected environment value: \"{}\"", match.value);
        }
        return {};
      });
}

std::string ClearcutEnvironmentUrl(const ClearcutEnvironment environment) {
  switch (environment) {
    case ClearcutEnvironment::Local:
      return "http://localhost:27910/log";
    case ClearcutEnvironment::Staging:
      return "https://play.googleapis.com:443/staging/log";
    case ClearcutEnvironment::Prod:
      return "https://play.googleapis.com:443/log";
  }
}

Result<void> PostRequest(HttpClient& http_client, const std::string& output,
                         const ClearcutEnvironment server) {
  const std::string clearcut_url = ClearcutEnvironmentUrl(server);
  HttpResponse<std::string> response =
      CF_EXPECT(HttpPostToString(http_client, clearcut_url, output));
  CF_EXPECTF(response.HttpSuccess(), "Metrics POST failed ({}): {}",
             response.http_code, response.data);
  return {};
}

Result<void> TransmitMetricsEvent(
    const wireless_android_play_playlog::LogRequest& log_request,
    ClearcutEnvironment environment) {
  CurlGlobalInit curl_global_init;
  const bool use_logging_debug_function = true;
  std::unique_ptr<HttpClient> http_client =
      CurlHttpClient(use_logging_debug_function);
  CF_EXPECT(http_client.get() != nullptr,
            "Unable to create cURL client for metrics transmission");
  CF_EXPECT(
      PostRequest(*http_client, log_request.SerializeAsString(), environment));
  return {};
}

LogRequest BuildLogRequest(std::chrono::milliseconds now,
                           const std::string& cf_log_event) {
  LogRequest log_request;
  log_request.set_request_time_ms(now.count());
  log_request.set_log_source(kLogSourceId);
  log_request.set_log_source_name(kLogSourceStr);

  ClientInfo& client_info = *log_request.mutable_client_info();
  client_info.set_client_type(kCppClientType);

  LogEvent& log_event = *log_request.add_log_event();
  log_event.set_event_time_ms(now.count());
  log_event.set_source_extension(cf_log_event);
  return log_request;
}

Flags ProcessFlags(int argc, char** argv) {
  Flags result;
  std::vector<Flag> flags;
  flags.emplace_back(
      GflagsCompatFlag("environment", result.environment).Help(""));
  flags.emplace_back(
      GflagsCompatFlag("serialized_proto", result.serialized_proto).Help(""));
  flags.emplace_back(HelpFlag(flags));
  flags.emplace_back(UnexpectedArgumentGuard());
  std::vector<std::string> args =
      ArgsToVec(argc - 1, argv + 1);  // Skip argv[0]
  auto parse_result = ConsumeFlags(flags, args);
  CHECK(parse_result.ok()) << "Could not process command line flags.";
  return result;
}

int MetricsMain(const MetricsFlags& flags) {
  //  TODO CJR: should I pass this time value in?
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch());
  const LogRequest log_request =
      BuildLogRequest(now, flags.serialized_proto) Result<void>
          transmit_result =
              TransmitMetricsEvent(log_request, flags.environment);
  if (transmit_result.ok()) {
    return EXIT_SUCCESS;
  } else {
    // TODO CJR: log something
    return EXIT_FAILURE;
  }
}

}  // namespace
}  // namespace cuttlefish

int main(int argc, char** argv) {
  ::android::base::InitLogging(argv, android::base::StderrLogger);
  const MetricsFlags flags = cuttlefish::ProcessFlags(argc, argv);
  // TODO CJR: consider adding filepath flag for the new metrics/metrics.log and
  // updating logger if it exists
  return cuttlefish::MetricsMain(flags);
}
