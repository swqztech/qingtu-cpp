#include "platform_win.h"

#include "imgui.h"

#include <windows.h>

#include <filesystem>
#include <string>

ImFont* g_font_body = nullptr;
ImFont* g_font_title = nullptr;

static std::string wide_path_to_utf8(const std::wstring& wpath) {
  const int u8len = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (u8len <= 0) return {};
  std::string path_u8(static_cast<size_t>(u8len), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, path_u8.data(), u8len, nullptr, nullptr);
  return path_u8;
}

static ImFont* add_font_from_system(ImGuiIO* io, float size_px) {
  const wchar_t* windir = _wgetenv(L"WINDIR");
  std::wstring fonts_dir = windir ? std::wstring(windir) + L"\\Fonts" : L"C:\\Windows\\Fonts";
  const wchar_t* candidates[] = {L"msyh.ttc", L"msyhbd.ttc", L"simhei.ttf"};
  for (const wchar_t* name : candidates) {
    std::filesystem::path path = std::filesystem::path(fonts_dir) / name;
    if (!std::filesystem::exists(path)) continue;
    const std::string path_u8 = wide_path_to_utf8(path.wstring());
    ImFontConfig cfg;
    cfg.OversampleH = cfg.OversampleV = 2;
    ImFont* font = io->Fonts->AddFontFromFileTTF(
        path_u8.c_str(), size_px, &cfg, io->Fonts->GetGlyphRangesChineseFull());
    if (font) return font;
  }
  return io->Fonts->AddFontDefault();
}

bool load_cjk_fonts(ImGuiIO* io, float body_px, float title_px) {
  g_font_body = add_font_from_system(io, body_px);
  g_font_title = add_font_from_system(io, title_px);
  if (g_font_body) io->FontDefault = g_font_body;
  return g_font_body != nullptr;
}
