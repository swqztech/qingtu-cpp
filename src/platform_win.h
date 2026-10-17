#pragma once

struct ImFont;
struct ImGuiIO;

extern ImFont* g_font_body;
extern ImFont* g_font_title;

bool load_cjk_fonts(ImGuiIO* io, float body_px, float title_px);
