#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>

#include <Common/WindowSystemInfo.h>
#include <Core/BootManager.h>
#include <UICommon/UICommon.h>
#include <Core/Boot/Boot.h>
#include <Core/Host.h>
#include <Core/Core.h>
#include <Core/IOS/STM/STM.h>
#include <Core/HW/ProcessorInterface.h>

using namespace winrt;

using namespace Windows;
using namespace Windows::Storage;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Pickers;


Common::Flag m_running{true};
Common::Flag m_shutdown_requested{false};
Common::Flag m_tried_graceful_shutdown{false};

struct App : implements<App, IFrameworkViewSource, IFrameworkView>
{
    IFrameworkView CreateView()
    {
        return *this;
    }

    void Initialize(CoreApplicationView const &)
    {
    }

    void Load(hstring const&)
    {
    }

    void Uninitialize()
    {
    }

    void Run()
    {
        CoreWindow window = CoreWindow::GetForCurrentThread();
        window.Activate();

        while (m_running.IsSet())
        {
          window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
          ::Core::HostDispatchJobs();
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void SetWindow(CoreWindow const & w)
    {
        InitializeDolphin(w.Bounds().Width, w.Bounds().Height);
    }

    winrt::fire_and_forget InitializeDolphin(float window_width, float window_height)
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
            // - we can copy the game to use a file from anywhere, but that isn't great for huge games
            void* window = winrt::get_abi(CoreWindow::GetForCurrentThread());

            WindowSystemInfo wsi;
            wsi.type = WindowSystemType::UWP;
            wsi.render_window = window;
            wsi.render_surface = window;
            wsi.render_height = window_height;
            wsi.render_width = window_width;

            std::unique_ptr<BootParameters> boot = BootParameters::GenerateFromFile(
                winrt::to_string(file.Path().data()), BootSessionData("", DeleteSavestateAfterBoot::No));

            UICommon::SetUserDirectory(
                winrt::to_string(ApplicationData::Current().LocalFolder().Path()));
            UICommon::Init();
            UICommon::InitControllers(wsi);

            if (!BootManager::BootCore(std::move(boot), wsi))
            {
                fprintf(stderr, "Could not boot the specified file\n");
                DeleteFile(file.Path().data());
            }
        }
    }

    void OnPointerPressed(IInspectable const &, PointerEventArgs const & args) { }

    void OnPointerMoved(IInspectable const &, PointerEventArgs const & args) { }
};

int WINAPIV WinMain()
{
    winrt::init_apartment();

    SetThreadAffinityMask(GetCurrentThread(), 0x1);
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
    return false;
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
