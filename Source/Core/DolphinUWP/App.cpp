#include <windows.h>

#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Input.h>

#include <Gamingdeviceinformation.h>

#include <Common/WindowSystemInfo.h>
#include <Core/Boot/Boot.h>
#include <Core/BootManager.h>
#include <Core/Core.h>
#include <Core/HW/ProcessorInterface.h>
#include <Core/Host.h>
#include <Core/IOS/STM/STM.h>
#include <UICommon/UICommon.h>

#define SDL_MAIN_HANDLED

using namespace winrt;

using namespace Windows;
using namespace Windows::Storage;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI::Composition;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Pickers;

using winrt::Windows::UI::Core::BackRequestedEventArgs;
using winrt::Windows::UI::Core::CoreProcessEventsOption;
using winrt::Windows::UI::Core::CoreWindow;

Common::Flag m_running{true};
Common::Flag m_shutdown_requested{false};
Common::Flag m_tried_graceful_shutdown{false};

struct App : implements<App, IFrameworkViewSource, IFrameworkView>
{
  IFrameworkView CreateView() { return *this; }

  void Initialize(CoreApplicationView const& v) { v.Activated({this, &App::OnActivate}); }

  void Load(hstring const&) {}

  void Uninitialize() {}

  void Run()
  {
    InitializeDolphin();

    while (m_running.IsSet())
    {
      if (m_shutdown_requested.TestAndClear())
      {
        const auto ios = IOS::HLE::GetIOS();
        const auto stm = ios ? ios->GetDeviceByName("/dev/stm/eventhook") : nullptr;
        if (!m_tried_graceful_shutdown.IsSet() && stm &&
            std::static_pointer_cast<IOS::HLE::STMEventHookDevice>(stm)->HasHookInstalled())
        {
          ProcessorInterface::PowerButton_Tap();
          m_tried_graceful_shutdown.Set();
        }
        else
        {
          m_running.Clear();
        }
      }

      ::Core::HostDispatchJobs();
      CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(
          CoreProcessEventsOption::ProcessAllIfPresent);

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  winrt::fire_and_forget InitializeDolphin()
  {
    FileOpenPicker openPicker;
    openPicker.ViewMode(PickerViewMode::List);
    openPicker.SuggestedStartLocation(PickerLocationId::ComputerFolder);
    openPicker.FileTypeFilter().Append(L".iso");
    openPicker.FileTypeFilter().Append(L".ciso");
    openPicker.FileTypeFilter().Append(L".rvz");
    openPicker.FileTypeFilter().Append(L".wbfs");
    openPicker.FileTypeFilter().Append(L".gcm");
    openPicker.FileTypeFilter().Append(L".gcz");

    auto file = co_await openPicker.PickSingleFileAsync();
    if (file)
    {
      CoreWindow window = CoreWindow::GetForCurrentThread();
      void* abi = winrt::get_abi(window);

      WindowSystemInfo wsi;
      wsi.type = WindowSystemType::UWP;
      wsi.render_window = abi;
      wsi.render_surface = abi;
      wsi.render_width = window.Bounds().Width;
      wsi.render_height = window.Bounds().Height;

      auto navigation = winrt::Windows::UI::Core::SystemNavigationManager::GetForCurrentView();

      // UWP on Xbox One triggers a back request whenever the B button is pressed
      // which can result in the app being suspended if unhandled
      navigation.BackRequested([](const winrt::Windows::Foundation::IInspectable&,
                                  const BackRequestedEventArgs& args) { args.Handled(true); });

      GAMING_DEVICE_MODEL_INFORMATION info = {};
      GetGamingDeviceModelInformation(&info);
      if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT)
      {
        switch (info.deviceId)
        {
        case GAMING_DEVICE_DEVICE_ID_XBOX_ONE:
        case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_S:
          wsi.render_width = 1920;
          wsi.render_height = 1080;
          break;

        case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X:
        case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X_DEVKIT:
        default:  // Forward compatibility
          wsi.render_width = 3840;
          wsi.render_height = 2160;
          break;
        }
      }

      std::unique_ptr<BootParameters> boot = BootParameters::GenerateFromFile(
          winrt::to_string(file.Path().data()), BootSessionData("", DeleteSavestateAfterBoot::No));

      UICommon::SetUserDirectory(winrt::to_string(ApplicationData::Current().LocalFolder().Path()));
      UICommon::CreateDirectories();
      UICommon::Init();
      UICommon::InitControllers(wsi);

      if (!BootManager::BootCore(std::move(boot), wsi))
      {
        fprintf(stderr, "Could not boot the specified file\n");
      }
    }
  }

  void SetWindow(CoreWindow const& w) {}

  void OnActivate(const winrt::Windows::ApplicationModel::Core::CoreApplicationView&,
                  const winrt::Windows::ApplicationModel::Activation::IActivatedEventArgs& args)
  {
    CoreWindow window = CoreWindow::GetForCurrentThread();
    window.Activate();
  }
};

int WINAPIV WinMain()
{
  winrt::init_apartment();

  CoreApplication::Run(make<App>());

  winrt::uninit_apartment();

  return 0;
}

void ExitSample() noexcept
{
  winrt::Windows::ApplicationModel::Core::CoreApplication::Exit();
}

void UpdateRunningFlag()
{
  if (m_shutdown_requested.TestAndClear())
  {
    const auto ios = IOS::HLE::GetIOS();
    const auto stm = ios ? ios->GetDeviceByName("/dev/stm/eventhook") : nullptr;
    if (!m_tried_graceful_shutdown.IsSet() && stm &&
        std::static_pointer_cast<IOS::HLE::STMEventHookDevice>(stm)->HasHookInstalled())
    {
      ProcessorInterface::PowerButton_Tap();
      m_tried_graceful_shutdown.Set();
    }
    else
    {
      m_running.Clear();
    }
  }
}

std::vector<std::string> Host_GetPreferredLocales()
{
  return {};
}

void Host_NotifyMapLoaded()
{
}

void Host_RefreshDSPDebuggerWindow()
{
}

bool Host_UIBlocksControllerState()
{
  return false;
}

void Host_Message(HostMessageID id)
{
}

void Host_UpdateTitle(const std::string& title)
{
}

void Host_UpdateDisasmDialog()
{
}

void Host_UpdateMainFrame()
{
}

void Host_RequestRenderWindowSize(int width, int height)
{
}

bool Host_RendererHasFocus()
{
  return true;
}

bool Host_RendererHasFullFocus()
{
  // Mouse capturing isn't implemented
  return Host_RendererHasFocus();
}

bool Host_RendererIsFullscreen()
{
  return true;
}

void Host_YieldToUI()
{
}

void Host_TitleChanged()
{
}

void Host_UpdateDiscordClientID(const std::string& client_id)
{
}

bool Host_UpdateDiscordPresenceRaw(const std::string& details, const std::string& state,
                                   const std::string& large_image_key,
                                   const std::string& large_image_text,
                                   const std::string& small_image_key,
                                   const std::string& small_image_text,
                                   const int64_t start_timestamp, const int64_t end_timestamp,
                                   const int party_size, const int party_max)
{
  return false;
}

std::unique_ptr<GBAHostInterface> Host_CreateGBAHost(std::weak_ptr<HW::GBA::Core> core)
{
  return nullptr;
}
