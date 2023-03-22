#include "WinRTKeyboard.h"

#include <winrt/Windows.UI.ViewManagement.Core.h>
#pragma optimize("", off)
namespace UWP
{
std::vector<uint32_t> g_char_buffer;
std::mutex g_buffer_mutex;

bool IsKeyboardShowing()
{
  return false;
}

void ShowKeyboard()
{
  winrt::Windows::UI::ViewManagement::Core::CoreInputView::GetForCurrentView().TryShowPrimaryView();
}

void HandleCharacter(uint32_t keycode)
{
  //if (keycode == 0x08 /* backspace */)
  //{
  //  g_char_buffer.push_back('\0');
  //}
  //else
  //{

  std::unique_lock lk(g_buffer_mutex);
  g_char_buffer.push_back(keycode);
}
}  // namespace UWP
#pragma optimize("", on)
