#pragma once

#include <d3d11.h>
#include <dxgi1_4.h>

class D3DWindow
{
  ID3D11Device* g_pd3dDevice = NULL;
  ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
  IDXGISwapChain1* g_pSwapChain = NULL;
  ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

public:
  bool CreateDeviceD3D();
  void CleanupDeviceD3D();
  void CreateRenderTarget();
  void CleanupRenderTarget();
  void InitImGui();
  void DeInitImGui();

  void Render();

  void ResizeSwapChain(uint32_t width, uint32_t height);
  bool CreateTextureFromBuffer(uint32_t width, uint32_t height, uint8_t* buffer,
                                          size_t size, ID3D11ShaderResourceView** out_srv);
};
