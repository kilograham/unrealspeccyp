/*
 * Copyright (c) 2023 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdio>
#include <exception>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <vector>

#define MINIZ_NO_STDIO
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_TIME
#define MINIZ_NO_ZLIB_APIS
#define MINIZ_NO_MALLOC
#include "miniz.h"
#include "khan/games/games.h"

#define ERROR_ARGS -1
#define ERROR_UNKNOWN -2
#define ERROR_SYNTAX -3
#define ERROR_FILE -3

using std::string;
using std::vector;
using std::filesystem::path;

string root_dir;
bool is_game;
bool is_rom;

vector<uint8_t> compress(const vector<uint8_t>& raw);

static string hex_string(int value, int width = 8, bool prefix = true) {
    std::stringstream ss;
    if (prefix) ss << "0x";
    ss << std::setfill('0') << std::setw(width) << std::hex << value;
    return ss.str();
}

struct failure : std::exception {
    failure(int code, string s) : c(code), s(std::move(s)) {}

    const char *what() const noexcept override {
        return s.c_str();
    }

    int code() const { return c; }

private:
    int c;
    string s;
};

static int usage() {
    fprintf(stderr, "Usage: embed_tool [-d | -r] -p <root_path> <description file> <output c file>\n");
    return ERROR_ARGS;
}

string trim_left(const string &str) {
    const string pattern = " \f\n\r\t\v";
    auto n = str.find_first_not_of(pattern);
    if (n == string::npos) return str;
    return str.substr(n);
}

string trim_right(const string &str) {
    const string pattern = " \f\n\r\t\v";
    auto n = str.find_last_not_of(pattern);
    if (n == string::npos) return str;
    return str.substr(0,  n+ 1);
}

string trim(const string &str) {
    return trim_left(trim_right(str));
}

void doit(std::ifstream &in, std::ofstream &out) {
    string entity = is_game ? "game" : "rom";

    string line;
    auto dump = [&](vector<uint8_t> data) {
        for (uint i = 0; i < data.size(); i += 32) {
            out << "   ";
            for (uint j = i; j < std::min(i + 32, (uint) data.size()); j++) {
                out << hex_string(data[j], 2) << ", ";
            }
            out << "\n";
        }
    };

    std::vector<std::tuple<string, string, uint, uint>> games;

    out << "#include <stdio.h>\n";
    out << "#include \"../std.h\"\n";
    out << "#include \"" << entity << "s/" << entity << "s.h\"\n";

    int default_index = -1;
    for (int lnum = 1; std::getline(in, line); lnum++) {
        line = trim(line);
        if (!line.empty()) {
            if (line.find_first_of('#') == 0) continue;
            if (line.find_first_of('>') == 0) {
                default_index = games.size();
                line = line.substr(1);
            }
            uint flags = 0;
            auto starts_with_and_skip = [](string& line, string to_match) {
                if (line.find(to_match) == 0) {
                    line = line.substr(to_match.length());
                    return true;
                }
                return false;
            };
            while (true) {
                line = trim(line);
                if (starts_with_and_skip(line, "SLOW_TAPE")) {
                    flags |= GF_SLOW_TAPE;
                    continue;
                }
                if (starts_with_and_skip(line, "NO_WAIT_VBLANK")) {
                    flags |= GF_NO_WAIT_VBLANK;
                    continue;
                }
                break;
            }
            size_t split = line.find_first_of('=');
            if (split == string::npos) {
                std::stringstream ss;
                ss << "Expected to find = at line " << lnum << ": '" << line << "'";
                throw failure(ERROR_SYNTAX, ss.str());
            }
            string name = trim(line.substr(0, split));
            string file = trim(line.substr(split + 1));
            if (name.empty() || file.empty()) {
                std::stringstream ss;
                ss << "Expected to find name = value at line " << lnum;
                throw failure(ERROR_SYNTAX, ss.str());
            }
            int index = games.size();
            if (!root_dir.empty()) {
                path fs_path = root_dir;
                fs_path /= file;
                file = fs_path;
            }
            FILE *fp;
            const char *fn = file.c_str();
            const char *ext;
            ext = strrchr(fn, '.');
            if (!ext) {
                std::stringstream ss;
                ss << "Expected file extension on " << fn;
                throw failure(ERROR_ARGS, ss.str());
            }
            if ((fp = fopen(fn, "rb"))) {
                const struct sdf_geometry *geo;
                vector<uint8_t> contents;
                fseek(fp, 0, SEEK_END);
                size_t size = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                games.emplace_back(name, ext, size, flags);
                contents.resize(size);
                if (1 != fread(contents.data(), size, 1, fp)) {
                    throw failure(ERROR_FILE, "Failed to read file\n");
                }
                auto compressed = compress(contents);
                fclose(fp);
                out << "static unsigned char __game_data " << entity << "_" << index << "_data_z[" << compressed.size()
                    << "] = {\n";
                dump(compressed);
                out << "};\n";
            } else {
                std::stringstream ss;
                ss << "Can't open " << file;
                throw failure(ERROR_FILE, ss.str());
            }
        }

    }

    out << "embedded_" << entity << "_t embedded_" << entity << "s[" << games.size() << "] = {\n";
    int i = 0;
    for (const auto &e : games) {
        out << "   { \"" << std::get<0>(e) << "\",";
        if (is_game) out << " \"" << std::get<1>(e) << "\",";
        out << entity << "_" << i << "_data_z, sizeof(" << entity << "_" << i << "_data_z), " << std::get<2>(e) << "";
        if (is_game) out << ", " << std::get<3>(e);
        out << " },\n";
        i++;
    }
    out << "};\n";
    out << "int embedded_" << entity << "_count = " << games.size() << ";\n";
    out << "int embedded_" << entity << "_default = " << default_index << ";\n";
}

int main(int argc, char **argv) {
    string in_filename, out_filename;
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'p':
                    if (i != argc - 1) {
                        root_dir = argv[++i];
                    }
                    break;
                case 'r':
                    is_rom = true;
                    break;
                case 'g':
                    is_game  = true;
                    break;
            }
        } else if (in_filename.empty()) {
            in_filename = argv[i];
        } else if (out_filename.empty()) {
            out_filename = argv[i];
        } else {
            rc = ERROR_ARGS;
        }
    }
    if (!is_rom && !is_game) {
        printf("ERROR: must specify -r or -g\n");
        rc = ERROR_ARGS;
    }
    if (rc) {
        usage();
    } else {
        try {
            std::ifstream ifile(in_filename);
            if (!ifile.good()) {
                std::stringstream ss;
                ss << "Can't open " << in_filename;
                throw failure(ERROR_FILE, ss.str());
            }
            std::ofstream ofile(out_filename);
            if (!ofile.good()) {
                std::stringstream ss;
                ss << "Can't open " << out_filename;
                throw failure(ERROR_FILE, ss.str());
            }
            doit(ifile, ofile);
        } catch (failure &e) {
            std::cerr << "ERROR: " << e.what();
            rc = e.code();
        } catch (std::exception &e) {
            std::cerr << "ERROR: " << e.what();
            rc = ERROR_UNKNOWN;
        }
        if (rc) {
            std::filesystem::remove(out_filename.c_str());
        }
    }
    return rc;
}


vector<uint8_t> compress(const vector<uint8_t>& raw) {
    tdefl_status status;
    static tdefl_compressor deflator;

    mz_uint comp_flags = TDEFL_WRITE_ZLIB_HEADER | 1500;
    // Initialize the low-level compressor.
    status = tdefl_init(&deflator, NULL, NULL, comp_flags);
    if (status != TDEFL_STATUS_OKAY) {
        throw failure(ERROR_UNKNOWN, "tdefl_init() failed!");
    }

    // just a hacky one pass compression without hsassling with growing buffers;
    vector<uint8_t> compressed(raw.size() * 2);
    const void *next_in = raw.data();
    void *next_out = compressed.data();
    size_t in_bytes = raw.size();
    size_t out_bytes = compressed.size();
    status = tdefl_compress(&deflator, next_in, &in_bytes, next_out, &out_bytes, TDEFL_FINISH);
    compressed.resize(out_bytes);
    if (status != TDEFL_STATUS_DONE) {
        throw failure(ERROR_UNKNOWN, "tdefl_compress failed!");
    }
    return compressed;
}

