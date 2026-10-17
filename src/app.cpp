#include "app.h"

#include "dialogs.h"
#include "platform_win.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace fs = std::filesystem;

static bool is_image(const fs::path& p) {
  auto e = p.extension().string();
  if (!e.empty() && e[0] == '.') e.erase(0, 1);
  for (auto& c : e) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return e == "png" || e == "jpg" || e == "jpeg" || e == "webp" || e == "bmp" || e == "gif";
}

void AppState::add_paths(const std::vector<fs::path>& paths) {
  for (const auto& p : paths) {
    if (!is_image(p)) continue;
    if (std::find(files.begin(), files.end(), p) == files.end()) files.push_back(p);
  }
}

void AppState::run_compress() {
  if (files.empty()) {
    log = "请先添加图片";
    return;
  }
  JobOptions opts;
  opts.quality = quality;
  opts.max_side = max_side;
  opts.format = format;
  opts.use_custom_out = custom_out;
  if (custom_out && output_dir[0]) opts.output_dir = fs::path(output_dir);
  std::ostringstream oss;
  for (const auto& r : run_compress_jobs(files, opts)) {
    oss << r.input.filename().string() << " → ";
    if (r.output.empty()) {
      oss << r.message << "\n";
    } else {
      oss << r.output.filename().string() << " (" << r.bytes_before << " → " << r.bytes_after
          << " B)\n";
    }
  }
  log = oss.str();
}

void AppState::run_stitch() {
  if (files.size() < 2) {
    log = "拼接需要至少 2 张图";
    return;
  }
  fs::path out;
  if (custom_out && output_dir[0]) {
    out = fs::path(output_dir) / "stitched.png";
  } else if (!files.empty()) {
    out = files[0].parent_path() / "compressed" / "stitched.png";
  } else {
    out = "stitched.png";
  }
  std::string err;
  if (stitch_horizontal(files, out, err)) {
    log = std::string("拼接完成 → ") + out.string();
  } else {
    log = err;
  }
}

void apply_qingtu_theme(float ui_scale) {
  ImGuiStyle& s = ImGui::GetStyle();
  s.ScaleAllSizes(ui_scale);
  ImVec4* c = s.Colors;
  c[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.15f, 0.16f, 1.f);
  c[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.18f, 0.20f, 1.f);
  c[ImGuiCol_Button] = ImVec4(0.90f, 0.55f, 0.28f, 1.f);
  c[ImGuiCol_ButtonHovered] = ImVec4(0.95f, 0.62f, 0.35f, 1.f);
  c[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.77f, 0.71f, 1.f);
}

void draw_ui(AppState& state) {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
  ImGui::Begin("qingtu", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
  if (g_font_title) ImGui::PushFont(g_font_title);
  ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("轻图").x) * 0.5f);
  ImGui::TextColored(ImVec4(0.28f, 0.77f, 0.71f, 1.f), "轻图");
  if (g_font_title) ImGui::PopFont();
  ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("把体积压下去，细节留住").x) * 0.5f);
  ImGui::TextDisabled("把体积压下去，细节留住");
  ImGui::Spacing();

  if (ImGui::Button("压缩", ImVec2(72, 30))) state.tab = 0;
  ImGui::SameLine();
  if (ImGui::Button("拼接", ImVec2(72, 30))) state.tab = 1;
  ImGui::Spacing();

  ImGui::BeginChild("drop", ImVec2(-1, 90), true);
  ImGui::TextDisabled("拖拽图片到窗口，或点击下方按钮");
  if (ImGui::Button("选择图片")) {
    std::vector<fs::path> picked;
    if (pick_image_files(picked)) state.add_paths(picked);
  }
  if (!state.files.empty()) {
    ImGui::Text("已选 %zu 个文件", state.files.size());
  }
  ImGui::EndChild();

  if (state.tab == 0) {
    ImGui::SliderInt("质量", &state.quality, 1, 100);
    ImGui::SliderInt("最长边 (0=不缩放)", &state.max_side, 0, 8192);
    ImGui::TextDisabled("输出格式");
    if (ImGui::RadioButton("保持原格式", state.format == OutputFormat::KeepOriginal))
      state.format = OutputFormat::KeepOriginal;
    ImGui::SameLine();
    if (ImGui::RadioButton("JPEG", state.format == OutputFormat::Jpeg))
      state.format = OutputFormat::Jpeg;
    ImGui::SameLine();
    if (ImGui::RadioButton("PNG", state.format == OutputFormat::Png)) state.format = OutputFormat::Png;
    ImGui::SameLine();
    if (ImGui::RadioButton("WebP", state.format == OutputFormat::WebP))
      state.format = OutputFormat::WebP;
    ImGui::TextDisabled("WebP 导出请使用 Rust 版 qingtu");
  } else {
    ImGui::TextDisabled("按顺序横向拼接，高度对齐最短边");
  }

  ImGui::Checkbox("自定义输出目录", &state.custom_out);
  ImGui::InputText("##out", state.output_dir, sizeof(state.output_dir));
  ImGui::SameLine();
  if (ImGui::Button("输出目录")) {
    std::string folder;
    if (pick_output_folder(folder)) {
      strncpy(state.output_dir, folder.c_str(), sizeof(state.output_dir) - 1);
      state.custom_out = true;
    }
  }
  if (!state.custom_out) ImGui::TextDisabled("默认：源文件旁 /compressed");

  const char* btn = state.tab == 0 ? "开始压缩" : "开始拼接";
  char label[64];
  snprintf(label, sizeof(label), "%s (%zu)", btn, state.files.size());
  if (ImGui::Button(label, ImVec2(140, 36))) {
    if (state.tab == 0) state.run_compress();
    else state.run_stitch();
  }
  ImGui::SameLine();
  if (ImGui::Button("清空列表")) {
    state.files.clear();
    state.log.clear();
  }

  if (!state.log.empty()) {
    ImGui::BeginChild("log", ImVec2(-1, 100), true);
    ImGui::TextWrapped("%s", state.log.c_str());
    ImGui::EndChild();
  }
  ImGui::End();
}
