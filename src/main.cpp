#include "app.h"
#include "platform_win.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <d3d11.h>
#include <shellapi.h>
#include <filesystem>
#include <tchar.h>
#include <vector>

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
      g_resizeWidth = static_cast<UINT>(LOWORD(lParam));
      g_resizeHeight = static_cast<UINT>(HIWORD(lParam));
      return 0;
    case WM_SYSCOMMAND:
      if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
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
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;
  const float dpi = EffectiveDpiScale(hwnd);
  load_cjk_fonts(&io, 24.f * dpi, 36.f * dpi);
  apply_qingtu_theme(dpi * 1.15f);

  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
  ImGui_ImplDX11_CreateDeviceObjects();

  AppState state;
  g_app = &state;

  // 首帧绘制完成后再显示，避免空白/闪屏
  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  draw_ui(state);
  ImGui::Render();
  const float boot_clear[4] = {0.10f, 0.15f, 0.16f, 1.f};
  PresentFrame(boot_clear, false);

  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);

  bool done = false;
  while (!done) {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (msg.message == WM_QUIT) done = true;
    }
    if (done) break;

    if (g_resizeWidth > 0 && g_resizeHeight > 0) {
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

  g_app = nullptr;
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  CleanupDeviceD3D();
  DestroyWindow(hwnd);
  UnregisterClassW(wc.lpszClassName, wc.hInstance);
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
  float scale = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
  if (scale <= 0.f) scale = 1.f;
  const UINT dpi = GetDpiForWindow(hwnd);
  if (dpi > 0) {
    const float from_hwnd = static_cast<float>(dpi) / 96.f;
    if (from_hwnd > scale) scale = from_hwnd;
  }
  if (scale < 1.f) scale = 1.f;
  return scale;
}

void PresentFrame(const float clear_color[4], bool vsync) {
  if (!g_mainRenderTargetView) return;
  g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
  g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
  if (ImDrawData* dd = ImGui::GetDrawData()) {
    if (dd->Valid) ImGui_ImplDX11_RenderDrawData(dd);
  }
  g_pSwapChain->Present(vsync ? 1 : 0, 0);
}
