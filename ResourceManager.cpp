#include "ResourceManager.h"

#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

// REFACTOR (milestone): loadGeometry used to be one 80-line loop. Each kind of
// line is now handled by a small helper, so the loop only decides WHICH helper
// gets the line.
namespace {

enum class Section {
    None,
    Points,
    Indices,
};

// True if the line is blank (empty, or only spaces/tabs) or a # comment.
// Step050: the guide's pyramid.txt ends with a line containing a single
// space. The old check (line.empty()) did not see it as blank, so the guide's
// parser silently added a fake triangle (4, 4, 4) and our stricter parser
// refused the whole file.
bool isBlankOrComment(const std::string& line) {
    size_t firstChar = line.find_first_not_of(" \t");
    return firstChar == std::string::npos || line[firstChar] == '#';
}

// Read exactly `count` numbers of type T from the line, or fail.
// `iss >> value` evaluates to false if the next word is not a number or if the
// line has too few values. Before, we pushed whatever was left in `value` and
// silently loaded a broken mesh. Now we report the file as invalid instead.
template <typename T>
bool readNumbers(const std::string& line, int count, std::vector<T>& output) {
    std::istringstream iss(line);
    T value;
    for (int i = 0; i < count; ++i) {
        if (!(iss >> value)) {
            return false;
        }
        output.push_back(value);
    }
    return true;
}

// Read the whole text file into a string. The old version measured the file
// with tellg() and then read that many characters, but on Windows a text-mode
// read turns each "\r\n" into "\n", so fewer characters arrive than measured.
// Reading until the end of the stream gets exactly what is there.
bool readTextFile(const std::filesystem::path& path, std::string& text) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

} // namespace

bool ResourceManager::loadGeometry(
    const std::filesystem::path& path,
    std::vector<float>& pointData,
    std::vector<uint16_t>& indexData,
    int dimensions) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    pointData.clear();
    indexData.clear();

    Section currentSection = Section::None;
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
        bool ok = true;
        if (line == "[points]") currentSection = Section::Points;
        else if (line == "[indices]") currentSection = Section::Indices;
        else if (isBlankOrComment(line)) continue;
        //Points: x,y(,z),r,g,b = `dimensions` coordinates then 3 color values
        else if (currentSection == Section::Points) ok = readNumbers(line, dimensions + 3, pointData);
        //Indices: the corners of each triangle...#0 #1 #2
        else if (currentSection == Section::Indices) ok = readNumbers(line, 3, indexData);
        if (!ok) {
            return false;
        }
    }
    return true;
}

wgpu::ShaderModule ResourceManager::loadShaderModule(const std::filesystem::path& path, wgpu::Device device) {
    std::string shaderSource;
    if (!readTextFile(path, shaderSource)) {
        return nullptr;
    }
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
