#include "localization.h"
#include "toolbar_text.h"

#include <windows.h>

#include <cwchar>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using vtsfloat::i18n::Tr;
using vtsfloat::i18n::UiLanguage;

namespace {

struct Contract {
    const wchar_t* source;
    int availableWidth;
    int fontHeight;
    const wchar_t* suffix = L"";
};

constexpr Contract kPanelContracts[] = {
    // Image, blend and view settings panels, at their logical drawing widths.
    {L"图像", 374, 18},
    {L"画面混合", 374, 18},
    {L"画面设置", 380, 18},
    {L"羽化与旋转", 380, 18},
    {L"清晰度", 117, 14},
    {L"暗角", 117, 14},
    {L"内部阴影", 129, 13},
    {L"外部阴影", 129, 13},
    {L"启用阴影", 95, 12},
    {L"阴影距离", 203, 14},
    {L"阴影扩展", 203, 14},
    {L"阴影模糊", 203, 14},
    {L"阴影颜色", 104, 14},
    {L"不透明度", 203, 14},
    {L"参考背景像素", 273, 14},
    {L"加深程度", 203, 14},
    {L"亮度", 116, 11},
    {L"连接VTube Studio API", 228, 13},
    {L"我不想连接VTube Studio API进行使用", 310, 13},
    {L"正在连接 VTube Studio API", 304, 15, L"..."},
    {L"请在 VTube Studio 中授权插件", 304, 10, L"..."},
    {L"进行画面设置时可能会增加性能开销", 490, 13},
    {L"[不再提示]", 170, 13},
    {L"曝光", 107, 14},
    {L"黑场", 64, 14},
    {L"白场", 64, 14},
    {L"曲线", 175, 14},
    {L"对比度", 117, 14},
    {L"饱和度", 117, 14},
    {L"色相", 117, 14},
    {L"色温", 117, 14},
    {L"锐化", 117, 14},
    {L"混合模式", 114, 14},
    {L"混合强度", 117, 14},
    {L"渐变网格", 117, 14},
    {L"线性减淡（添加）", 291, 14},
    {L"以角色下方的平均色作为混合底色", 407, 12},
    {L"无法捕获屏幕背景", 407, 12},
    {L"边缘羽化", 148, 14},
    {L"点击边缘开关羽化", 260, 12},
    {L"旋转与翻转", 408, 14},
    {L"水平翻转", 116, 15},
    {L"垂直翻转", 115, 15},
    {L"重置旋转与翻转", 412, 15},
};

constexpr Contract kContracts[] = {
    {L"图形设置", 88, 15},
    {L"个性化", 76, 15},
    {L"调试", 64, 15},
    {L"完成", 58, 15},
    {L"自定义", 88, 13},
    {L"跑马灯渐变", 88, 13},
    {L"普通渐变", 88, 13},
    {L"恢复默认", 108, 13},
    {L"取消", 108, 13},
    {L"手动框选悬停触发范围", 192, 13},
    {L"选择完成", 106, 13},
    // Wide toolbar, including the configured shortcut suffix.
    {L"快捷键", 191, 15, L" Ctrl+Shift+L"},
    // Personalization panel (420 logical px layout).
    {L"边框样式", 89, 14},
    {L"自定义颜色", 154, 14},
    {L"亮度", 79, 13},
    {L"边框粗细", 99, 13},
    {L"模型不透明度", 119, 13},
    {L"鼠标经过模型时将模型透明化", 345, 13},
    {L"悬停不透明度", 119, 13},
    {L"透明度恢复延时", 154, 13},
    {L"默认 0 秒", 105, 10},
    {L"悬停扩展", 99, 13},
    {L"启用手动框选范围", 141, 13},
    {L"鼠标移入时触发表情，移出后立即恢复", 330, 13},
    {L"表情恢复延时", 124, 13},
    {L"ENTER 确认", 100, 10},
    {L"选择表情", 373, 13},
    {L"暂无可用表情，请确认 VTube Studio API 已连接", 424, 13},
    // Manual-region floating toolbar.
    {L"当前框选主体数量：", 172, 14, L"99"},
    {L"完成  Enter", 118, 14},
    // Waiting page at the minimum 640 px overlay width.
    {L"已找到 VTube Studio，但当前尚未运行", 580, 22},
    {L"等待 VTube Studio 启动中", 580, 22},
    {L"从 Steam 启动 VTube Studio", 273, 16},
    {L"从外部启动 VTS", 273, 16},
    {L"你刚刚拒绝了VTSFloat_Meow", 340, 15},
    {L"[重新发起授权]", 220, 15},
    {L"你可以访问该教程页面：", 580, 14},
    {L"GitHub-VTSFloat_Meow_DOCS", 580, 14},
    {L" 页面查看更多", 580, 14},
    // Toolbar notifications and in-model warning cards.
    // These result cells are measured at runtime and expanded to their text.
    {L"扫描中", 180, 15, L"..."},
    {L"扫描成功", 180, 15},
    {L"获取失败", 180, 15},
    {L"VTube Studio API 连接已断开", 430, 15},
    {L"检测到当前 Spout 非透明推流：", 448, 14},
    {L"请在 VTS 主界面更改背景为 \"ColorPicker\" 并启用 \"透明推流\"", 448, 14},
    {L"当前运行在高性能显卡中，高负载场景性能将会受限", 430, 14},
};

}  // namespace

int wmain() {
    constexpr UiLanguage languages[] = {
        UiLanguage::SimplifiedChinese,
        UiLanguage::TraditionalChinese,
        UiLanguage::English,
        UiLanguage::Japanese,
        UiLanguage::Korean,
        UiLanguage::Russian,
    };
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return 2;
    int failures = 0;
    const auto auditHotkeyPrompt = [&]() {
        for (int scale = 70; scale <= 200; scale += 10) {
            const auto px = [scale](int logical) {
                return static_cast<int>(std::lround(logical * scale / 100.0));
            };
            for (const int width : {195, 300}) {
                const RECT textRect{px(4), px(5), px(width) - px(4), px(37)};
                if (!vtsfloat::DrawFittedToolbarText(dc, textRect,
                        Tr(L"请按组合键（Esc取消）"), vtsfloat::i18n::UiFontFace(),
                        px(15), px(11), false)) {
                    ++failures;
                    std::wcerr << L"[hotkey-prompt] "
                               << vtsfloat::i18n::LanguageCode(vtsfloat::i18n::GetLanguage())
                               << L" scale=" << scale << L" width=" << width << L"\n";
                }
            }
        }
    };
    const auto expectLanguage = [&failures](
        const wchar_t* label, UiLanguage actual, UiLanguage expected) {
        if (actual == expected) return;
        ++failures;
        std::wcerr << L"[language-detection] " << label << L" failed\n";
    };
    expectLanguage(
        L"zh-TW", LanguageFromLocaleName(L"zh-TW", UiLanguage::English),
        UiLanguage::TraditionalChinese);
    expectLanguage(
        L"zh-Hant-HK", LanguageFromLocaleName(L"zh-Hant-HK", UiLanguage::English),
        UiLanguage::TraditionalChinese);
    expectLanguage(
        L"zh-CN", LanguageFromLocaleName(L"zh-CN", UiLanguage::English),
        UiLanguage::SimplifiedChinese);
    expectLanguage(
        L"saved zh-TW", LanguageFromCode(L"zh-TW", UiLanguage::English),
        UiLanguage::TraditionalChinese);
    vtsfloat::i18n::SetLanguage(UiLanguage::TraditionalChinese);
    if (std::wcscmp(Tr(L"图形设置"), L"圖形設定") != 0 ||
        std::wcscmp(Tr(L"正片叠底"), L"色彩增值") != 0 ||
        std::wcscmp(Tr(L"实色混合"), L"實色疊印混合") != 0 ||
        std::wcscmp(
            Tr(L"桌面模式：其他程序全屏时暂停输出"),
            L"桌面模式：其他程式全螢幕時暫停輸出") != 0) {
        ++failures;
        std::wcerr << L"[traditional-chinese] conversion or terminology failed\n";
    }
    for (UiLanguage language : languages) {
        vtsfloat::i18n::SetLanguage(language);
        auditHotkeyPrompt();
        const auto audit = [&](const auto& contracts, int scale, const wchar_t* group) {
            const auto px = [scale](int logical) {
                return static_cast<int>(std::lround(logical * scale / 100.0));
            };
            for (size_t index = 0; index < ARRAYSIZE(contracts); ++index) {
                const Contract& contract = contracts[index];
                HFONT font = CreateFontW(
                    -px(contract.fontHeight), 0, 0, 0, FW_SEMIBOLD,
                    FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                    vtsfloat::i18n::UiFontFace());
                HGDIOBJ previous = SelectObject(dc, font);
                std::wstring text = Tr(contract.source);
                text += contract.suffix;
                SIZE size{};
                GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
                SelectObject(dc, previous);
                DeleteObject(font);
                if (size.cx > px(contract.availableWidth)) {
                    ++failures;
                    std::wcerr << L"[overflow] "
                               << vtsfloat::i18n::LanguageCode(language) << L" " << group
                               << L" index=" << index << L" scale=" << scale
                               << L" width=" << size.cx
                               << L"px available=" << px(contract.availableWidth) << L"px\n";
                }
            }
        };
        audit(kContracts, 100, L"existing");
        for (const int scale : {70, 100, 140, 200}) audit(kPanelContracts, scale, L"edit-panels");
    }

    // Exercise the external INI path separately. Keep the audit isolated from
    // the user's real LOCALAPPDATA language file.
    const std::filesystem::path customAuditPath =
        std::filesystem::temp_directory_path() /
        (L"VTSFloat_Meow.custom-language.audit-" +
         std::to_wstring(GetCurrentProcessId()) + L".ini");
    std::error_code customAuditError;
    std::filesystem::remove(customAuditPath, customAuditError);
    SetEnvironmentVariableW(
        L"VTSFLOAT_CUSTOM_LANGUAGE_PATH", customAuditPath.c_str());
    vtsfloat::i18n::SetLanguage(UiLanguage::Custom);
    auditHotkeyPrompt();
    const std::wstring customPath = vtsfloat::i18n::CustomLanguageFilePath();
    if (customPath.empty() ||
        std::wcscmp(Tr(L"图形设置"), L"Graphics") != 0) {
        ++failures;
        std::wcerr << L"[custom-language] English baseline/template failed\n";
    } else {
        const auto replaceGraphicsValue = [&customPath](const std::string& replacement) {
            std::ifstream input(customPath, std::ios::binary);
            std::string bytes(
                (std::istreambuf_iterator<char>(input)),
                std::istreambuf_iterator<char>());
            constexpr char key[] = "T_7F135691321F4B56=";
            const size_t valueStart = bytes.find(key);
            if (valueStart == std::string::npos) return false;
            const size_t begin = valueStart + sizeof(key) - 1;
            size_t end = bytes.find_first_of("\r\n", begin);
            if (end == std::string::npos) end = bytes.size();
            bytes.replace(begin, end - begin, replacement);
            std::ofstream output(customPath, std::ios::binary | std::ios::trunc);
            output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            return output.good();
        };
        if (!replaceGraphicsValue("Audit custom text #Graphics") ||
            !vtsfloat::i18n::ReloadCustomLanguage() ||
            std::wcscmp(Tr(L"图形设置"), L"Audit custom text") != 0) {
            ++failures;
            std::wcerr << L"[custom-language] edited value or inline note parsing failed\n";
        }
        replaceGraphicsValue("Graphics #Graphics");
        vtsfloat::i18n::ReloadCustomLanguage();
    }
    SetEnvironmentVariableW(L"VTSFLOAT_CUSTOM_LANGUAGE_PATH", nullptr);
    customAuditError.clear();
    std::filesystem::remove(customAuditPath, customAuditError);
    DeleteDC(dc);
    if (failures == 0) {
        std::wcout << L"Localization fixed-width audit passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
