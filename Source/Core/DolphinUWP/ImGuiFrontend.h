#pragma once

#include "D3DWindow.h"
#include "Core/TitleDatabase.h"

namespace UICommon
{
class GameFile;
}

class WGDevice;

namespace ImGuiFrontend
{
class UIState;

class FrontendResult
{
public:
  std::shared_ptr<UICommon::GameFile> game_result;
  bool netplay;

  FrontendResult() {
    game_result = nullptr;
    netplay = false;
  }

  FrontendResult(std::shared_ptr<UICommon::GameFile> game)
  {
    game_result = game;
    netplay = false;
  }
};

class ImGuiFrontend
{
public:
  ImGuiFrontend();
  FrontendResult RunUntilSelection();
  Core::TitleDatabase m_title_database;

private:
  void PopulateControls();
  void RefreshControls(bool updateGameSelection);

  FrontendResult RunMainLoop();
  FrontendResult CreateMainPage();
  std::shared_ptr<UICommon::GameFile> CreateGameList();

  void CreateGeneralTab(UIState* state);
  void CreateInterfaceTab(UIState* state);
  void CreateGraphicsTab(UIState* state);
  void CreateGameCubeTab(UIState* state);
  void CreateWiiTab(UIState* state);
  void CreateAdvancedTab(UIState* state);
  void CreatePathsTab(UIState* state);

  void LoadGameList();
  void RecurseFolder(std::string path);
  void AddGameFolder(std::string path);

  ID3D11ShaderResourceView* GetOrCreateBackgroundTex();
  ID3D11ShaderResourceView* GetOrCreateMissingTex();
  ID3D11ShaderResourceView* GetHandleForGame(std::shared_ptr<UICommon::GameFile> game);
  ID3D11ShaderResourceView* CreateCoverTexture(std::shared_ptr<UICommon::GameFile> game);
};
}  // namespace ImGuiFrontend
