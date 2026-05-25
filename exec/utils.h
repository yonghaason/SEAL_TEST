#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <vector>

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

std::vector<uint64_t> string_to_ascii_vector(const std::string& s) {
    std::vector<uint64_t> v;
    v.reserve(s.size());

    for (unsigned char c : s) {
        if (c > 127) {
            throw std::invalid_argument("non-ASCII character detected");
        }
        v.push_back(static_cast<uint64_t>(c));
    }

    return v;
}

std::string ascii_vector_to_string(const std::vector<uint64_t>& v) {
    std::string s;
    s.reserve(v.size());

    for (uint64_t x : v) {
        if (x > 127) {
            throw std::invalid_argument("invalid ASCII code");
        }
        s.push_back(static_cast<char>(x));
    }

    return s;
}