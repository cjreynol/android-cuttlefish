/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <sys/types.h>

#include "common/libs/utils/result.h"
#include "host/commands/cvd/selector/instance_database.h"
#include "host/commands/cvd/selector/instance_database_types.h"
#include "host/commands/cvd/selector/selector_common_parser.h"
#include "host/commands/cvd/types.h"

namespace cuttlefish {
namespace selector {

class GroupSelector {
 public:
  static Result<GroupSelector> GetSelector(
      const SelectorOptions& selector_options, const Queries& extra_queries,
      const cvd_common::Envs& envs);
  /*
   * If default, try running single instance group. If multiple, try to find
   * HOME == SystemWideUserHome. If not exists, give up.
   *
   * If group given, find group, and check if all instance names are included
   *
   * If group not given, not yet supported. Will be in next CLs
   */
  Result<LocalInstanceGroup> FindGroup(
      const InstanceDatabase& instance_database);

 private:
  GroupSelector(const Queries& queries) : queries_{queries} {}

  Result<LocalInstanceGroup> FindDefaultGroup(
      const InstanceDatabase& instance_database);

  const Queries queries_;
};

}  // namespace selector
}  // namespace cuttlefish
