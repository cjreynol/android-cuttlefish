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

#include "cuttlefish/common/libs/utils/subprocess.h"

#ifdef __linux__
#include <sys/prctl.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <android-base/logging.h>
#include <android-base/strings.h>

#include "cuttlefish/common/libs/utils/contains.h"
#include "cuttlefish/common/libs/utils/files.h"

extern char** environ;

namespace cuttlefish {
namespace {

// If a redirected-to file descriptor was already closed, it's possible that
// some inherited file descriptor duped to this file descriptor and the redirect
// would override that. This function makes sure that doesn't happen.
bool validate_redirects(
    const std::map<Subprocess::StdIOChannel, int>& redirects,
    const std::map<SharedFD, int>& inherited_fds) {
  // Add the redirected IO channels to a set as integers. This allows converting
  // the enum values into integers instead of the other way around.
  std::set<int> int_redirects;
  for (const auto& entry : redirects) {
    int_redirects.insert(static_cast<int>(entry.first));
  }
  for (const auto& entry : inherited_fds) {
    auto dupped_fd = entry.second;
    if (int_redirects.count(dupped_fd)) {
      LOG(ERROR) << "Requested redirect of fd(" << dupped_fd
                 << ") conflicts with inherited FD.";
      return false;
    }
  }
  return true;
}

void do_redirects(const std::map<Subprocess::StdIOChannel, int>& redirects) {
  for (const auto& entry : redirects) {
    auto std_channel = static_cast<int>(entry.first);
    auto fd = entry.second;
    TEMP_FAILURE_RETRY(dup2(fd, std_channel));
  }
}

std::vector<const char*> ToCharPointers(const std::vector<std::string>& vect) {
  std::vector<const char*> ret = {};
  for (const auto& str : vect) {
    ret.push_back(str.c_str());
  }
  ret.push_back(NULL);
  return ret;
}
}  // namespace

std::vector<std::string> ArgsToVec(char** argv) {
  std::vector<std::string> args;
  for (int i = 0; argv && argv[i]; i++) {
    args.push_back(argv[i]);
  }
  return args;
}

std::unordered_map<std::string, std::string> EnvpToMap(char** envp) {
  std::unordered_map<std::string, std::string> env_map;
  if (!envp) {
    return env_map;
  }
  for (char** e = envp; *e != nullptr; e++) {
    std::string env_var_val(*e);
    auto tokens = android::base::Split(env_var_val, "=");
    if (tokens.size() <= 1) {
      LOG(WARNING) << "Environment var in unknown format: " << env_var_val;
      continue;
    }
    const auto var = tokens.at(0);
    tokens.erase(tokens.begin());
    env_map[var] = android::base::Join(tokens, "=");
  }
  return env_map;
}

SubprocessOptions& SubprocessOptions::Verbose(bool verbose) & {
  verbose_ = verbose;
  return *this;
}
SubprocessOptions SubprocessOptions::Verbose(bool verbose) && {
  verbose_ = verbose;
  return std::move(*this);
}

#ifdef __linux__
SubprocessOptions& SubprocessOptions::ExitWithParent(bool exit_with_parent) & {
  exit_with_parent_ = exit_with_parent;
  return *this;
}
SubprocessOptions SubprocessOptions::ExitWithParent(bool exit_with_parent) && {
  exit_with_parent_ = exit_with_parent;
  return std::move(*this);
}
#endif

SubprocessOptions& SubprocessOptions::InGroup(bool in_group) & {
  in_group_ = in_group;
  return *this;
}
SubprocessOptions SubprocessOptions::InGroup(bool in_group) && {
  in_group_ = in_group;
  return std::move(*this);
}

SubprocessOptions& SubprocessOptions::Strace(std::string s) & {
  strace_ = std::move(s);
  return *this;
}
SubprocessOptions SubprocessOptions::Strace(std::string s) && {
  strace_ = std::move(s);
  return std::move(*this);
}

Subprocess::Subprocess(Subprocess&& subprocess)
    : pid_(subprocess.pid_.load()),
      started_(subprocess.started_),
      stopper_(subprocess.stopper_) {
  // Make sure the moved object no longer controls this subprocess
  subprocess.pid_ = -1;
  subprocess.started_ = false;
}

Subprocess& Subprocess::operator=(Subprocess&& other) {
  pid_ = other.pid_.load();
  started_ = other.started_;
  stopper_ = other.stopper_;

  other.pid_ = -1;
  other.started_ = false;
  return *this;
}

int Subprocess::Wait() {
  if (pid_ < 0) {
    LOG(ERROR)
        << "Attempt to wait on invalid pid(has it been waited on already?): "
        << pid_;
    return -1;
  }
  int wstatus = 0;
  auto pid = pid_.load();  // Wait will set pid_ to -1 after waiting
  auto wait_ret = waitpid(pid, &wstatus, 0);
  if (wait_ret < 0) {
    auto error = errno;
    LOG(ERROR) << "Error on call to waitpid: " << strerror(error);
    return wait_ret;
  }
  int retval = 0;
  if (WIFEXITED(wstatus)) {
    pid_ = -1;
    retval = WEXITSTATUS(wstatus);
    if (retval) {
      LOG(DEBUG) << "Subprocess " << pid
                 << " exited with error code: " << retval;
    }
  } else if (WIFSIGNALED(wstatus)) {
    pid_ = -1;
    int sig_num = WTERMSIG(wstatus);
    LOG(ERROR) << "Subprocess " << pid << " was interrupted by a signal '"
               << strsignal(sig_num) << "' (" << sig_num << ")";
    retval = -1;
  }
  return retval;
}
int Subprocess::Wait(siginfo_t* infop, int options) {
  if (pid_ < 0) {
    LOG(ERROR)
        << "Attempt to wait on invalid pid(has it been waited on already?): "
        << pid_;
    return -1;
  }
  *infop = {};
  auto retval = TEMP_FAILURE_RETRY(waitid(P_PID, pid_, infop, options));
  // We don't want to wait twice for the same process
  bool exited = infop->si_code == CLD_EXITED || infop->si_code == CLD_DUMPED;
  bool reaped = !(options & WNOWAIT);
  if (exited && reaped) {
    pid_ = -1;
  }
  return retval;
}

static Result<void> SendSignalImpl(const int signal, const pid_t pid,
                                   bool to_group, const bool started) {
  if (pid == -1) {
    return CF_ERR(strerror(ESRCH));
  }
  CF_EXPECTF(started == true,
             "The Subprocess object lost the ownership"
             "of the process {}.",
             pid);
  int ret_code = 0;
  if (to_group) {
    ret_code = killpg(getpgid(pid), signal);
  } else {
    ret_code = kill(pid, signal);
  }
  CF_EXPECTF(ret_code == 0, "kill/killpg returns {} with errno: {}", ret_code,
             strerror(errno));
  return {};
}

Result<void> Subprocess::SendSignal(const int signal) {
  CF_EXPECT(SendSignalImpl(signal, pid_, /* to_group */ false, started_));
  return {};
}

Result<void> Subprocess::SendSignalToGroup(const int signal) {
  CF_EXPECT(SendSignalImpl(signal, pid_, /* to_group */ true, started_));
  return {};
}

StopperResult KillSubprocess(Subprocess* subprocess) {
  auto pid = subprocess->pid();
  if (pid > 0) {
    auto pgid = getpgid(pid);
    if (pgid < 0) {
      auto error = errno;
      LOG(WARNING) << "Error obtaining process group id of process with pid="
                   << pid << ": " << strerror(error);
      // Send the kill signal anyways, because pgid will be -1 it will be sent
      // to the process and not a (non-existent) group
    }
    bool is_group_head = pid == pgid;
    auto kill_ret = (is_group_head ? killpg : kill)(pid, SIGKILL);
    if (kill_ret == 0) {
      return StopperResult::kStopSuccess;
    }
    auto kill_cmd = is_group_head ? "killpg(" : "kill(";
    PLOG(ERROR) << kill_cmd << pid << ", SIGKILL) failed: ";
    return StopperResult::kStopFailure;
  }
  return StopperResult::kStopSuccess;
}

SubprocessStopper KillSubprocessFallback(std::function<StopperResult()> nice) {
  return KillSubprocessFallback([nice](Subprocess*) { return nice(); });
}

SubprocessStopper KillSubprocessFallback(SubprocessStopper nice_stopper) {
  return [nice_stopper](Subprocess* process) {
    auto nice_result = nice_stopper(process);
    if (nice_result == StopperResult::kStopFailure) {
      auto harsh_result = KillSubprocess(process);
      return harsh_result == StopperResult::kStopSuccess
                 ? StopperResult::kStopCrash
                 : harsh_result;
    }
    return nice_result;
  };
}

Command::Command(std::string executable, SubprocessStopper stopper)
    : subprocess_stopper_(stopper) {
  for (char** env = environ; *env; env++) {
    env_.emplace_back(*env);
  }
  command_.emplace_back(std::move(executable));
}

Command::~Command() {
  // Close all inherited file descriptors
  for (const auto& entry : inherited_fds_) {
    close(entry.second);
  }
  // Close all redirected file descriptors
  for (const auto& entry : redirects_) {
    close(entry.second);
  }
}

void Command::BuildParameter(std::stringstream* stream, SharedFD shared_fd) {
  int fd;
  if (inherited_fds_.count(shared_fd)) {
    fd = inherited_fds_[shared_fd];
  } else {
    fd = shared_fd->Fcntl(F_DUPFD_CLOEXEC, 3);
    CHECK(fd >= 0) << "Could not acquire a new file descriptor: "
                   << shared_fd->StrError();
    inherited_fds_[shared_fd] = fd;
  }
  *stream << fd;
}

Command& Command::RedirectStdIO(Subprocess::StdIOChannel channel,
                                SharedFD shared_fd) & {
  CHECK(shared_fd->IsOpen());
  CHECK(redirects_.count(channel) == 0)
      << "Attempted multiple redirections of fd: " << static_cast<int>(channel);
  auto dup_fd = shared_fd->Fcntl(F_DUPFD_CLOEXEC, 3);
  CHECK(dup_fd >= 0) << "Could not acquire a new file descriptor: "
                     << shared_fd->StrError();
  redirects_[channel] = dup_fd;
  return *this;
}
Command Command::RedirectStdIO(Subprocess::StdIOChannel channel,
                               SharedFD shared_fd) && {
  RedirectStdIO(channel, shared_fd);
  return std::move(*this);
}
Command& Command::RedirectStdIO(Subprocess::StdIOChannel subprocess_channel,
                                Subprocess::StdIOChannel parent_channel) & {
  return RedirectStdIO(subprocess_channel,
                       SharedFD::Dup(static_cast<int>(parent_channel)));
}
Command Command::RedirectStdIO(Subprocess::StdIOChannel subprocess_channel,
                               Subprocess::StdIOChannel parent_channel) && {
  RedirectStdIO(subprocess_channel, parent_channel);
  return std::move(*this);
}

Command& Command::SetWorkingDirectory(const std::string& path) & {
#ifdef __linux__
  auto fd = SharedFD::Open(path, O_RDONLY | O_PATH | O_DIRECTORY);
#elif defined(__APPLE__)
  auto fd = SharedFD::Open(path, O_RDONLY | O_DIRECTORY);
#else
#error "Unsupported operating system"
#endif
  CHECK(fd->IsOpen()) << "Could not open \"" << path
                      << "\" dir fd: " << fd->StrError();
  return SetWorkingDirectory(fd);
}
Command Command::SetWorkingDirectory(const std::string& path) && {
  return std::move(SetWorkingDirectory(path));
}
Command& Command::SetWorkingDirectory(SharedFD dirfd) & {
  CHECK(dirfd->IsOpen()) << "Dir fd invalid: " << dirfd->StrError();
  working_directory_ = std::move(dirfd);
  return *this;
}
Command Command::SetWorkingDirectory(SharedFD dirfd) && {
  return std::move(SetWorkingDirectory(std::move(dirfd)));
}

Command& Command::AddPrerequisite(
    const std::function<Result<void>()>& prerequisite) & {
  prerequisites_.push_back(prerequisite);
  return *this;
}

Command Command::AddPrerequisite(
    const std::function<Result<void>()>& prerequisite) && {
  prerequisites_.push_back(prerequisite);
  return std::move(*this);
}

Subprocess Command::Start(SubprocessOptions options) const {
  auto cmd = ToCharPointers(command_);

  if (!options.Strace().empty()) {
    auto strace_args = {
        "/usr/bin/strace",
        "--daemonize",
        "--output-separately",  // Add .pid suffix
        "--follow-forks",
        "-o",  // Write to a separate file.
        options.Strace().c_str(),
    };
    cmd.insert(cmd.begin(), strace_args);
  }

  if (!validate_redirects(redirects_, inherited_fds_)) {
    return Subprocess(-1, {});
  }

  for (auto& prerequisite : prerequisites_) {
    auto prerequisiteResult = prerequisite();

    if (!prerequisiteResult.ok()) {
      LOG(ERROR) << "Failed to check prerequisites: "
                 << prerequisiteResult.error().FormatForEnv();
    }
  }

  // ToCharPointers allocates memory so it can't be called in the child process.
  auto envp = ToCharPointers(env_);
  pid_t pid = fork();
  if (!pid) {
    // LOG(...) can't be used in the child process because it may block waiting
    // for other threads which don't exist in the child process.
#ifdef __linux__
    if (options.ExitWithParent()) {
      prctl(PR_SET_PDEATHSIG, SIGHUP); // Die when parent dies
    }
#endif

    do_redirects(redirects_);

    if (options.InGroup()) {
      // This call should never fail (see SETPGID(2))
      if (setpgid(0, 0) != 0) {
        exit(-errno);
      }
    }
    for (const auto& entry : inherited_fds_) {
      if (fcntl(entry.second, F_SETFD, 0)) {
        exit(-errno);
      }
    }
    if (working_directory_->IsOpen()) {
      if (SharedFD::Fchdir(working_directory_) != 0) {
        exit(-errno);
      }
    }
    int rval;
    const char* executable = executable_ ? executable_->c_str() : cmd[0];
#ifdef __linux__
    rval = execvpe(executable, const_cast<char* const*>(cmd.data()),
                   const_cast<char* const*>(envp.data()));
#elif defined(__APPLE__)
    rval = execve(executable, const_cast<char* const*>(cmd.data()),
                  const_cast<char* const*>(envp.data()));
#else
#error "Unsupported architecture"
#endif
    // No need to check for error, execvpe/execve don't return on success.
    exit(rval);
  }
  if (pid == -1) {
    LOG(ERROR) << "fork failed (" << strerror(errno) << ")";
  }
  if (options.Verbose()) { // "more verbose", and LOG(DEBUG) > LOG(VERBOSE)
    LOG(DEBUG) << "Started (pid: " << pid << "): " << cmd[0];
    for (int i = 1; cmd[i]; i++) {
      LOG(DEBUG) << cmd[i];
    }
  } else {
    LOG(VERBOSE) << "Started (pid: " << pid << "): " << cmd[0];
    for (int i = 1; cmd[i]; i++) {
      LOG(VERBOSE) << cmd[i];
    }
  }
  return Subprocess(pid, subprocess_stopper_);
}

std::ostream& operator<<(std::ostream& out, const Command& command) {
  std::unordered_set<std::string> to_show{"HOME",
                                          "ANDROID_HOST_OUT",
                                          "ANDROID_SOONG_HOST_OUT",
                                          "ANDROID_PRODUCT_OUT",
                                          "CUTTLEFISH_CONFIG_FILE",
                                          "CUTTLEFISH_INSTANCE"};
  for (const std::string& env_var : command.env_) {
    std::vector<std::string> env_split = android::base::Split(env_var, "=");
    if (!env_split.empty() && Contains(to_show, env_split.front())) {
      out << env_var << " ";
    }
  }
  return out << android::base::Join(command.command_, " ");
}

std::string Command::ToString() const {
  std::stringstream ss;
  if (!env_.empty()) {
    ss << android::base::Join(env_, " ");
    ss << " ";
  }
  ss << android::base::Join(command_, " ");
  return ss.str();
}

std::string Command::AsBashScript(
    const std::string& redirected_stdio_path) const {
  CHECK(inherited_fds_.empty())
      << "Bash wrapper will not have inheritied file descriptors.";
  CHECK(redirects_.empty()) << "Bash wrapper will not have redirected stdio.";

  std::string contents =
      "#!/usr/bin/env bash\n\n" + android::base::Join(command_, " \\\n");
  if (!redirected_stdio_path.empty()) {
    contents += " &> " + AbsolutePath(redirected_stdio_path);
  }
  return contents;
}

namespace {

struct ExtraParam {
  // option for Subprocess::Start()
  SubprocessOptions subprocess_options;
  // options for Subprocess::Wait(...)
  int wait_options;
  siginfo_t* infop;
};
Result<int> ExecuteImpl(const std::vector<std::string>& command,
                        const std::optional<std::vector<std::string>>& envs,
                        std::optional<ExtraParam> extra_param) {
  Command cmd(command[0]);
  for (size_t i = 1; i < command.size(); ++i) {
    cmd.AddParameter(command[i]);
  }
  if (envs) {
    cmd.SetEnvironment(*envs);
  }
  auto subprocess =
      (!extra_param ? cmd.Start()
                    : cmd.Start(std::move(extra_param->subprocess_options)));
  CF_EXPECT(subprocess.Started(), "Subprocess failed to start.");

  if (extra_param) {
    CF_EXPECT(extra_param->infop != nullptr,
              "When ExtraParam is given, the infop buffer address "
                  << "must not be nullptr.");
    return subprocess.Wait(extra_param->infop, extra_param->wait_options);
  } else {
    return subprocess.Wait();
  }
}

}  // namespace

int Execute(const std::vector<std::string>& commands,
            const std::vector<std::string>& envs) {
  auto result = ExecuteImpl(commands, envs, /* extra_param */ std::nullopt);
  return (!result.ok() ? -1 : *result);
}

int Execute(const std::vector<std::string>& commands) {
  std::vector<std::string> envs;
  auto result = ExecuteImpl(commands, /* envs */ std::nullopt,
                            /* extra_param */ std::nullopt);
  return (!result.ok() ? -1 : *result);
}

Result<siginfo_t> Execute(const std::vector<std::string>& commands,
                          SubprocessOptions subprocess_options,
                          int wait_options) {
  siginfo_t info;
  auto ret_code = CF_EXPECT(ExecuteImpl(
      commands, /* envs */ std::nullopt,
      ExtraParam{.subprocess_options = std::move(subprocess_options),
                 .wait_options = wait_options,
                 .infop = &info}));
  CF_EXPECT(ret_code == 0, "Subprocess::Wait() returned " << ret_code);
  return info;
}

Result<siginfo_t> Execute(const std::vector<std::string>& commands,
                          const std::vector<std::string>& envs,
                          SubprocessOptions subprocess_options,
                          int wait_options) {
  siginfo_t info;
  auto ret_code = CF_EXPECT(ExecuteImpl(
      commands, envs,
      ExtraParam{.subprocess_options = std::move(subprocess_options),
                 .wait_options = wait_options,
                 .infop = &info}));
  CF_EXPECT(ret_code == 0, "Subprocess::Wait() returned " << ret_code);
  return info;
}

}  // namespace cuttlefish
