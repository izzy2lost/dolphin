#pragma once

#include "D3DWindow.h"

#include <dxgidebug.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <gamingdeviceinformation.h>

#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/windows.graphics.display.core.h>

using winrt::Windows::UI::Core::CoreWindow;

bool D3DWindow::CreateDeviceD3D()
{
  D3D_FEATURE_LEVEL featureLevel;
  const D3D_FEATURE_LEVEL featureLevelArray[2] = {
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_0,
  };

  HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevelArray, 2,
                    D3D11_SDK_VERSION, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);

  if (FAILED(hr))
    return false;


  IDXGIFactory2* pFactory;
  hr = CreateDXGIFactory2(0, __uuidof(IDXGIFactory1), (void**)(&pFactory));

  if (FAILED(hr))
    return false;

  DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};

  GAMING_DEVICE_MODEL_INFORMATION info = {};
  GetGamingDeviceModelInformation(&info);
  if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT)
  {
    winrt::Windows::Graphics::Display::Core::HdmiDisplayInformation hdi =
        winrt::Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
    if (hdi)
    {
      swap_chain_desc.Width = hdi.GetCurrentDisplayMode().ResolutionWidthInRawPixels();
      swap_chain_desc.Height = hdi.GetCurrentDisplayMode().ResolutionHeightInRawPixels();
    }
  }

  swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swap_chain_desc.SampleDesc.Count = 1;
  swap_chain_desc.BufferCount = 3;
  swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

  CoreWindow window = CoreWindow::GetForCurrentThread();
  void* abi = winrt::get_abi(window);

  hr = pFactory->CreateSwapChainForCoreWindow(g_pd3dDevice, static_cast<IUnknown*>(abi),
                                              &swap_chain_desc, nullptr, &g_pSwapChain);
  if (FAILED(hr))
  {
    return false;
  }

  CreateRenderTarget();

  return true;
}

void D3DWindow::CleanupDeviceD3D()
{
  CleanupRenderTarget();
  if (g_pSwapChain)
  {
    g_pSwapChain->Release();
    g_pSwapChain = NULL;
  }
  if (g_pd3dDeviceContext)
  {
    g_pd3dDeviceContext->Release();
    g_pd3dDeviceContext = NULL;
  }
  if (g_pd3dDevice)
  {
    g_pd3dDevice->Release();
    g_pd3dDevice = NULL;
  }
}

void D3DWindow::CreateRenderTarget()
{
  ID3D11Texture2D* pBackBuffer;
  g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
  g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
  pBackBuffer->Release();
}

void D3DWindow::CleanupRenderTarget()
{
  if (g_mainRenderTargetView)
  {
    g_mainRenderTargetView->Release();
    g_mainRenderTargetView = NULL;
  }
}

void D3DWindow::InitImGui()
{
  ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
}

void D3DWindow::DeInitImGui()
{
  ImGui_ImplDX11_Shutdown();
  ImGui::DestroyContext();
}

void D3DWindow::Render()
{
  ImVec4 clear_color = ImVec4(0.27f, 0.27f, 0.27f, 0.00f);
  ImGui::Render();
  const float clear_color_with_alpha[4] = {clear_color.x * clear_color.w,
                                           clear_color.y * clear_color.w,
                                           clear_color.z * clear_color.w, clear_color.w};
  g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
  g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

  g_pSwapChain->Present(1, 0);  // Present with vsync
}

void D3DWindow::ResizeSwapChain(uint32_t width, uint32_t height)
{
  HRESULT result = g_pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN,
                                               DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
  assert(SUCCEEDED(result) && "Failed to resize swapchain.");
}

bool D3DWindow::CreateTextureFromBuffer(uint32_t width, uint32_t height,
                                                               uint8_t* buffer, size_t size,
                                                               ID3D11ShaderResourceView** out_srv)
{
  // Create texture
  D3D11_TEXTURE2D_DESC desc;
  ZeroMemory(&desc, sizeof(desc));
  desc.Width = width;
  desc.Height = height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  desc.CPUAccessFlags = 0;

  ID3D11Texture2D* pTexture = NULL;
  D3D11_SUBRESOURCE_DATA subResource;
  subResource.pSysMem = buffer;
  subResource.SysMemPitch = desc.Width * 4;
  subResource.SysMemSlicePitch = 0;
  g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

  // Create texture view
  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
  ZeroMemory(&srvDesc, sizeof(srvDesc));
  srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = desc.MipLevels;
  srvDesc.Texture2D.MostDetailedMip = 0;
  g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
  pTexture->Release();
 
  return true;
}
