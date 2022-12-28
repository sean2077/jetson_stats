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

static std::vector<std::string> tegrastats_path{
    "/usr/bin/tegrastats",
    "/home/nvidia/tegrastats",
};

static std::regex RAM_RE{R"(RAM (\d+)\/(\d+)MB)"};
static std::regex UTIL_RE{R"(GR3D(_FREQ?) ([0-9]+)%)"};
static std::regex CPU_RE{R"(CPU \[(.*?)\])"};
static std::regex VAL_FRE_RE{R"(\b(\d+)%@(\d+))"};

static bool is_file(const std::string& path) {
    struct stat s {};
    if (stat(path.c_str(), &s) == 0) {
        return s.st_mode & S_IFREG;
    }
    return false;
}

static std::vector<std::string> split_string(const std::string& s, const std::string& c) {
    std::vector<std::string> v;
    std::string::size_type pos1, pos2;
    pos2 = s.find(c);
    pos1 = 0;
    while (std::string::npos != pos2) {
        v.push_back(s.substr(pos1, pos2 - pos1));
        pos1 = pos2 + c.size();
        pos2 = s.find(c, pos1);
    }
    if (pos1 != s.length()) v.push_back(s.substr(pos1));
    return v;
}

static std::map<std::string, int> val_fraq(const std::string& val) {
    if (val.find("@") != std::string::npos) {
        std::smatch sm;
        if (std::regex_search(val, sm, VAL_FRE_RE)) {
            if (sm.size() == 3) {
                return {{"val", std::stoi(sm[1])}, {"frq", std::stoi(sm[2]) * 1000}};
            }
            return {};
        }
    }
    return {{"val", std::stoi(val)}};
}

static FILE* pipe_file;

static void init() {
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
    float cpu_utilization;  // in percentage
};

static info get_info() {
    info res{};

    char line[512];
    if (fgets(line, 512, pipe_file) == nullptr) {
        return res;
    };
    //    std::cout << line;

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
    if (std::regex_search(s, sm, CPU_RE)) {
        if (sm.size() == 2) {
            auto cpu_list = split_string(sm[1], ",");
            int total_cpu_usage = 0;
            for (size_t i = 0; i < cpu_list.size(); i++) {
                if (cpu_list[i] == "off") {
                    continue;
                }
                auto val = val_fraq(cpu_list[i]);
                total_cpu_usage += val["val"];
            }
            if (cpu_list.size() > 0) {
                res.cpu_utilization = total_cpu_usage * 1.f / cpu_list.size();
            }
        }
    }

    return res;
}

static void showdown() {
    if (pipe_file) {
        pclose(pipe_file);
        pipe_file = nullptr;
    }
}
}  // namespace jetson_stats
