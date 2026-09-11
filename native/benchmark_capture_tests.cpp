#include "benchmark_capture.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    const auto& plan = vtsfloat::benchmark::ResolutionPlan();
    if (plan.size() != 49 ||
        plan.front().vts.width != 3840 || plan.front().render.width != 640 ||
        plan[6].vts.width != 640 || plan[6].render.width != 640 ||
        plan[7].vts.width != 3840 || plan[7].render.width != 960 ||
        plan[14].vts.width != 3840 || plan[14].render.width != 1280 ||
        plan.back().vts.width != 640 || plan.back().render.width != 3840) {
        std::cerr << "resolution plan mismatch\n";
        return 1;
    }
    const std::string timestamp =
        vtsfloat::benchmark::LocalTimestampMilliseconds();
    if (timestamp.size() != 17 || timestamp[4] != ':' ||
        timestamp[7] != ':' || timestamp[10] != ':' ||
        timestamp[13] != ':') {
        std::cerr << "millisecond timestamp format mismatch\n";
        return 2;
    }
    for (const std::size_t index : {0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u,
                                    11u, 12u, 14u, 15u, 16u}) {
        if (!std::isdigit(static_cast<unsigned char>(timestamp[index]))) {
            std::cerr << "millisecond timestamp contains a non-digit\n";
            return 3;
        }
    }

    const auto unique = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        ("vtsfloat_benchmark_capture_" + std::to_string(unique));
    const std::filesystem::path rawName = "20260909_010203.csv";
    vtsfloat::benchmark::CaptureSummary summary;
    summary.cpu = "AMD Ryzen Test CPU";
    summary.gpu = "NVIDIA Test GPU";
    summary.ramGb = 31.25;
    summary.gpuRamGb = 15.99;
    summary.startTime = timestamp;
    {
        vtsfloat::benchmark::CsvCapture capture;
        std::wstring error;
        if (!capture.Open(directory, error, rawName, summary)) {
            std::cerr << "could not open capture output\n";
            return 4;
        }
        vtsfloat::benchmark::Sample sample;
        sample.recordedAt = timestamp;
        sample.alignmentPercent = 25.0;
        sample.receiveMs = 0.16;
        sample.scaleMs = 4.6;
        sample.presentMs = 1.43;
        sample.cpuPercent = 2.0;
        sample.gpuPercent = 3.0;
        sample.vramMb = 128.0;
        sample.memoryMb = 64.0;
        sample.vtsFps = 60.0;
        sample.programFps = 59.5;
        if (!capture.AppendSample(0, 0, plan.front(), "NVIDIA Test GPU", sample)) {
            std::cerr << "could not append raw sample\n";
            return 5;
        }
        summary.stopTime = timestamp;
        summary.avgVtsFps = 59.8;
        summary.minVtsFps = 58.0;
        summary.maxVtsFps = 60.0;
        summary.avgVtsmFps = 59.5;
        summary.minVtsmFps = 57.0;
        summary.maxVtsmFps = 60.0;
        if (!capture.FinalizeSummary(summary, error)) {
            std::cerr << "could not finalize CSV summary\n";
            return 6;
        }
    }
    std::size_t fileCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) ++fileCount;
    }
    std::ifstream raw(directory / rawName, std::ios::binary);
    std::string summaryHeader;
    std::string summaryLine;
    std::string blankLine;
    std::string header;
    std::string rawLine;
    std::string extraLine;
    std::getline(raw, summaryHeader);
    std::getline(raw, summaryLine);
    std::getline(raw, blankLine);
    std::getline(raw, header);
    std::getline(raw, rawLine);
    std::getline(raw, extraLine);
    if (fileCount != 1 ||
        summaryHeader.find("AVG_VTS_FPS") == std::string::npos ||
        summaryLine.find("AMD Ryzen Test CPU") == std::string::npos ||
        summaryLine.find("59.800") == std::string::npos ||
        !blankLine.empty() ||
        header.find("程序帧数(FPS)") == std::string::npos ||
        rawLine.find(timestamp) == std::string::npos ||
        !extraLine.empty() ||
        std::count(header.begin(), header.end(), ',') != 16 ||
        std::count(rawLine.begin(), rawLine.end(), ',') != 16) {
        std::cerr << "single CSV capture format mismatch\n";
        return 7;
    }
    {
        vtsfloat::benchmark::CsvCapture secondCapture;
        std::wstring secondError;
        const std::filesystem::path secondRawName = "20260909_020304.csv";
        summary.gpu = "AMD Test GPU";
        if (!secondCapture.Open(
                directory, secondError, secondRawName, summary) ||
            !secondCapture.AppendSample(
                0, 0, plan.front(), "AMD Test GPU",
                vtsfloat::benchmark::Sample{}) ||
            !secondCapture.FinalizeSummary(summary, secondError) ||
            !std::filesystem::is_regular_file(directory / secondRawName) ||
            !std::filesystem::is_regular_file(directory / rawName)) {
            std::cerr << "per-GPU CSV separation failed\n";
            return 8;
        }
    }
    fileCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) ++fileCount;
    }
    if (fileCount != 2) {
        std::cerr << "unexpected extra benchmark report file\n";
        return 9;
    }
    std::error_code cleanupError;
    std::filesystem::remove_all(directory, cleanupError);
    return 0;
}
