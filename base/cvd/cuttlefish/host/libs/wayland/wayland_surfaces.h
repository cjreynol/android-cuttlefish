/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <stdint.h>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>

#include "cuttlefish/host/libs/wayland/wayland_server_callbacks.h"

namespace wayland {

class Surface;

class Surfaces {
 public:
  Surfaces() = default;
  virtual ~Surfaces() = default;

  Surfaces(const Surfaces& rhs) = delete;
  Surfaces& operator=(const Surfaces& rhs) = delete;

  Surfaces(Surfaces&& rhs) = delete;
  Surfaces& operator=(Surfaces&& rhs) = delete;

  using FrameCallback =
      std::function<void(std::uint32_t /*display_number*/,       //
                         std::uint32_t /*frame_width*/,          //
                         std::uint32_t /*frame_height*/,         //
                         std::uint32_t /*frame_fourcc_format*/,  //
                         std::uint32_t /*frame_stride_bytes*/,   //
                         std::uint8_t* /*frame_bytes*/)>;

  void SetFrameCallback(FrameCallback callback);

  void SetDisplayEventCallback(DisplayEventCallback callback);

  void SetFramesAreRGBA(bool frames_are_rgba);

 private:
  friend class Surface;
  void HandleSurfaceFrame(std::uint32_t display_number,       //
                          std::uint32_t frame_width,          //
                          std::uint32_t frame_height,         //
                          std::uint32_t frame_fourcc_format,  //
                          std::uint32_t frame_stride_bytes,   //
                          std::uint8_t* frame_bytes);

  void HandleSurfaceCreated(std::uint32_t display_number,
                            std::uint32_t display_width,
                            std::uint32_t display_height);

  void HandleSurfaceDestroyed(std::uint32_t display_number);

  std::mutex callback_mutex_;
  std::optional<FrameCallback> callback_;
  std::optional<DisplayEventCallback> event_callback_;
  // If true, report that the received frames are RGBA regardless
  // of the format reported by the wayland client.
  bool frames_are_rgba_ = false;
};

}  // namespace wayland
