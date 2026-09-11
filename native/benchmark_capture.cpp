#include "benchmark_capture.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace vtsfloat::benchmark {
namespace {

constexpr std::array<Resolution, 7> kVtsResolutions{{
    {3840, 2160},
    {2560, 1440},
    {1920, 1080},
    {1600, 900},
    {1280, 720},
    {960, 540},
    {640, 480},
}};

constexpr std::array<Resolution, 7> kRenderResolutions{{
    {640, 480},
    {960, 540},
    {1280, 720},
    {1600, 900},
    {1920, 1080},
    {2560, 1440},
    {3840, 2160},
}};

const std::vector<ResolutionPair> kResolutionPlan = [] {
    std::vector<ResolutionPair> plan;
    plan.reserve(kVtsResolutions.size() * kRenderResolutions.size());
    for (const Resolution& render : kRenderResolutions) {
        for (const Resolution& vts : kVtsResolutions) {
            plan.push_back({vts, render});
        }
    }
    return plan;
}();

std::string ResolutionText(const Resolution& resolution) {
    return std::to_string(resolution.width) + "x" +
        std::to_string(resolution.height);
}

std::string Number(double value) {
    if (!std::isfinite(value)) return {};
    std::ostringstream output;
    output << std::fixed << std::setprecision(3) << value;
    return output.str();
}

std::string EscapeCsv(const std::string& value) {
    if (value.find_first_of(",\"\r\n") == std::string::npos) return value;
    std::string escaped = "\"";
    for (const char character : value) {
        if (character == '"') escaped += '"';
        escaped += character;
    }
    escaped += '"';
    return escaped;
}

std::string SummaryLine(const CaptureSummary& summary) {
    std::ostringstream output;
    output << EscapeCsv(summary.cpu) << ','
           << EscapeCsv(summary.gpu) << ','
           << Number(summary.ramGb) << ','
           << Number(summary.gpuRamGb) << ','
           << EscapeCsv(summary.startTime) << ','
           << EscapeCsv(summary.stopTime) << ','
           << Number(summary.avgVtsFps) << ','
           << Number(summary.minVtsFps) << ','
           << Number(summary.maxVtsFps) << ','
           << Number(summary.avgVtsmFps) << ','
           << Number(summary.minVtsmFps) << ','
           << Number(summary.maxVtsmFps);
    return output.str();
}

void WriteUtf8Bom(std::ofstream& output) {
    constexpr unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    output.write(reinterpret_cast<const char*>(bom), sizeof(bom));
}

}  // namespace

const std::vector<ResolutionPair>& ResolutionPlan() {
    return kResolutionPlan;
}

double AlignmentPercent(const ResolutionPair& pair) {
    const double sourcePixels =
        static_cast<double>(pair.vts.width) * pair.vts.height;
    const double renderPixels =
        static_cast<double>(pair.render.width) * pair.render.height;
    if (sourcePixels <= 0.0 || renderPixels <= 0.0) return 0.0;
    return (std::min)(sourcePixels, renderPixels) /
        (std::max)(sourcePixels, renderPixels) * 100.0;
}

std::string RatingUtf8(const ResolutionPair& pair) {
    const double percent = AlignmentPercent(pair);
    if (percent >= 90.0) return "完美";
    const double sourcePixels =
        static_cast<double>(pair.vts.width) * pair.vts.height;
    const double renderPixels =
        static_cast<double>(pair.render.width) * pair.render.height;
    if (renderPixels < sourcePixels) {
        if (percent < 10.0) return "绝顶质量";
        if (percent < 30.0) return "极高质量";
        if (percent < 50.0) return "超高质量";
        if (percent < 70.0) return "较高质量";
        return "质量";
    }
    if (percent < 10.0) return "究极性能";
    if (percent < 30.0) return "极高性能";
    if (percent < 50.0) return "超高性能";
    if (percent < 70.0) return "较高性能";
    return "性能";
}

std::string LocalTimestampMilliseconds() {
    const auto now = std::chrono::system_clock::now();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &time);
    std::ostringstream output;
    output << std::put_time(&local, "%Y:%H:%M:%S") << ':'
           << std::setfill('0') << std::setw(3) << milliseconds.count();
    return output.str();
}

std::string FolderTimestamp() {
    const std::time_t time = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &time);
    std::ostringstream output;
    output << std::put_time(&local, "%Y%m%d_%H%M%S");
    return output.str();
}

bool CsvCapture::Open(
    const std::filesystem::path& directory,
    std::wstring& error,
    const std::filesystem::path& rawFileName,
    const CaptureSummary& summary,
    bool append) {
    std::error_code filesystemError;
    std::filesystem::create_directories(directory, filesystemError);
    if (filesystemError) {
        error = L"无法创建采集目录：" + directory.wstring();
        return false;
    }
    directory_ = directory;
    if (rawFileName.empty() || rawFileName.has_parent_path() ||
        rawFileName.extension() != L".csv") {
        error = L"原始采集文件名无效。";
        return false;
    }
    rawPath_ = directory_ / rawFileName;
    const bool hasExistingContent = append &&
        std::filesystem::is_regular_file(rawPath_, filesystemError) &&
        std::filesystem::file_size(rawPath_, filesystemError) > 0;
    raw_.open(
        rawPath_, std::ios::binary |
        (append ? std::ios::app : std::ios::trunc));
    if (!raw_) {
        error = L"无法创建 CSV 文件：" + directory.wstring();
        return false;
    }
    if (!hasExistingContent) {
        WriteUtf8Bom(raw_);
        raw_ << "CPU,GPU,RAM,GPU_RAM,START_TIME,STOP_TIME,"
                "AVG_VTS_FPS,MIN_VTS_FPS,MAX_VTS_FPS,"
                "AVG_VTSM_FPS,MIN_VTSM_FPS,MAX_VTSM_FPS\n"
             << SummaryLine(summary) << "\n\n";
        raw_ << "记录时间,GPU名称,VTS原生分辨率,程序缩放分辨率,性能评级,对齐率(%),"
                "接收延迟(ms),缩放延迟(ms),上屏延迟(ms),CPU占用(%),GPU占用(%),"
                "显存(MB),内存(MB),VTS帧数(FPS),程序帧数(FPS),组序号,样本序号\n";
        raw_.flush();
    }
    return true;
}

bool CsvCapture::FinalizeSummary(
    const CaptureSummary& summary, std::wstring& error) {
    if (raw_.is_open()) {
        raw_.flush();
        raw_.close();
    }
    std::ifstream input(rawPath_, std::ios::binary);
    if (!input) {
        error = L"无法读取 CSV 文件：" + rawPath_.wstring();
        return false;
    }
    const std::string contents{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    const std::size_t firstLineEnd = contents.find('\n');
    const std::size_t secondLineEnd = firstLineEnd == std::string::npos
        ? std::string::npos : contents.find('\n', firstLineEnd + 1);
    if (firstLineEnd == std::string::npos || secondLineEnd == std::string::npos) {
        error = L"CSV 汇总区格式无效：" + rawPath_.wstring();
        return false;
    }

    const std::filesystem::path temporary = rawPath_.wstring() + L".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = L"无法更新 CSV 汇总：" + rawPath_.wstring();
        return false;
    }
    output.write(contents.data(), static_cast<std::streamsize>(firstLineEnd + 1));
    output << SummaryLine(summary) << '\n';
    output.write(
        contents.data() + secondLineEnd + 1,
        static_cast<std::streamsize>(contents.size() - secondLineEnd - 1));
    output.close();
    if (!output.good()) {
        error = L"写入 CSV 汇总失败：" + rawPath_.wstring();
        return false;
    }
    std::error_code filesystemError;
    std::filesystem::copy_file(
        temporary, rawPath_,
        std::filesystem::copy_options::overwrite_existing,
        filesystemError);
    if (filesystemError) {
        error = L"替换 CSV 汇总失败：" + rawPath_.wstring();
        std::error_code cleanupError;
        std::filesystem::remove(temporary, cleanupError);
        return false;
    }
    std::filesystem::remove(temporary, filesystemError);
    return true;
}

bool CsvCapture::AppendSample(
    std::size_t pairIndex,
    std::size_t sampleIndex,
    const ResolutionPair& pair,
    const std::string& gpuName,
    const Sample& sample) {
    if (!raw_) return false;
    raw_ << EscapeCsv(sample.recordedAt) << ','
         << EscapeCsv(gpuName) << ','
         << ResolutionText(pair.vts) << ','
         << ResolutionText(pair.render) << ','
         << RatingUtf8(pair) << ','
         << Number(sample.alignmentPercent) << ','
         << Number(sample.receiveMs) << ','
         << Number(sample.scaleMs) << ','
         << Number(sample.presentMs) << ','
         << Number(sample.cpuPercent) << ','
         << Number(sample.gpuPercent) << ','
         << Number(sample.vramMb) << ','
         << Number(sample.memoryMb) << ','
         << Number(sample.vtsFps) << ','
         << Number(sample.programFps) << ','
         << pairIndex + 1 << ',' << sampleIndex + 1 << '\n';
    raw_.flush();
    return raw_.good();
}

}  // namespace vtsfloat::benchmark
