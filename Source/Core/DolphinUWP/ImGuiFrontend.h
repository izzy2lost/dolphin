#pragma once

#include "D3DWindow.h"

namespace UICommon
{
class GameFile;
}

class WGDevice;

namespace ImGuiFrontend
{
class UIState;

class ImGuiFrontend
{
public:
  ImGuiFrontend();
  std::shared_ptr<UICommon::GameFile> RunUntilSelection();

private:
  void PopulateControls();
  void RefreshControls(bool updateGameSelection);

  std::shared_ptr<UICommon::GameFile> RunMainLoop();
  std::shared_ptr<UICommon::GameFile> CreateMainPage();
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
