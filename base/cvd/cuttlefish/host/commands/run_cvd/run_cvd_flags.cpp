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

#include "host/commands/run_cvd/run_cvd_flags.h"

#include <gflags/gflags.h>

#include "host/commands/assemble_cvd/flags_defaults.h"

DEFINE_int32(reboot_notification_fd, CF_DEFAULTS_REBOOT_NOTIFICATION_FD,
             "A file descriptor to notify when boot completes.");
