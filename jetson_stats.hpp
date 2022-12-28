// Code Source: https://github.com/zhangxianbing/jetson_stats

#pragma once
#include <sys/stat.h>

#include <cstdio>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace jetson_stats {

std::vector<std::string> tegrastats_path{
    "/usr/bin/tegrastats",
    "/home/nvidia/tegrastats",
};

inline bool is_file(const std::string& path) {
    struct stat s {};
    if (stat(path.c_str(), &s) == 0) {
        return s.st_mode & S_IFREG;
    }
    return false;
}

FILE* pipe_file;

void init() {
    std::string cmd;
    for (auto&& p : tegrastats_path) {
        if (is_file(p)) {
            cmd = p;
            break;
        }
    }
    if (cmd.empty()) {
        throw std::runtime_error("tegrastats is not available on this board");
    }
    cmd += " --interval 500";  // 500ms

    pipe_file = popen(cmd.c_str(), "r");

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
};

struct info {
    float memory_used;      // MB
    float memory_total;     // MB
    float gpu_utilization;  // in percentage
};

info get_info() {
    info res{};

    char line[512];
    fgets(line, 512, pipe_file);
    //    std::cout << line;

    static std::regex RAM_RE{R"(RAM (\d+)\/(\d+)MB)"};
    static std::regex UTIL_RE{R"(GR3D(_FREQ?) ([0-9]+)%)"};

    std::string s = line;

    std::smatch sm;
    if (std::regex_search(s, sm, RAM_RE)) {
        if (sm.size() == 3) {
            res.memory_used = std::stof(sm[1]);
            res.memory_total = std::stof(sm[2]);
        }
    }
    if (std::regex_search(s, sm, UTIL_RE)) {
        if (sm.size() == 3) {
            res.gpu_utilization = std::stof(sm[2]);
        }
    }

    return res;
}

void showdown() {
    if (pipe_file) {
        pclose(pipe_file);
        pipe_file = nullptr;
    }
}
}  // namespace jetson_stats