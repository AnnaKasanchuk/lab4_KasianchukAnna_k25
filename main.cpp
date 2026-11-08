// Author: Kasianchuk Anna K-25
// Variant 19
// Compiler: Bundled MinGW 14.0 w64 (GCC 14), CLion toolchain
// Language standard: C++20

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "Data.h"

namespace {

constexpr int kParts = 6;
constexpr int kRepeats = 5;
constexpr int kMaxThreads = 3;
constexpr std::size_t kDefaultOpsPerFile = 500000;

struct Scenario {
    std::string name;
    std::array<double, 5> weights;
    std::string title;
};

void generateFile(const std::string& path, const Scenario& sc, std::size_t opsCount, unsigned seed) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Cannot create " + path);
    }
    std::mt19937 gen(seed);
    std::discrete_distribution<int> dist(sc.weights.begin(), sc.weights.end());
    for (std::size_t i = 0; i < opsCount; ++i) {
        switch (dist(gen)) {
            case 0: out << "read 0\n"; break;
            case 1: out << "write 0 1\n"; break;
            case 2: out << "read 1\n"; break;
            case 3: out << "write 1 1\n"; break;
            default: out << "string\n"; break;
        }
    }
}

std::string filePath(const Scenario& sc, int part) {
    return "data/" + sc.name + "_" + std::to_string(part) + ".txt";
}

void generateAll(const std::vector<Scenario>& scenarios, std::size_t opsPerFile) {
    std::filesystem::create_directories("data");
    for (const Scenario& sc : scenarios) {
        for (int part = 0; part < kParts; ++part) {
            const unsigned seed = 12345u + 101u * static_cast<unsigned>(part) +
                                  static_cast<unsigned>(sc.name[0]);
            generateFile(filePath(sc, part), sc, opsPerFile, seed);
        }
    }
}

enum class OpType : std::uint8_t { Read, Write, Str };

struct Op {
    OpType type;
    int field;
    int value;
};

using OpList = std::vector<Op>;
using Clock = std::chrono::steady_clock;

OpList loadFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Cannot open " + path);
    }
    OpList ops;
    std::string cmd;
    while (in >> cmd) {
        if (cmd == "write") {
            int field = 0;
            int value = 0;
            in >> field >> value;
            ops.push_back({OpType::Write, field, value});
        } else if (cmd == "read") {
            int field = 0;
            in >> field;
            ops.push_back({OpType::Read, field, 0});
        } else if (cmd == "string") {
            ops.push_back({OpType::Str, 0, 0});
        } else {
            throw std::runtime_error("Unknown command '" + cmd + "' in " + path);
        }
    }
    return ops;
}

long long runSequence(SharedData& data, const OpList& ops) {
    long long sink = 0;
    for (const Op& op : ops) {
        switch (op.type) {
            case OpType::Read:
                sink += data.get(op.field);
                break;
            case OpType::Write:
                data.set(op.field, op.value);
                break;
            case OpType::Str:
                sink += static_cast<long long>(static_cast<std::string>(data).size());
                break;
        }
    }
    return sink;
}

std::vector<OpList> splitByThreads(const std::vector<OpList>& parts, int threadCount) {
    std::vector<OpList> perThread(static_cast<std::size_t>(threadCount));
    for (std::size_t i = 0; i < parts.size(); ++i) {
        OpList& target = perThread[i % static_cast<std::size_t>(threadCount)];
        target.insert(target.end(), parts[i].begin(), parts[i].end());
    }
    return perThread;
}

double measureOnce(const std::vector<OpList>& perThread, long long& checksum) {
    SharedData data;
    std::vector<long long> sinks(perThread.size(), 0);
    std::vector<std::thread> threads;
    threads.reserve(perThread.size());

    const auto start = Clock::now();
    for (std::size_t t = 0; t < perThread.size(); ++t) {
        threads.emplace_back([&data, &perThread, &sinks, t] {
            long long local_sink = runSequence(data, perThread[t]);
            sinks[t] = local_sink;
        });
    }
    for (std::thread& th : threads) {
        th.join();
    }
    const auto finish = Clock::now();

    checksum = 0;
    for (long long s : sinks) {
        checksum += s;
    }
    return std::chrono::duration<double, std::milli>(finish - start).count();
}

std::size_t totalOps(const std::vector<OpList>& lists) {
    std::size_t sum = 0;
    for (const OpList& l : lists) {
        sum += l.size();
    }
    return sum;
}

}

int main(int argc, char** argv) {
    try {
        const std::size_t opsPerFile =
            argc > 1 ? static_cast<std::size_t>(std::stoull(argv[1])) : kDefaultOpsPerFile;

        SharedData demo;
        demo.set(0, 13);
        demo.set(1, 7);
        std::cout << "Demo: string(data) = " << static_cast<std::string>(demo) << "\n\n";

        const std::vector<Scenario> scenarios = {
            {"a", {5, 40, 5, 5, 45},    "a) frequencies from the task"},
            {"b", {20, 20, 20, 20, 20}, "b) all frequencies equal"},
            {"c", {15, 5, 5, 70, 5},    "c) manually chosen, mismatching"},
        };

        std::cout << "Generating test files (" << kParts << " files x " << opsPerFile
                  << " operations per scenario)...\n\n";
        generateAll(scenarios, opsPerFile);

        double table[3][kMaxThreads] = {};

        for (std::size_t s = 0; s < scenarios.size(); ++s) {
            std::vector<OpList> parts;
            for (int p = 0; p < kParts; ++p) {
                parts.push_back(loadFile(filePath(scenarios[s], p)));
            }

            for (int threads = 1; threads <= kMaxThreads; ++threads) {
                const std::vector<OpList> perThread = splitByThreads(parts, threads);

                double sum = 0.0;
                long long checksum = 0;
                std::cout << "Scenario " << scenarios[s].name << ", threads=" << threads
                          << ", total ops=" << totalOps(perThread) << ", runs (ms):";
                for (int r = 0; r < kRepeats; ++r) {
                    const double ms = measureOnce(perThread, checksum);
                    sum += ms;
                    std::cout << ' ' << std::fixed << std::setprecision(1) << ms;
                }

                table[s][threads - 1] = sum / kRepeats;
                std::cout << " | avg = " << std::fixed << std::setprecision(1)
                          << table[s][threads - 1] << " ms\n";
            }
            std::cout << '\n';
        }

        std::cout << "Average time, ms (rows - test files, columns - threads)\n";
        std::cout << std::left << std::setw(36) << "" << std::right
                  << std::setw(12) << "1 thread" << std::setw(12) << "2 threads"
                  << std::setw(12) << "3 threads" << '\n';
        for (std::size_t s = 0; s < scenarios.size(); ++s) {
            std::cout << std::left << std::setw(36) << scenarios[s].title << std::right;
            for (int t = 0; t < kMaxThreads; ++t) {
                std::cout << std::setw(12) << std::fixed << std::setprecision(1) << table[s][t];
            }
            std::cout << '\n';
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}