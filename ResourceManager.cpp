#include "ResourceManager.h"

#include <fstream>
#include <sstream>
#include <string>

bool ResourceManager::loadGeometry(
    const std::filesystem::path& path,
    std::vector<float>& pointData,
    std::vector<uint16_t>& indexData) {
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
    // Loop on getline() itself rather than on `!file.eof()`: getline returns
    // false as soon as there is nothing left to read, whereas eof() only
    // becomes true AFTER a read has already failed, which would make us
    // process one extra (empty) line at the end.
    while (getline(file, line)) {

        //overcome the `CRLF` problem
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line == "[points]") {
            currentSection = Section::Points;
        }
        else if (line == "[indices]") {
            currentSection = Section::Indices;
        }
        else if (line.empty() || line[0] == '#') {
            //DO NOTHING THE LINE IS EMPTY OR A COMMENT
            //(check empty() first, so we never look at line[0] of an empty line)
        }
        else if (currentSection == Section::Points) {
            std::istringstream iss(line);
            //Get x,y,r,g,b
            for (int i = 0; i < 5; ++i) {
                // `iss >> value` evaluates to false if the next word is not a
                // number or if the line has fewer than 5 values. Before, we
                // pushed whatever was left in `value` and silently loaded a
                // broken mesh. Now we report the file as invalid instead.
                if (!(iss >> value)) {
                    return false;
                }
                pointData.push_back(value);
            }
        }
        else if (currentSection == Section::Indices) {
            std::istringstream iss(line);
            //Get the corners of each triangle...#0 #1 #2
            for (int i = 0; i < 3; ++i) {
                if (!(iss >> index)) {
                    return false;
                }
                indexData.push_back(index);
            }
        }
    }
    return true;
}


    wgpu::ShaderModule ResourceManager::loadShaderModule(const std::filesystem::path& path, wgpu::Device device) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return nullptr;
        }
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        std::string shaderSource(size, ' ');
        file.seekg(0);
        file.read(shaderSource.data(), size);

        //[NEW API] ShaderModuleWGSLDescriptor was renamed ShaderSourceWGSL, and
        //= wgpu::Default now fills chain.sType for us (SType::ShaderSourceWGSL)
        wgpu::ShaderSourceWGSL shaderCodeDesc = wgpu::Default;
        //[NEW API] code is a StringView (pointer + LENGTH), so no more c_str()
        shaderCodeDesc.code = wgpu::StringView(shaderSource);

        wgpu::ShaderModuleDescriptor shaderDesc = wgpu::Default;
        //[NEW API] The wgpu-native-only hintCount/hints fields are gone, and so is the
        //#ifdef WEBGPU_BACKEND_WGPU that was around them
        shaderDesc.nextInChain = &shaderCodeDesc.chain;
        //A label makes shader errors say WHICH file is wrong. Keep the std::string in
        //a named variable: a StringView only POINTS to the characters. (The guide writes
        //StringView(path.string()), which points into a temporary that is already gone.)
        std::string label = path.string();
        shaderDesc.label = wgpu::StringView(label);

        return device.createShaderModule(shaderDesc);
    }
