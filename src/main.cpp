#include "app.h"
#include "platform_win.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <d3d11.h>
#include <shellapi.h>
#include <tchar.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static UINT g_resizeWidth = 0, g_resizeHeight = 0;
static AppState* g_app = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void PresentFrame(const float clear_color[4], bool vsync);
float EffectiveDpiScale(HWND hwnd);

static void handle_drop(HDROP drop) {
  if (!g_app) return;
  const UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
  std::vector<std::filesystem::path> paths;
  for (UINT i = 0; i < n; ++i) {
    wchar_t buf[MAX_PATH];
    if (DragQueryFileW(drop, i, buf, MAX_PATH)) paths.emplace_back(buf);
  }
  DragFinish(drop);
  g_app->add_paths(paths);
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
  switch (msg) {
    case WM_DROPFILES:
      handle_drop(reinterpret_cast<HDROP>(lParam));
      return 0;
    case WM_SIZE:
      if (wParam == SIZE_MINIMIZED) return 0;
      g_resizeWidth = LOWORD(lParam);
      g_resizeHeight = HIWORD(lParam);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  ImGui_ImplWin32_EnableDpiAwareness();
  static const HBRUSH k_dark = CreateSolidBrush(RGB(26, 38, 41));
  WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr, nullptr, k_dark,
                    nullptr, L"QingtuCpp", nullptr};
  RegisterClassExW(&wc);
  HWND hwnd = CreateWindowW(wc.lpszClassName, L"轻图", WS_OVERLAPPEDWINDOW, 100, 100, 560, 720,
                            nullptr, nullptr, wc.hInstance, nullptr);
  DragAcceptFiles(hwnd, TRUE);

  if (!CreateDeviceD3D(hwnd)) {
    CleanupDeviceD3D();
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 1;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  const float dpi = EffectiveDpiScale(hwnd);
  load_cjk_fonts(&io, 22.f * dpi, 34.f * dpi);
  apply_qingtu_theme(dpi * 1.1f);
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

  AppState state;
  g_app = &state;

  bool done = false;
  ShowWindow(hwnd, SW_SHOWDEFAULT);
  while (!done) {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (msg.message == WM_QUIT) done = true;
    }
    if (done) break;
    if (g_resizeWidth && g_resizeHeight) {
      CleanupRenderTarget();
      g_pSwapChain->ResizeBuffers(0, g_resizeWidth, g_resizeHeight, DXGI_FORMAT_UNKNOWN, 0);
      g_resizeWidth = g_resizeHeight = 0;
      CreateRenderTarget();
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    draw_ui(state);
    ImGui::Render();
    const float clear[4] = {0.10f, 0.15f, 0.16f, 1.f};
    PresentFrame(clear, true);
  }

  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  CleanupDeviceD3D();
  DestroyWindow(hwnd);
  UnregisterClassW(wc.lpszClassName, wc.hInstance);
  CoUninitialize();
  return 0;
}

bool CreateDeviceD3D(HWND hWnd) {
  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferCount = 2;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
  if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
                                    D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr,
                                    &g_pd3dDeviceContext) != S_OK)
    return false;
  CreateRenderTarget();
  return true;
}

void CleanupDeviceD3D() {
  CleanupRenderTarget();
  if (g_pSwapChain) g_pSwapChain->Release();
  if (g_pd3dDeviceContext) g_pd3dDeviceContext->Release();
  if (g_pd3dDevice) g_pd3dDevice->Release();
  g_pSwapChain = nullptr;
  g_pd3dDeviceContext = nullptr;
  g_pd3dDevice = nullptr;
}

void CreateRenderTarget() {
  ID3D11Texture2D* buf = nullptr;
  g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&buf));
  g_pd3dDevice->CreateRenderTargetView(buf, nullptr, &g_mainRenderTargetView);
  buf->Release();
}

void CleanupRenderTarget() {
  if (g_mainRenderTargetView) {
    g_mainRenderTargetView->Release();
    g_mainRenderTargetView = nullptr;
  }
}

float EffectiveDpiScale(HWND hwnd) {
  float s = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
  if (s <= 0.f) s = 1.f;
  return s;
}

void PresentFrame(const float clear_color[4], bool vsync) {
  g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
  g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  g_pSwapChain->Present(vsync ? 1 : 0, 0);
}
