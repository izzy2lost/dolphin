// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/OnScreenDisplay.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <string>

#include <fmt/format.h>
#include <imgui.h>

#include "VideoCommon/VideoConfig.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Timer.h"

#ifdef _UWP
#include "DolphinUWP/Host.h"
#include "DolphinUWP/UWPUtils.h"
#endif

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/ConfigManager.h"

namespace OSD
{
constexpr float LEFT_MARGIN = 10.0f;         // Pixels to the left of OSD messages.
constexpr float TOP_MARGIN = 10.0f;          // Pixels above the first OSD message.
constexpr float WINDOW_PADDING = 4.0f;       // Pixels between subsequent OSD messages.
constexpr float MESSAGE_FADE_TIME = 1000.f;  // Ms to fade OSD messages at the end of their life.
constexpr float MESSAGE_DROP_TIME = 5000.f;  // Ms to drop OSD messages that has yet to ever render.

static std::atomic<int> s_obscured_pixels_left = 0;
static std::atomic<int> s_obscured_pixels_top = 0;
static bool s_showing_menu = false;

struct Message
{
  Message() = default;
  Message(std::string text_, u32 duration_, u32 color_)
      : text(std::move(text_)), duration(duration_), color(color_)
  {
    timer.Start();
  }
  s64 TimeRemaining() const { return duration - timer.ElapsedMs(); }
  std::string text;
  Common::Timer timer;
  u32 duration = 0;
  bool ever_drawn = false;
  u32 color = 0;
};
static std::multimap<MessageType, Message> s_messages;
static std::mutex s_messages_mutex;

static ImVec4 ARGBToImVec4(const u32 argb)
{
  return ImVec4(static_cast<float>((argb >> 16) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 0) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 24) & 0xFF) / 255.0f);
}

static float DrawMessage(int index, Message& msg, const ImVec2& position, int time_left)
{
  // We have to provide a window name, and these shouldn't be duplicated.
  // So instead, we generate a name based on the number of messages drawn.
  const std::string window_name = fmt::format("osd_{}", index);

  // The size must be reset, otherwise the length of old messages could influence new ones.
  ImGui::SetNextWindowPos(position);
  ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f));

  // Gradually fade old messages away (except in their first frame)
  const float fade_time = std::max(std::min(MESSAGE_FADE_TIME, (float)msg.duration), 1.f);
  const float alpha = std::clamp(time_left / fade_time, 0.f, 1.f);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, msg.ever_drawn ? alpha : 1.0);

  float window_height = 0.0f;
  if (ImGui::Begin(window_name.c_str(), nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing))
  {
    // Use %s in case message contains %.
    ImGui::TextColored(ARGBToImVec4(msg.color), "%s", msg.text.c_str());
    window_height =
        ImGui::GetWindowSize().y + (WINDOW_PADDING * ImGui::GetIO().DisplayFramebufferScale.y);
  }

  ImGui::End();
  ImGui::PopStyleVar();

  msg.ever_drawn = true;

  return window_height;
}

void AddTypedMessage(MessageType type, std::string message, u32 ms, u32 argb)
{
  std::lock_guard lock{s_messages_mutex};
  s_messages.erase(type);
  s_messages.emplace(type, Message(std::move(message), ms, argb));
}

void AddMessage(std::string message, u32 ms, u32 argb)
{
  std::lock_guard lock{s_messages_mutex};
  s_messages.emplace(MessageType::Typeless, Message(std::move(message), ms, argb));
}

void DrawMessages()
{
  const bool draw_messages = Config::Get(Config::MAIN_OSD_MESSAGES);
  const float current_x =
      LEFT_MARGIN * ImGui::GetIO().DisplayFramebufferScale.x + s_obscured_pixels_left;
  float current_y = TOP_MARGIN * ImGui::GetIO().DisplayFramebufferScale.y + s_obscured_pixels_top;
  int index = 0;

  std::lock_guard lock{s_messages_mutex};

  for (auto it = s_messages.begin(); it != s_messages.end();)
  {
    Message& msg = it->second;
    const s64 time_left = msg.TimeRemaining();

    // Make sure we draw them at least once if they were printed with 0ms,
    // unless enough time has expired, in that case, we drop them
    if (time_left <= 0 && (msg.ever_drawn || -time_left >= MESSAGE_DROP_TIME))
    {
      it = s_messages.erase(it);
      continue;
    }
    else
    {
      ++it;
    }

    if (draw_messages)
      current_y += DrawMessage(index++, msg, ImVec2(current_x, current_y), time_left);
  }
}

void ClearMessages()
{
  std::lock_guard lock{s_messages_mutex};
  s_messages.clear();
}

void SetObscuredPixelsLeft(int width)
{
  s_obscured_pixels_left = width;
}

void SetObscuredPixelsTop(int height)
{
  s_obscured_pixels_top = height;
}

void ToggleMenuVisibility()
{
  s_showing_menu = !s_showing_menu;
}

void DrawMenu()
{
  if (!s_showing_menu)
    return;

  // Update input states
  std::vector<std::unique_ptr<ControllerEmu::Control>>* btns;
  std::vector<std::unique_ptr<ControllerEmu::Control>>* stick;

  if (SConfig::GetInstance().bWii)
  {
    btns = &Wiimote::GetWiimoteGroup(0, WiimoteEmu::WiimoteGroup::Buttons)->controls;
    stick = &Wiimote::GetNunchukGroup(0, WiimoteEmu::NunchukGroup::Stick)->controls;
  }
  else
  {
    btns = &Pad::GetGroup(0, PadGroup::Buttons)->controls;
    stick = &Pad::GetGroup(0, PadGroup::MainStick)->controls;
  }

  const float center_x = ImGui::GetIO().DisplaySize.x * 0.5f;
  const float center_y = ImGui::GetIO().DisplaySize.y * 0.5f;
  const float scale = ImGui::GetIO().DisplayFramebufferScale.x;

  ImGui::SetNextWindowSize(ImVec2(300.0f * scale, 300.0f * scale), ImGuiCond_Always);
  ImGui::SetNextWindowPos(ImVec2(center_x, center_y), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowFocus();

  if (ImGui::Begin("side_menu", nullptr, ImGuiWindowFlags_NoTitleBar))
  {
    bool dualCore = Config::Get(Config::MAIN_CPU_THREAD);
    if (ImGui::Checkbox("Dual Core", &dualCore)) {
      Core::QueueHostJob([dualCore] {
          Config::SetBaseOrCurrent(Config::MAIN_CPU_THREAD, dualCore);
          }, false);
    }

    const char* ir_items[] = {"1x (Native)", "2x (720p)", "3x (1080p)", "4x (1440p)", "5x (2640p)", "6x (4K)"};
    static int ir_idx = Config::Get(Config::GFX_EFB_SCALE);

    if (ImGui::TreeNode("Internal Resolution"))
    {
      for (int i = 0; i < 4; i++)
      {
        ImGui::PushID(i);
        if (ImGui::RadioButton(ir_items[i], &ir_idx))
        {
          Core::QueueHostJob([i] {
            Config::SetBaseOrCurrent(Config::GFX_EFB_SCALE, i);

          });
        }
        ImGui::PopID();
      }

      ImGui::TreePop();
    }

    const char* aspect_items[] = {"Auto", "Force 16:9", "Force 4:3", "Stretch"};
    static int aspect_idx = 0;
    auto aspect = Config::Get(Config::GFX_ASPECT_RATIO);
    switch (aspect)
    {
    case AspectMode::Auto:
      aspect_idx = 0;
      break;
    case AspectMode::AnalogWide:
      aspect_idx = 1;
      break;
    case AspectMode::Analog:
      aspect_idx = 2;
      break;
    case AspectMode::Stretch:
      aspect_idx = 3;
      break;
    }

    if (ImGui::TreeNode("Aspect Ratio"))
    {
      for (int i = 0; i < 4; i++)
      {
        ImGui::PushID(i);
        if (ImGui::RadioButton(aspect_items[i], &aspect_idx))
        {
          Core::QueueHostJob([i] {
            switch (i)
            {
            case 0:
              Config::SetBaseOrCurrent(Config::GFX_ASPECT_RATIO, AspectMode::Auto);
              break;
            case 1:
              Config::SetBaseOrCurrent(Config::GFX_ASPECT_RATIO, AspectMode::AnalogWide);
              break;
            case 2:
              Config::SetBaseOrCurrent(Config::GFX_ASPECT_RATIO, AspectMode::Analog);
              break;
            case 3:
              Config::SetBaseOrCurrent(Config::GFX_ASPECT_RATIO, AspectMode::Stretch);
              break;
            }
          });  
        }
        ImGui::PopID();
      }

      ImGui::TreePop();
    }

    const char* shader_items[] = { "Synchronous", "Hybrid Ubershaders", "Exclusive Ubershaders", "Skip Drawing" };
    static int shader_idx = 0;
    auto shader = Config::Get(Config::GFX_SHADER_COMPILATION_MODE);
    switch (shader)
    {
    case ShaderCompilationMode::Synchronous:
      shader_idx = 0;
      break;
    case ShaderCompilationMode::AsynchronousUberShaders:
      shader_idx = 1;
      break;
    case ShaderCompilationMode::SynchronousUberShaders:
      shader_idx = 2;
      break;
    case ShaderCompilationMode::AsynchronousSkipRendering:
      shader_idx = 3;
      break;
    }

    if (ImGui::TreeNode("Shader Compilation"))
    {
      for (int i = 0; i < 4; i++)
      {
        ImGui::PushID(i);
        if (ImGui::RadioButton(shader_items[i], &shader_idx))
        {
          Core::QueueHostJob([i] {
            switch (i)
            {
            case 0:
              Config::SetBaseOrCurrent(Config::GFX_SHADER_COMPILATION_MODE,
                                       ShaderCompilationMode::Synchronous);
              break;
            case 1:
              Config::SetBaseOrCurrent(Config::GFX_SHADER_COMPILATION_MODE,
                                       ShaderCompilationMode::AsynchronousUberShaders);
              break;
            case 2:
              Config::SetBaseOrCurrent(Config::GFX_SHADER_COMPILATION_MODE,
                                       ShaderCompilationMode::SynchronousUberShaders);
              break;
            case 3:
              Config::SetBaseOrCurrent(Config::GFX_SHADER_COMPILATION_MODE,
                                       ShaderCompilationMode::AsynchronousSkipRendering);
              break;
            }
          });
        }
        ImGui::PopID();
      }

      ImGui::TreePop();
    }

#if _UWP
    if (ImGui::TreeNode("Config Location (Requires Restart)"))
    {
      if (ImGui::Button("Set Dolphin User Folder Location"))
      {
        UWP::SetNewUserLocation();
        s_showing_menu = false;
      }

      if (ImGui::Button("Reset Dolphin User Folder Location"))
      {
        UWP::ResetUserLocation();
      }
      ImGui::TreePop();
    }
#endif

    if (ImGui::Button("Change Disc"))
    {
      UWP::PickDisc();
      s_showing_menu = false;
    }

#if _UWP
    if (ImGui::Button("Exit Game"))
    {
      if (!UWP::g_tried_graceful_shutdown.TestAndClear())
      {
        UWP::g_shutdown_requested.Set();
      }
      else
      {
        exit(0);
      }
    } 
  }
#endif

  ImGui::End();
}
}  // namespace OSD
