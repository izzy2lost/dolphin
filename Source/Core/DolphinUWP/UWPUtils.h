#pragma once

#include <Common/FileUtil.h>
#include <Core/HW/Wiimote.h>

#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>

#include <ppltasks.h>
#include <Core/HW/WiimoteEmu/WiimoteEmu.h>

using namespace winrt;
using namespace winrt::Windows::Storage::Pickers;
using namespace Windows::ApplicationModel::Core;

namespace UWP
{
inline std::string GetUserLocation()
{
  std::string user_path = 
      winrt::to_string(winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path()) +
      "/user.txt";
  if (File::Exists(user_path))
  {
    std::ifstream t(user_path);
    std::stringstream buffer;
    buffer << t.rdbuf();

    return buffer.str();
  }
  else
  {
    return winrt::to_string(
        winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path());
  }
}

#pragma warning(push)
#pragma warning(disable : 4265)
inline winrt::fire_and_forget OpenNewUserPicker()
{
  std::string user_path =
      winrt::to_string(winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path()) +
      "/user.txt";
  FolderPicker openPicker;
  openPicker.ViewMode(PickerViewMode::List);
  openPicker.SuggestedStartLocation(PickerLocationId::ComputerFolder);

  auto folder = co_await openPicker.PickSingleFolderAsync();
  if (folder)
  {
    std::ofstream t(user_path);
    t << winrt::to_string(folder.Path().data());
  }
}


inline void SetNewUserLocation()
{
  CoreApplication::MainView().CoreWindow().Dispatcher().RunAsync(
      winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, []
      {
        OpenNewUserPicker();
      });
}
#pragma warning(pop)

inline void ResetUserLocation()
{
  std::string user_path =
      winrt::to_string(winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path()) +
      "/user.txt";

  std::ofstream t(user_path);
  t << winrt::to_string(winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path());
}
} // namespace UWP
