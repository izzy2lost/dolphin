// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// 
//  This is a WIP version of an ImGui frontend by SirMangler for use in the UWP fork.
//  This needs some cleaning up, right now it's something of a god class.
//  Note: This is not intended to replace any official Dolphin 'big picture' UI, of which does not exist at the time of writing.
//

#pragma once

#include "ImGuiFrontend.h"
#include "D3DWindow.h"
#include "UWPUtils.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>

#include <d3d11.h>
#include <dxgi1_4.h>
#include <tchar.h>

#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/windows.graphics.display.core.h>
#include <winrt/windows.gaming.input.h>
#include <windows.applicationmodel.h>
#include <gamingdeviceinformation.h>

#ifdef _DEBUG
#define DX11_ENABLE_DEBUG_LAYER
#endif

#ifdef DX11_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#include <unordered_map>
#include <wil/com.h>

#include "Core/Config/MainSettings.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/TitleDatabase.h"
#include "Core/HW/EXI/EXI_Device.h"

#include "Common/FileUtil.h"
#include "Common/Image.h"
#include "Common/Timer.h"

#include "UICommon/UICommon.h"
#include "UICommon/GameFile.h"
#include "UICommon/GameFileCache.h"

#include "VideoCommon/VideoConfig.h"

#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/ControllerInterface/WGInput/WGInput.h"

namespace WGI = winrt::Windows::Gaming::Input;
using winrt::Windows::UI::Core::CoreWindow;
using namespace winrt;

namespace ImGuiFrontend {
class UIState
{
public:
  bool controlsDisabled = false;
  bool showSettingsWindow = false;
  bool menuPressed = false;
  std::string selectedPath;
};

D3DWindow m_d3dWnd;

std::shared_ptr<ciface::Core::Device> m_controller = nullptr;
std::vector<std::string> m_paths;
std::vector<std::shared_ptr<UICommon::GameFile>> m_games;
Core::TitleDatabase m_title_database;
std::unordered_map<std::string, ID3D11ShaderResourceView*> m_cover_textures;
u64 m_imgui_last_frame_time;

ID3D11ShaderResourceView* m_background_tex;
ID3D11ShaderResourceView* m_missing_tex;

int m_selectedGameIdx;
float m_frameScale = 1.0f;
bool m_direction_pressed = false;
std::chrono::high_resolution_clock::time_point m_scroll_last = std::chrono::high_resolution_clock::now();

ImGuiFrontend::ImGuiFrontend()
{
  // Initialize Direct3D11
  m_d3dWnd.CreateDeviceD3D();

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  m_d3dWnd.InitImGui();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
  io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

  ImGui::StyleColorsDark();

  m_d3dWnd.CleanupRenderTarget();

  GAMING_DEVICE_MODEL_INFORMATION info = {};
  GetGamingDeviceModelInformation(&info);
  if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT)
  {
    winrt::Windows::Graphics::Display::Core::HdmiDisplayInformation hdi =
        winrt::Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
    if (hdi)
    {
      constexpr float frontend_modifier = 1.8f; 
      uint32_t width = hdi.GetCurrentDisplayMode().ResolutionWidthInRawPixels();
      uint32_t height = hdi.GetCurrentDisplayMode().ResolutionHeightInRawPixels();

      m_frameScale = ((float) width / 1920.0f) * frontend_modifier;
      io.DisplaySize.x = width;
      io.DisplaySize.y = height;
      io.FontGlobalScale = m_frameScale;
      io.DisplayFramebufferScale.x = m_frameScale;
      io.DisplayFramebufferScale.y = m_frameScale;

      ImGui::GetStyle().ScaleAllSizes(m_frameScale);
      m_d3dWnd.ResizeSwapChain(width, height);
    }
  }

  m_d3dWnd.CreateRenderTarget();

  g_controller_interface.Initialize({});

  PopulateControls();

  UICommon::SetUserDirectory(UWP::GetUserLocation());
  UICommon::CreateDirectories();
  UICommon::Init();

  LoadGameList();
}

static bool COMIsInitialized()
{
  APTTYPE apt_type{};
  APTTYPEQUALIFIER apt_qualifier{};
  return CoGetApartmentType(&apt_type, &apt_qualifier) == S_OK;
}

void ImGuiFrontend::PopulateControls()
{
  g_controller_interface.RefreshDevices();

  if (!g_controller_interface.HasDefaultDevice())
    return;

  ciface::Core::DeviceQualifier dq;
  dq.FromString(g_controller_interface.GetDefaultDeviceString());
  auto device = g_controller_interface.FindDevice(dq);
  if (device)
  {
    m_controller = std::move(device);
  }
}

void ImGuiFrontend::RefreshControls(bool updateGameSelection)
{
  if (m_controller == nullptr || m_controller->Inputs().empty() || m_controller->Outputs().empty())
    PopulateControls();

  if (m_controller == nullptr || !m_controller->IsValid())
    return;

  m_controller->UpdateInput();

  ImGuiIO& io = ImGui::GetIO();

  io.NavInputs[ImGuiNavInput_Activate] =
      m_controller->FindInput("Button A")->GetState() == 1.0f ? 1.0 : 0;
  io.NavInputs[ImGuiNavInput_Cancel] =
      m_controller->FindInput("Button B")->GetState() == 1.0f ? 1.0 : 0;
  io.NavInputs[ImGuiNavInput_DpadUp] =
      m_controller->FindInput("Left Y+")->GetState() == 1.0f ? 1.0 : 0;
  io.NavInputs[ImGuiNavInput_DpadDown] =
      m_controller->FindInput("Left Y-")->GetState() == 1.0f ? 1.0 : 0;
  io.NavInputs[ImGuiNavInput_DpadLeft] =
      m_controller->FindInput("Left X-")->GetState() == 1.0f ? 1.0 : 0;
  io.NavInputs[ImGuiNavInput_DpadRight] =
      m_controller->FindInput("Left X+")->GetState() == 1.0f ? 1.0 : 0;

  // wrap around if exceeding the max games or going below
  if (updateGameSelection)
  {
    auto now = std::chrono::high_resolution_clock::now();
    long timeSinceLastInput =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - m_scroll_last).count();
    if (m_controller->FindInput("Pad W")->GetState() > 0.5f)
    {
      if (!m_direction_pressed)
      {
        m_selectedGameIdx = m_selectedGameIdx <= 0 ? m_games.size() - 1 : m_selectedGameIdx - 1;
        m_direction_pressed = true;
      }
    }
    else if (m_controller->FindInput("Pad E")->GetState() > 0.5f)
    {
      if (!m_direction_pressed)
      {
        m_selectedGameIdx = m_selectedGameIdx >= m_games.size() - 1 ? 0 : m_selectedGameIdx + 1;
        m_direction_pressed = true;
      }
    }
    else if (m_controller->FindInput("Left X-")->GetState() > 0.5f)
    {
      if (timeSinceLastInput > 200L)
      {
        m_selectedGameIdx = m_selectedGameIdx <= 0 ? m_games.size() - 1 : m_selectedGameIdx - 1;
        m_scroll_last = std::chrono::high_resolution_clock::now();
      }
    }
    else if (m_controller->FindInput("Left X+")->GetState() > 0.5f)
    {
      if (timeSinceLastInput > 200L)
      {
        m_selectedGameIdx = m_selectedGameIdx >= m_games.size() - 1 ? 0 : m_selectedGameIdx + 1;
        m_scroll_last = std::chrono::high_resolution_clock::now();
      }
    }
    else if (m_games.size() > 10 && m_controller->FindInput("Bumper L")->GetState() > 0.5f)
    {
      if (timeSinceLastInput > 500L)
      {
        int i = m_selectedGameIdx - 10;
        if (i < 0)
        {
          // wrap around, total games + -index
          m_selectedGameIdx = m_games.size() + i;
        }
        else
        {
          m_selectedGameIdx = i;
        }

        m_scroll_last = std::chrono::high_resolution_clock::now();
      }
    }
    else if (m_games.size() > 10 && m_controller->FindInput("Bumper R")->GetState() > 0.5f)
    {
      if (timeSinceLastInput > 500L)
      {
        int i = m_selectedGameIdx + 10;
        if (i >= m_games.size())
        {
          // wrap around, i - total games
          m_selectedGameIdx = i - m_games.size();
        }
        else
        {
          m_selectedGameIdx = i;
        }

        m_scroll_last = std::chrono::high_resolution_clock::now();
      }
    }
    else
    {
      m_direction_pressed = false;
    }
  }
}

std::shared_ptr<UICommon::GameFile> ImGuiFrontend::RunUntilSelection()
{
  return RunMainLoop();
}

std::shared_ptr<UICommon::GameFile> ImGuiFrontend::RunMainLoop()
{
  std::shared_ptr<UICommon::GameFile> selection;
  UIState state = UIState();

  // Main loop
  bool done = false;
  while (!done)
  {
    CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(
        winrt::Windows::UI::Core::CoreProcessEventsOption::ProcessAllIfPresent);

    if (!state.controlsDisabled)
      RefreshControls(!state.showSettingsWindow);

    if (m_controller && m_controller->IsValid() && !state.controlsDisabled)
    {
      if (m_controller->FindInput("View")->GetState() == 1.0f)
      {
        if (!state.menuPressed)
        {
          state.showSettingsWindow = !state.showSettingsWindow;
          state.menuPressed = true;
        }

        LoadGameList();   
      }
      else
      {
        state.menuPressed = false;
      }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    // Draw Background first

    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("Background", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav)) {
      ImGui::Image(GetOrCreateBackgroundTex(), ImGui::GetIO().DisplaySize);
      ImGui::End();
    }
    ImGui::PopStyleVar(3);
    // -- Background

    auto navigation = winrt::Windows::UI::Core::SystemNavigationManager::GetForCurrentView();

    if (state.showSettingsWindow)
    {
      // Disable the back button while in a popup
      navigation.BackRequested(
          [](const winrt::Windows::Foundation::IInspectable&,
             const winrt::Windows::UI::Core::BackRequestedEventArgs& args) { args.Handled(true); });

      ImGui::SetNextWindowSize(ImVec2(540 * m_frameScale, 425 * m_frameScale));
      ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2 - (540 / 2) * m_frameScale,
                                         ImGui::GetIO().DisplaySize.y / 2 - (425 / 2) * m_frameScale));
      if (ImGui::Begin("Settings", nullptr,
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize))
      {
        ImGui::BeginTabBar("#settingsTab");

        if (ImGui::BeginTabItem("General"))
        {
          CreateGeneralTab(&state);
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Interface"))
        {
          CreateInterfaceTab(&state);
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Graphics"))
        {
          CreateGraphicsTab(&state);
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("GameCube"))
        {
          CreateGameCubeTab(&state);
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Wii"))
        {
          CreateWiiTab(&state);
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Paths & Folders"))
        {
          CreatePathsTab(&state);
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Advanced"))
        {
          CreateAdvancedTab(&state);
          ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
        ImGui::End();
      }
    }
    else
    {
      // Re-enable the Back button.
      navigation.BackRequested([](const winrt::Windows::Foundation::IInspectable&,
                                  const winrt::Windows::UI::Core::BackRequestedEventArgs& args) {});
      selection = CreateMainPage();
      if (selection != nullptr)
      {
        break;
      }
    }

    m_d3dWnd.Render();
  }

  m_d3dWnd.DeInitImGui();
  m_d3dWnd.CleanupDeviceD3D();
  g_controller_interface.Shutdown();

  return selection;
}

void ImGuiFrontend::CreateGeneralTab(UIState* state)
{
  bool dualCore = Config::Get(Config::MAIN_CPU_THREAD);
  if (ImGui::Checkbox("Dual Core", &dualCore))
  {
    Config::SetBaseOrCurrent(Config::MAIN_CPU_THREAD, dualCore);
    Config::Save();
  }

  bool cheats = Config::Get(Config::MAIN_ENABLE_CHEATS);
  if (ImGui::Checkbox("Enable Cheats", &cheats))
  {
    Config::SetBaseOrCurrent(Config::MAIN_ENABLE_CHEATS, cheats);
    Config::Save();
  }

  bool mismatchedRegion = Config::Get(Config::MAIN_OVERRIDE_REGION_SETTINGS);
  if (ImGui::Checkbox("Allow Mismatched Region Settings", &mismatchedRegion))
  {
    Config::SetBaseOrCurrent(Config::MAIN_OVERRIDE_REGION_SETTINGS, mismatchedRegion);
    Config::Save();
  }

  bool changeDiscs = Config::Get(Config::MAIN_AUTO_DISC_CHANGE);
  if (ImGui::Checkbox("Change Discs Automatically", &changeDiscs))
  {
      Config::SetBaseOrCurrent(Config::MAIN_AUTO_DISC_CHANGE, changeDiscs);
      Config::Save();
  }

  const auto fallback = Config::Get(Config::MAIN_FALLBACK_REGION);
  if (ImGui::TreeNode("Fallback Region"))
  {
    if (ImGui::RadioButton("NTSC JP", fallback == DiscIO::Region::NTSC_J))
    {
      Config::SetBaseOrCurrent(Config::MAIN_FALLBACK_REGION, DiscIO::Region::NTSC_J);
      Config::Save();
    }

    if (ImGui::RadioButton("NTSC NA", fallback == DiscIO::Region::NTSC_U))
    {
      Config::SetBaseOrCurrent(Config::MAIN_FALLBACK_REGION, DiscIO::Region::NTSC_U);
      Config::Save();
    }

    if (ImGui::RadioButton("PAL", fallback == DiscIO::Region::PAL))
    {
      Config::SetBaseOrCurrent(Config::MAIN_FALLBACK_REGION, DiscIO::Region::PAL);
      Config::Save();
    }

    if (ImGui::RadioButton("Unknown", fallback == DiscIO::Region::Unknown))
    {
      Config::SetBaseOrCurrent(Config::MAIN_FALLBACK_REGION, DiscIO::Region::Unknown);
      Config::Save();
    }

    if (ImGui::RadioButton("NTSC Korea", fallback == DiscIO::Region::NTSC_K))
    {
      Config::SetBaseOrCurrent(Config::MAIN_FALLBACK_REGION, DiscIO::Region::NTSC_K);
      Config::Save();
    }

    ImGui::TreePop();
  }
}

void ImGuiFrontend::CreateInterfaceTab(UIState* state)
{
  bool showFps = Config::Get(Config::GFX_SHOW_FPS);
  if (ImGui::Checkbox("Show FPS", &showFps))
  {
    Config::SetBaseOrCurrent(Config::GFX_SHOW_FPS, showFps);
    Config::Save();
  }

  bool showOSD = Config::Get(Config::MAIN_OSD_MESSAGES);
  if (ImGui::Checkbox("Show On-Screen Messages", &showOSD))
  {
    Config::SetBaseOrCurrent(Config::MAIN_OSD_MESSAGES, showOSD);
    Config::Save();
  }

  bool showStats = Config::Get(Config::GFX_OVERLAY_STATS);
  if (ImGui::Checkbox("Show Rendering Statistics", &showStats))
  {
    Config::SetBaseOrCurrent(Config::GFX_OVERLAY_STATS, showStats);
    Config::Save();
  }
}


void ImGuiFrontend::CreateGraphicsTab(UIState* state)
{
  bool vSync = Config::Get(Config::GFX_VSYNC);
  if (ImGui::Checkbox("V-Sync", &vSync))
  {
    Config::SetBaseOrCurrent(Config::GFX_VSYNC, vSync);
    Config::Save();
  }

  bool scaledEfb = Config::Get(Config::GFX_HACK_COPY_EFB_SCALED);
  if (ImGui::Checkbox("Scaled EFB Copy", &scaledEfb))
  {
    Config::SetBaseOrCurrent(Config::GFX_HACK_COPY_EFB_SCALED, scaledEfb);
    Config::Save();
  }

  const char* ir_items[] = {"Auto (Multiple of 640x528)",      "Native (640x528)",
                            "2x Native (1280x1056) for 720p",  "3x Native (1920x1584) for 1080p",
                            "4x Native (2560x2112) for 1440p", "5x Native (3200x2640)",
                            "6x Native (3840x3168) for 4K",    "7x Native (4480x3696)",
                            "8x Native (5120x4224) for 5K" };
      
  int ir_idx = Config::Get(Config::GFX_EFB_SCALE);

  if (ImGui::TreeNode("Internal Resolution"))
  {
    for (int i = 0; i < 9; i++)
    {
      ImGui::PushID(i);
      if (ImGui::RadioButton(ir_items[i], i == ir_idx))
      {
        Config::SetBase(Config::GFX_EFB_SCALE, i);
        Config::Save();
      }
      ImGui::PopID();
    }

    ImGui::TreePop();
  }

  const char* aspect_items[] = {"Auto", "Force 16:9", "Force 4:3", "Stretch"};
  int aspect_idx = 0;
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
      if (ImGui::RadioButton(aspect_items[i], i == aspect_idx))
      {
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

          Config::Save();
      }
      ImGui::PopID();
    }

    ImGui::TreePop();
  }

  const char* shader_items[] = {"Synchronous", "Hybrid Ubershaders", "Exclusive Ubershaders",
                                "Skip Drawing"};
  int shader_idx = 0;
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
      if (ImGui::RadioButton(shader_items[i], i == shader_idx))
      {
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

          Config::Save();
      }
      ImGui::PopID();
    }

    bool waitForCompile = Config::Get(Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING);
    if (ImGui::Checkbox("Compile Shaders Before Starting", &waitForCompile))
    {
      Config::SetBaseOrCurrent(Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING, waitForCompile);
      Config::Save();
    }

    ImGui::TreePop();
  }

  const char* aalevel_items[] = {"None", "2x", "4x", "8x"};
  auto msaa = Config::Get(Config::GFX_MSAA);
  bool ssaa = Config::Get(Config::GFX_SSAA);

  if (ImGui::TreeNode("Anti-Aliasing"))
  {
    if (ImGui::RadioButton("MSAA", !ssaa))
    {
      Config::SetBaseOrCurrent(Config::GFX_SSAA, false);
      Config::Save();
    }

    if (ImGui::RadioButton("SSAA", ssaa))
    {
      Config::SetBaseOrCurrent(Config::GFX_SSAA, true);
      Config::Save();
    }

    ImGui::Separator();
    for (int i = 0; i < 4; i++)
    {
      ImGui::PushID(i);
      if (ImGui::RadioButton(aalevel_items[i], i == msaa))
      {
          Config::SetBaseOrCurrent(Config::GFX_MSAA, i);
          Config::Save();
      }
      ImGui::PopID();
    }

    ImGui::TreePop();
  }

  const char* anisolevel_items[] = {"1x", "2x", "4x", "8x", "16x"};
  auto aniso = Config::Get(Config::GFX_ENHANCE_MAX_ANISOTROPY);

  if (ImGui::TreeNode("Anisotropic Filtering"))
  {
    for (int i = 0; i < 5; i++)
    {
      ImGui::PushID(i);
      if (ImGui::RadioButton(anisolevel_items[i], i == aniso))
      {
          Config::SetBaseOrCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY, i);
          Config::Save();
      }
      ImGui::PopID();
    }

    ImGui::TreePop();
  }

  bool disableFog = Config::Get(Config::GFX_DISABLE_FOG);
  if (ImGui::Checkbox("Disable Fog", &disableFog))
  {
    Config::SetBaseOrCurrent(Config::GFX_DISABLE_FOG, disableFog);
    Config::Save();
  }

  bool perPixelLighting = Config::Get(Config::GFX_ENABLE_PIXEL_LIGHTING);
  if (ImGui::Checkbox("Per-Pixel Lighting", &perPixelLighting))
  {
    Config::SetBaseOrCurrent(Config::GFX_ENABLE_PIXEL_LIGHTING, perPixelLighting);
    Config::Save();
  }

  bool disableCopyFilter = Config::Get(Config::GFX_ENHANCE_DISABLE_COPY_FILTER);
  if (ImGui::Checkbox("Disable Copy Filter", &disableCopyFilter))
  {
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_DISABLE_COPY_FILTER, disableCopyFilter);
    Config::Save();
  }
}

void ImGuiFrontend::CreateGameCubeTab(UIState* state)
{
  const char* language_items[] = {"English", "German", "French", "Spanish", "Italian", "Dutch"};
  auto lang = Config::Get(Config::MAIN_GC_LANGUAGE);

  if (ImGui::TreeNode("System Language"))
  {
    for (int i = 0; i < 6; i++)
    {
      ImGui::PushID(i);
      if (ImGui::RadioButton(language_items[i], i == lang))
      {
          Config::SetBaseOrCurrent(Config::MAIN_GC_LANGUAGE, i);
          Config::Save();
      }
      ImGui::PopID();
    }

    ImGui::TreePop();
  }

  auto slot1 = Config::Get(Config::MAIN_SLOT_A);
  if (ImGui::TreeNode("Slot A"))
  {
    if (ImGui::RadioButton("<Nothing>", slot1 == ExpansionInterface::EXIDeviceType::None))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SLOT_A, ExpansionInterface::EXIDeviceType::None);
      Config::Save();
    }

    if (ImGui::RadioButton("Dummy", slot1 == ExpansionInterface::EXIDeviceType::Dummy))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SLOT_A,
                               ExpansionInterface::EXIDeviceType::Dummy);
      Config::Save();
    }

    if (ImGui::RadioButton("Memory Card", slot1 == ExpansionInterface::EXIDeviceType::MemoryCard))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SLOT_A,
                               ExpansionInterface::EXIDeviceType::MemoryCard);
      Config::Save();
    }

    if (ImGui::RadioButton("GCI Folder",
                           slot1 == ExpansionInterface::EXIDeviceType::MemoryCardFolder))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SLOT_A,
                               ExpansionInterface::EXIDeviceType::MemoryCardFolder);
      Config::Save();
    }

    if (ImGui::RadioButton("USB Gecko", slot1 == ExpansionInterface::EXIDeviceType::Gecko))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SLOT_A,
                               ExpansionInterface::EXIDeviceType::Gecko);
      Config::Save();
    }

    if (ImGui::RadioButton("Advance Game Port", slot1 == ExpansionInterface::EXIDeviceType::AGP))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SLOT_A, ExpansionInterface::EXIDeviceType::AGP);
      Config::Save();
    }

    ImGui::TreePop();
  }

  // Todo: This really shouldn't be copy+pasted and could be cleaned up.
  auto slot2 = Config::Get(Config::MAIN_SLOT_B);
  if (ImGui::TreeNode("Slot B"))
  {
    if (ImGui::RadioButton("<Nothing>", slot2 == ExpansionInterface::EXIDeviceType::None))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SLOT_B, ExpansionInterface::EXIDeviceType::None);
      Config::Save();
    }

    if (ImGui::RadioButton("Dummy", slot2 == ExpansionInterface::EXIDeviceType::Dummy))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SLOT_B, ExpansionInterface::EXIDeviceType::Dummy);
      Config::Save();
    }

    if (ImGui::RadioButton("Memory Card", slot2 == ExpansionInterface::EXIDeviceType::MemoryCard))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SLOT_B, ExpansionInterface::EXIDeviceType::MemoryCard);
      Config::Save();
    }

    if (ImGui::RadioButton("GCI Folder",
                           slot2 == ExpansionInterface::EXIDeviceType::MemoryCardFolder))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SLOT_B,
                               ExpansionInterface::EXIDeviceType::MemoryCardFolder);
      Config::Save();
    }

    if (ImGui::RadioButton("USB Gecko", slot2 == ExpansionInterface::EXIDeviceType::Gecko))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SLOT_B, ExpansionInterface::EXIDeviceType::Gecko);
      Config::Save();
    }

    if (ImGui::RadioButton("Advance Game Port", slot2 == ExpansionInterface::EXIDeviceType::AGP))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SLOT_B, ExpansionInterface::EXIDeviceType::AGP);
      Config::Save();
    }

    ImGui::TreePop();
  }

  auto sp1 = Config::Get(Config::MAIN_SERIAL_PORT_1);
  if (ImGui::TreeNode("SP1"))
  {
    if (ImGui::RadioButton("<Nothing>", sp1 == ExpansionInterface::EXIDeviceType::None))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SERIAL_PORT_1, ExpansionInterface::EXIDeviceType::None);
      Config::Save();
    }

    if (ImGui::RadioButton("Dummy", sp1 == ExpansionInterface::EXIDeviceType::Dummy))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SERIAL_PORT_1, ExpansionInterface::EXIDeviceType::Dummy);
      Config::Save();
    }

    if (ImGui::RadioButton("Broadband Adapter (TAP)", sp1 == ExpansionInterface::EXIDeviceType::Ethernet))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SERIAL_PORT_1,
                               ExpansionInterface::EXIDeviceType::Ethernet);
      Config::Save();
    }

    if (ImGui::RadioButton("Broadband Adapter (XLink Kai)",
                           sp1 == ExpansionInterface::EXIDeviceType::EthernetXLink))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SERIAL_PORT_1,
                               ExpansionInterface::EXIDeviceType::EthernetXLink);
      Config::Save();
    }

    if (ImGui::RadioButton("Broadband Adapter (HLE)",
                           sp1 == ExpansionInterface::EXIDeviceType::EthernetBuiltIn))
    {
      Config::SetBaseOrCurrent(Config::MAIN_SERIAL_PORT_1,
                               ExpansionInterface::EXIDeviceType::EthernetBuiltIn);
      Config::Save();
    }

    ImGui::TreePop();
  }
}

void ImGuiFrontend::CreateWiiTab(UIState* state)
{
  bool pal60 = Config::Get(Config::SYSCONF_PAL60);
  if (ImGui::Checkbox("Enable PAL60", &pal60))
  {
    Config::SetBaseOrCurrent(Config::SYSCONF_PAL60, pal60);
    Config::Save();
  }

  const char* language_items[] = {"Japanese", "English", "German", "French",
                                  "Spanish",  "Italian", "Dutch",  "Simplified Chinese",
                                  "Traditional Chinese",
                                  "Korean"};
  auto lang = Config::Get(Config::SYSCONF_LANGUAGE);

  if (ImGui::TreeNode("System Language"))
  {
    for (int i = 0; i < 10; i++)
    {
      ImGui::PushID(i);
      if (ImGui::RadioButton(language_items[i], i == lang))
      {
          Config::SetBaseOrCurrent(Config::SYSCONF_LANGUAGE, i);
          Config::Save();
      }
      ImGui::PopID();
    }

    ImGui::TreePop();
  }

  const char* sound_items[] = {"Mono", "Stereo", "Surround"};
  auto sound = Config::Get(Config::SYSCONF_SOUND_MODE);

  if (ImGui::TreeNode("Sound"))
  {
    for (int i = 0; i < 4; i++)
    {
      ImGui::PushID(i);
      if (ImGui::RadioButton(sound_items[i], i == sound))
      {
          Config::SetBaseOrCurrent(Config::SYSCONF_SOUND_MODE, i);
          Config::Save();
      }
      ImGui::PopID();
    }

    ImGui::TreePop();
  }
}

void ImGuiFrontend::CreateAdvancedTab(UIState* state)
{
  bool viSkipEnable = Config::Get(Config::GFX_HACK_VI_SKIP);
  if (ImGui::Checkbox("Enable VI Skip", &viSkipEnable))
  {
    Config::SetBaseOrCurrent(Config::GFX_HACK_VI_SKIP, viSkipEnable);
    Config::Save();
  }

  bool hiresTexEnable = Config::Get(Config::GFX_HIRES_TEXTURES);
  if (ImGui::Checkbox("Load Custom Textures", &hiresTexEnable))
  {
    Config::SetBaseOrCurrent(Config::GFX_HIRES_TEXTURES, hiresTexEnable);
    Config::Save();
  }

  bool prefetchTexEnable = Config::Get(Config::GFX_CACHE_HIRES_TEXTURES);
  if (ImGui::Checkbox("Prefetch Custom Textures", &prefetchTexEnable))
  {
    Config::SetBaseOrCurrent(Config::GFX_CACHE_HIRES_TEXTURES, prefetchTexEnable);
    Config::Save();
  }

  bool graphicsModsEnable = Config::Get(Config::GFX_MODS_ENABLE);
  if (ImGui::Checkbox("Enable Graphics Mods", &graphicsModsEnable))
  {
    Config::SetBaseOrCurrent(Config::GFX_MODS_ENABLE,
                                   graphicsModsEnable);
    Config::Save();
  }

  bool textureDumping = Config::Get(Config::GFX_DUMP_TEXTURES);
  if (ImGui::Checkbox("Enable Texture Dumping", &textureDumping))
  {
    Config::SetBaseOrCurrent(Config::GFX_DUMP_TEXTURES, textureDumping);
    Config::Save();
  }

  auto textureCache = Config::Get(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES);
  if (ImGui::TreeNode("Texture Cache Accuracy"))
  {
    if (ImGui::RadioButton("Safe", textureCache == 0))
    {
      Config::SetBaseOrCurrent(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, 0);
      Config::Save();
    }

    if (ImGui::RadioButton("Balanced", textureCache == 512))
    {
      Config::SetBaseOrCurrent(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, 512);
      Config::Save();
    }

    if (ImGui::RadioButton("Fast", textureCache == 128))
    {
      Config::SetBaseOrCurrent(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, 128);
      Config::Save();
    }

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Clock Override"))
  {
    ImGui::Text("WARNING: Changing this from the default (1.0 = 100%) can and will break\ngames and cause glitches. Do so at your own risk. \nPlease do not report bugs that occur with a non-default clock. \nThis is not a magical performance slider!");

    bool overclockEnable = Config::Get(Config::MAIN_OVERCLOCK_ENABLE);
    if (ImGui::Checkbox("Enable Emulated CPU Clock Override", &overclockEnable))
    {
      Config::SetBaseOrCurrent(Config::MAIN_OVERCLOCK_ENABLE, overclockEnable);
      Config::Save();
    }

    float clockOverride = Config::Get(Config::MAIN_OVERCLOCK);
    if (ImGui::SliderFloat("Emulated CPU Clock Speed Override", &clockOverride, 0.06f, 4.0f))
    {
      Config::SetBaseOrCurrent(Config::MAIN_OVERCLOCK, clockOverride);
      Config::Save();
    }

    ImGui::TreePop();
  }
}

void ImGuiFrontend::CreatePathsTab(UIState* state)
{
  ImGui::Text("Game Folders");
  if (ImGui::BeginListBox("##folders")) {
    for (auto path : m_paths)
    {
      if (ImGui::Selectable(path.c_str()))
      {
        state->selectedPath = path;
      }
    }
    ImGui::EndListBox();
  }

  if (state->selectedPath == "")
  {
    ImGui::BeginDisabled();
  }

  if (ImGui::Button("Remove Path"))
  {
    if (state->selectedPath != "")
    {
      m_paths.erase(std::remove(m_paths.begin(), m_paths.end(), state->selectedPath),
                    m_paths.end());
      Config::SetIsoPaths(m_paths);
      Config::Save();
    }
  }

  if (state->selectedPath == "")
  {
    ImGui::EndDisabled();
  }

  ImGui::SameLine();
  if (ImGui::Button("Add Path"))
  {
    state->controlsDisabled = true;
    UWP::OpenGameFolderPicker([state](std::string path) {
      if (path != "")
      {
        m_paths.emplace_back(path);
        Config::SetIsoPaths(m_paths);
        Config::Save();
      }

      state->controlsDisabled = false;
    });
  }

  ImGui::Separator();

  if (ImGui::Button("Set Dolphin User Folder Location"))
  {
    state->controlsDisabled = true;
    UWP::OpenNewUserPicker([=]() { state->controlsDisabled = false; });

    // Reset everything and load the new config location.
    UICommon::Shutdown();
    UICommon::SetUserDirectory(UWP::GetUserLocation());
    UICommon::CreateDirectories();
    UICommon::Init();
  }

  if (ImGui::Button("Reset Dolphin User Folder Location"))
  {
    UWP::ResetUserLocation();

    // Reset everything and load the new config location.
    UICommon::Shutdown();
    UICommon::SetUserDirectory(UWP::GetUserLocation());
    UICommon::CreateDirectories();
    UICommon::Init();
  }
}

std::shared_ptr<UICommon::GameFile> ImGuiFrontend::CreateMainPage()
{
  //float selOffset = m_selectedGameIdx >= 5 ? 160.0f * (m_selectedGameIdx - 4) * -1.0f : 0;
  float posX = 30 * m_frameScale;
  float posY = (345.0f / 2) * m_frameScale;
  auto extraFlags = m_games.size() < 5 ? ImGuiWindowFlags_None :
                                         ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;

  ImGui::SetNextWindowPos(ImVec2(posX, posY));
  if (ImGui::Begin("Dolphin Emulator", nullptr, ImGuiWindowFlags_NoTitleBar |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoScrollbar |
                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground |
                       extraFlags))
  {
    auto game = CreateGameList();
    ImGui::End();
    if (game != nullptr)
    {
      return game;
    }
  }

  const u64 current_time_us = Common::Timer::NowUs();
  const u64 time_diff_us = current_time_us -m_imgui_last_frame_time;
  const float time_diff_secs = static_cast<float>(time_diff_us / 1000000.0);
  m_imgui_last_frame_time = current_time_us;

  // Update I/O with window dimensions.
  ImGuiIO& io = ImGui::GetIO();
  io.DeltaTime = time_diff_secs;

  return nullptr; // keep running
}

std::shared_ptr<UICommon::GameFile> ImGuiFrontend::CreateGameList()
{
  if (ImGui::GetIO().NavInputs[ImGuiNavInput_Activate] > 0.5f)
  {
    if (m_games.size() != 0)
      return m_games[m_selectedGameIdx];
  }

  auto table_flags = ImGuiTableFlags_Borders;
  int selectionIdx = 0;

  // Display 5 games, 2 games to the left of the selection, 2 games to the right.
  for (int i = m_selectedGameIdx - 2; i < m_selectedGameIdx + 3; i++)
  {
    int idx = i;
    if (m_games.size() >= 4)
    {
      if (i < 0)
      {
        // wrap around, total games + -index
        idx = m_games.size() + i;
      }
      else if (i >= m_games.size())
      {
        // wrap around, i - total games
        idx = i - m_games.size();
      }
    }
    else
    {
      if (i < 0)
      {
        idx = m_games.size() + i;
      }
      else if (i >= m_games.size())
      {
        continue;
      }
    }

    if (idx < 0 || idx >= m_games.size())
      continue;

    ImVec4 border_col;
    float selectedScale = 1.0f;
    if (m_selectedGameIdx == idx)
    {
      border_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
      // The background image doesn't fit 2 games very well when scaled up.
      selectedScale = m_games.size() > 2 ? 1.15 : 1.0f;
    }
    else
    {
      border_col = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    ID3D11ShaderResourceView* handle = GetHandleForGame(m_games[idx]);
    ImGui::SameLine();
    ImGui::BeginChild(
        m_games[idx]->GetFilePath().c_str(),
        ImVec2((160 + 25) * m_frameScale * selectedScale, 250 * m_frameScale * selectedScale), true,
        ImGuiWindowFlags_NavFlattened | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);

    if (handle != 0)
    {
      ImGui::Image((ImTextureID) handle,
          ImVec2(160.f * m_frameScale * selectedScale, 224.f * m_frameScale * selectedScale),
          ImVec2(0, 0), ImVec2(1, 1), ImVec4(1.0f, 1.0f, 1.0f, 1.0f), border_col);
      ImGui::Text(m_games[idx]->GetName(m_title_database).c_str());
    }
    else
    {
      ImGui::Text(m_games[idx]->GetName(m_title_database).c_str());
    }

    ImGui::EndChild();
  }

  return nullptr;
}

ID3D11ShaderResourceView*
ImGuiFrontend::GetHandleForGame(std::shared_ptr<UICommon::GameFile> game)
{
  std::string game_id = game->GetGameID();
  auto result = m_cover_textures.find(game_id);
  if (m_cover_textures.find(game_id) == m_cover_textures.end())
  {
    ID3D11ShaderResourceView* handle = CreateCoverTexture(game);
    if (handle == nullptr)
      handle = GetOrCreateMissingTex();

    m_cover_textures.emplace(game_id, handle);
    return handle;
  }

  return result->second;
}

ID3D11ShaderResourceView*
ImGuiFrontend::CreateCoverTexture(std::shared_ptr<UICommon::GameFile> game)
{
  if (!File::Exists(File::GetUserPath(D_COVERCACHE_IDX) + game->GetGameTDBID() + ".png"))
  {
    game->DownloadDefaultCover();
  }

  std::string png;
  if (!File::ReadFileToString(
          File::GetUserPath(D_COVERCACHE_IDX) + game->GetGameTDBID() + ".png", png))
    return {};

  std::vector<uint8_t> buffer = { png.begin(), png.end() };
  if (buffer.empty())
    return {};

  std::vector<uint8_t> data;
  u32 width, height;
  Common::LoadPNG(buffer, &data, &width, &height);

  ID3D11ShaderResourceView* tex_resource;
  if (m_d3dWnd.CreateTextureFromBuffer(width, height, data.data(), data.size(), &tex_resource))
    return tex_resource;

  return {};
}

ID3D11ShaderResourceView* ImGuiFrontend::GetOrCreateBackgroundTex()
{
  if (m_background_tex != nullptr)
    return m_background_tex;

  auto user_folder = File::GetUserPath(0);
  std::string bg_path = user_folder + "/background.png";

  if (!File::Exists(bg_path))
  {
    bg_path = "Assets/background.png";

    if (!File::Exists(bg_path))
      return nullptr;
  }
    
  std::string png;
  if (!File::ReadFileToString(bg_path, png))
    return {};

  std::vector<uint8_t> buffer = {png.begin(), png.end()};
  if (buffer.empty())
    return {};

  std::vector<uint8_t> data;
  u32 width, height;
  Common::LoadPNG(buffer, &data, &width, &height);

  if (m_d3dWnd.CreateTextureFromBuffer(width, height, data.data(), data.size(), &m_background_tex))
    return m_background_tex;

  return nullptr;
}

ID3D11ShaderResourceView* ImGuiFrontend::GetOrCreateMissingTex()
{
  if (m_missing_tex != nullptr)
    return m_missing_tex;

  std::string png;
  if (!File::ReadFileToString("Assets/missing.png", png))
    return {};

  std::vector<uint8_t> buffer = {png.begin(), png.end()};
  if (buffer.empty())
    return {};

  std::vector<uint8_t> data;
  u32 width, height;
  Common::LoadPNG(buffer, &data, &width, &height);

  if (m_d3dWnd.CreateTextureFromBuffer(width, height, data.data(), data.size(), &m_missing_tex))
    return m_missing_tex;

  return nullptr;
}

void ImGuiFrontend::LoadGameList()
{
  m_paths.clear();
  m_games.clear();
  m_paths = Config::GetIsoPaths();
  for (auto dir : m_paths)
  {
    RecurseFolder(dir);
  }

  // Load from the default path
  auto localCachePath = winrt::to_string(winrt::Windows::Storage::ApplicationData::Current().LocalCacheFolder().Path());
  RecurseFolder(localCachePath);

  std::sort(m_games.begin(), m_games.end(),
          [](std::shared_ptr<UICommon::GameFile> first,
                   std::shared_ptr<UICommon::GameFile> second) {
          return first->GetName(m_title_database) < second->GetName(m_title_database);
  });
}

void ImGuiFrontend::RecurseFolder(std::string path)
{
  try
  {
    for (auto file : std::filesystem::directory_iterator(path))
    {
      if (file.is_directory())
      {
        RecurseFolder(file.path().string());
        continue;
      }

      if (!file.is_regular_file())
        continue;

      auto game = new UICommon::GameFile(file.path().string());

      if (game && game->IsValid())
        m_games.emplace_back(std::move(game));
    }
  }
  catch (std::exception)
  {
    // This folder can't be opened.
  }
}

void ImGuiFrontend::AddGameFolder(std::string path)
{
  m_paths.push_back(path);
  Config::SetIsoPaths(m_paths);
}
}
