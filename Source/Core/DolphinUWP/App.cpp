#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.Storage.h>

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

        CoreDispatcher dispatcher = window.Dispatcher();
        //dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);

        while (m_running.IsSet())
        {
          dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
          ::Core::HostDispatchJobs();

          // TODO: Is this sleep appropriate?
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void SetWindow(CoreWindow const & w)
    {
        void* window = winrt::get_abi(CoreWindow::GetForCurrentThread());

        WindowSystemInfo wsi;
        wsi.type = WindowSystemType::UWP;
        wsi.render_window = window;
        wsi.render_surface = window;
        wsi.render_height = w.Bounds().Height;
        wsi.render_width = w.Bounds().Width;

        std::unique_ptr<BootParameters> boot =
          BootParameters::GenerateFromFile("E:/game.iso",
                                             BootSessionData("", DeleteSavestateAfterBoot::No));

        UICommon::SetUserDirectory(
            winrt::to_string(ApplicationData::Current().LocalFolder().Path()));
        UICommon::Init();
        UICommon::InitControllers(wsi);

        if (!BootManager::BootCore(std::move(boot), wsi))
        {
           fprintf(stderr, "Could not boot the specified file\n");
           return;
        }
    }

    void OnPointerPressed(IInspectable const &, PointerEventArgs const & args)
    {
        /*float2 const point = args.CurrentPoint().Position();

        for (Visual visual : m_visuals)
        {
            float3 const offset = visual.Offset();
            float2 const size = visual.Size();

            if (point.x >= offset.x &&
                point.x < offset.x + size.x &&
                point.y >= offset.y &&
                point.y < offset.y + size.y)
            {
                m_selected = visual;
                m_offset.x = offset.x - point.x;
                m_offset.y = offset.y - point.y;
            }
        }

        if (m_selected)
        {
            m_visuals.Remove(m_selected);
            m_visuals.InsertAtTop(m_selected);
        }
        else
        {
            AddVisual(point);
        }*/
    }

    void OnPointerMoved(IInspectable const &, PointerEventArgs const & args)
    {
        //if (m_selected)
        //{
        //    float2 const point = args.CurrentPoint().Position();

        //    m_selected.Offset(
        //    {
        //        point.x + m_offset.x,
        //        point.y + m_offset.y,
        //        0.0f
        //    });
        //}
    }

    void AddVisual(float2 const point)
    {
        //Compositor compositor = m_visuals.Compositor();
        //SpriteVisual visual = compositor.CreateSpriteVisual();

        //static Color colors[] =
        //{
        //    { 0xDC, 0x5B, 0x9B, 0xD5 },
        //    { 0xDC, 0xED, 0x7D, 0x31 },
        //    { 0xDC, 0x70, 0xAD, 0x47 },
        //    { 0xDC, 0xFF, 0xC0, 0x00 }
        //};

        //static unsigned last = 0;
        //unsigned const next = ++last % _countof(colors);
        //visual.Brush(compositor.CreateColorBrush(colors[next]));

        //float const BlockSize = 100.0f;

        //visual.Size(
        //{
        //    BlockSize,
        //    BlockSize
        //});

        //visual.Offset(
        //{
        //    point.x - BlockSize / 2.0f,
        //    point.y - BlockSize / 2.0f,
        //    0.0f,
        //});

        //m_visuals.InsertAtTop(visual);

        //m_selected = visual;
        //m_offset.x = -BlockSize / 2.0f;
        //m_offset.y = -BlockSize / 2.0f;
    }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CoreApplication::Run(make<App>());
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
