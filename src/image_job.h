#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

enum class OutputFormat { KeepOriginal, Jpeg, Png, WebP };

struct JobOptions {
  int quality = 80;
  int max_side = 0;
  OutputFormat format = OutputFormat::KeepOriginal;
  std::filesystem::path output_dir;
  bool use_custom_out = false;
};

struct JobResult {
  std::filesystem::path input;
  std::filesystem::path output;
  std::uint64_t bytes_before = 0;
  std::uint64_t bytes_after = 0;
  std::string message;
};

std::vector<JobResult> run_compress_jobs(const std::vector<std::filesystem::path>& inputs,
                                         const JobOptions& opts);
bool stitch_horizontal(const std::vector<std::filesystem::path>& inputs,
                       const std::filesystem::path& output, std::string& err);
