#pragma once

#include <filesystem>
#include <string>
#include <vector>

bool pick_image_files(std::vector<std::filesystem::path>& out);
bool pick_output_folder(std::string& utf8_path);
