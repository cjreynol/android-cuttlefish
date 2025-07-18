//
// Copyright (C) 2022 The Android Open Source Project
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

#include <chrono>

#include "cuttlefish/common/libs/fs/shared_fd.h"
#include "cuttlefish/common/libs/utils/result.h"

namespace cuttlefish {
namespace socket_proxy {

class Server {
 public:
  virtual Result<SharedFD> Start() = 0;
  virtual std::string Describe() const = 0;
  virtual ~Server() = default;
};

class TcpServer : public Server {
 public:
  TcpServer(int port, int retries_count = 1,
            std::chrono::milliseconds retries_delay = std::chrono::milliseconds(0));
  Result<SharedFD> Start() override;
  std::string Describe() const override;

 private:
  int port_;
  int retries_count_;
  std::chrono::milliseconds retries_delay_;
};

class VsockServer : public Server {
 public:
  VsockServer(int port, std::optional<int> vhost_user_vsock_cid);
  Result<SharedFD> Start() override;
  std::string Describe() const override;

 private:
  int port_;
  std::optional<int> vhost_user_vsock_cid_;
};

class DupServer : public Server {
 public:
  DupServer(int fd);
  Result<SharedFD> Start() override;
  std::string Describe() const override;

 private:
  int fd_;
  SharedFD sfd_;
};

}
}
