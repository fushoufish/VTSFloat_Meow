#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace vtsfloat::benchmark {

constexpr std::size_t kSamplesPerPair = 24;

struct Resolution {
    int width = 0;
    int height = 0;
};

struct ResolutionPair {
    Resolution vts;
    Resolution render;
};

struct Sample {
    std::string recordedAt;
    double alignmentPercent = 0.0;
    double receiveMs = 0.0;
    double scaleMs = 0.0;
    double presentMs = 0.0;
    double cpuPercent = 0.0;
    double gpuPercent = 0.0;
    double vramMb = 0.0;
    double memoryMb = 0.0;
    double vtsFps = 0.0;
    double programFps = 0.0;
};

struct CaptureSummary {
    std::string cpu;
    std::string gpu;
    double ramGb = 0.0;
    double gpuRamGb = 0.0;
    std::string startTime;
    std::string stopTime;
    double avgVtsFps = 0.0;
    double minVtsFps = 0.0;
    double maxVtsFps = 0.0;
    double avgVtsmFps = 0.0;
    double minVtsmFps = 0.0;
    double maxVtsmFps = 0.0;
};

const std::vector<ResolutionPair>& ResolutionPlan();
double AlignmentPercent(const ResolutionPair& pair);
std::string RatingUtf8(const ResolutionPair& pair);
std::string LocalTimestampMilliseconds();
std::string FolderTimestamp();

class CsvCapture {
public:
    bool Open(
        const std::filesystem::path& directory,
        std::wstring& error,
        const std::filesystem::path& rawFileName,
        const CaptureSummary& summary,
        bool append = false);
    bool FinalizeSummary(const CaptureSummary& summary, std::wstring& error);
    bool AppendSample(
        std::size_t pairIndex,
        std::size_t sampleIndex,
        const ResolutionPair& pair,
        const std::string& gpuName,
        const Sample& sample);
    const std::filesystem::path& Directory() const { return directory_; }
    const std::filesystem::path& RawPath() const { return rawPath_; }

private:
    std::filesystem::path directory_;
    std::filesystem::path rawPath_;
    std::ofstream raw_;
};

}  // namespace vtsfloat::benchmark
