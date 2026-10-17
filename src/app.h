#pragma once

#include "image_job.h"

#include <filesystem>
#include <string>
#include <vector>

struct AppState {
  int tab = 0;
  std::vector<std::filesystem::path> files;
  int quality = 80;
  int max_side = 0;
  OutputFormat format = OutputFormat::KeepOriginal;
  char output_dir[512]{};
  bool custom_out = false;
  std::string log;

  void add_paths(const std::vector<std::filesystem::path>& paths);
  void run_compress();
  void run_stitch();
};

void apply_qingtu_theme(float ui_scale = 1.f);
void draw_ui(AppState& state);
