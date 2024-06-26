#pragma once

#include <filesystem>

namespace fs = std::filesystem;

bool LoadGeometry(const fs::path &path, std::vector<float> &pointData, std::vector<uint16_t> &indexData);