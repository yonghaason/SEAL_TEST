#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

bool save_stringstream_to_file(const std::string& filename, const std::stringstream& ss) {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) return false;

    const std::string data = ss.str();
    ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(ofs);
}

bool load_file_to_stringstream(const std::string& filename, std::stringstream& ss_out) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) return false;

    ss_out.str(std::string());
    ss_out.clear();

    ss_out << ifs.rdbuf();
    return static_cast<bool>(ifs) || ifs.eof();
}
