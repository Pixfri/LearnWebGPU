#include "ModelLoader.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

bool LoadGeometry(const fs::path &path, std::vector<float> &pointData, std::vector<uint16_t> &indexData) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    pointData.clear();
    indexData.clear();

    enum class Section {
        None,
        Points,
        Indices,
    };
    Section currentSection = Section::None;

    float value;
    uint16_t index;
    std::string line;
    while (!file.eof()) {
        std::getline(file, line);

        // Overcome the `CRLF` problem
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        switch (line) {
            case "[points]":
                currentSection = Section::Points;
                break;
            case "[indices]":
                currentSection = Section::Indices;
                break;
            case "#": break; // Do nothing, this is a comment.
            default: std::cout << "Error reading line : " << line << '\n';
        }

        switch(currentSection) {
            case Section::Points:
            {
                std::istringstream iss(line);
                // Get x, y, r, g, b
                for (int i = 0; i < 5; ++i) {
                    iss >> value;
                    pointData.push_back(value);
                }
                break;
            }
            case Section::Indices:
            {
                std::istringstream iss(line);
                // Get corners #0, #1 and #2
                for (int i = 0; i < 3; ++i) {
                    iss >> index;
                    indexData.push_back(index);
                }
                break;
            }
            case Section::None:
                std::cout << "This section doesn't exist or isn't implemented yet.";
                break;
        }
    }
    return true;
}