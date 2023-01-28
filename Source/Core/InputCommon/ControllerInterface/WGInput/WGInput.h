// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <winrt/windows.gaming.input.h>
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface::WGInput
{
void Init();
void DeInit();
void PopulateDevices();
std::shared_ptr<ciface::Core::Device> CreateDevice(const winrt::Windows::Gaming::Input::RawGameController& raw_game_controller);
}  // namespace ciface::WGInput
