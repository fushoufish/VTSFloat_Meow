#include "localization.h"

#include <windows.h>

#include <iostream>
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

constexpr Contract kContracts[] = {
    {L"图形设置", 88, 15},
    {L"主屏居中", 74, 15},
    {L"切换屏幕", 74, 15},
    {L"比例 锁定", 76, 15},
    {L"比例 自由", 76, 15},
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
    {L"锁定/解锁 ", 191, 15, L"Ctrl+Shift+L"},
    // Personalization panel (420 logical px layout).
    {L"边框模式", 76, 14},
    {L"自定义颜色", 154, 14},
    {L"亮度", 79, 13},
    {L"边框粗细", 99, 13},
    {L"模型不透明度", 119, 13},
    {L"鼠标经过模型时将模型透明化", 345, 13},
    {L"悬停不透明度", 119, 13},
    {L"悬停扩展", 99, 13},
    {L"启用手动框选范围", 141, 13},
    {L"鼠标移入时触发表情，移出后立即恢复", 330, 13},
    {L"表情恢复延时", 124, 13},
    {L"选择表情", 373, 13},
    {L"暂无可用表情，请确认 VTS API 已连接", 424, 13},
    // Manual-region floating toolbar.
    {L"当前框选主体数量：", 172, 14, L"99"},
    {L"完成  Enter", 118, 14},
    // Waiting page at the minimum 640 px overlay width.
    {L"已找到 VTube Studio，但当前尚未运行", 580, 22},
    {L"等待 VTube Studio 启动中", 580, 22},
    {L"从 Steam 启动 VTube Studio", 273, 16},
    {L"从外部启动 VTS", 273, 16},
    // Toolbar notifications and in-model warning cards.
    // These result cells are measured at runtime and expanded to their text.
    {L"扫描中", 180, 15, L"..."},
    {L"扫描成功", 180, 15},
    {L"获取失败", 180, 15},
    {L"等待 VTube Studio 授权插件连接中，请打开 VTS 插件进行授权", 608, 15, L"..."},
    {L"VTS API 连接已断开", 430, 15},
    {L"VTS API 尚未授权", 430, 15},
    {L"检测到当前 Spout 非透明推流：", 448, 14},
    {L"请在 VTS 主界面更改背景为 \"ColorPicker\" 并启用 \"透明推流\"", 448, 14},
    {L"当前运行在高性能显卡中，高负载场景性能将会受限", 430, 14},
};

}  // namespace

int wmain() {
    constexpr UiLanguage languages[] = {
        UiLanguage::SimplifiedChinese,
        UiLanguage::English,
        UiLanguage::Japanese,
        UiLanguage::Korean,
        UiLanguage::Russian,
    };
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return 2;
    int failures = 0;
    for (UiLanguage language : languages) {
        vtsfloat::i18n::SetLanguage(language);
        for (size_t contractIndex = 0; contractIndex < ARRAYSIZE(kContracts); ++contractIndex) {
            const Contract& contract = kContracts[contractIndex];
            HFONT font = CreateFontW(
                -contract.fontHeight, 0, 0, 0, FW_SEMIBOLD,
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
            if (size.cx > contract.availableWidth) {
                ++failures;
                std::wcerr << L"[overflow] "
                           << vtsfloat::i18n::LanguageCode(language)
                           << L" contract=" << contractIndex
                           << L" width=" << size.cx
                           << L"px available=" << contract.availableWidth << L"px\n";
            }
        }
    }
    DeleteDC(dc);
    if (failures == 0) {
        std::wcout << L"Localization fixed-width audit passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
