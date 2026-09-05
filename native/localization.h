#pragma once

#include <string>

namespace vtsfloat::i18n {

enum class UiLanguage {
    SimplifiedChinese,
    English,
    Japanese,
    Korean,
    Russian,
};

UiLanguage DetectSystemLanguage();
UiLanguage LanguageFromCode(const std::wstring& code, UiLanguage fallback);
const wchar_t* LanguageCode(UiLanguage language);
const wchar_t* LanguageDisplayName(UiLanguage language);

void SetLanguage(UiLanguage language);
UiLanguage GetLanguage();

// Returns the localized text for a Simplified-Chinese source string. Missing
// entries deliberately fall back to the source text so an incomplete preview
// can never render an empty control.
const wchar_t* Tr(const wchar_t* simplifiedChinese);

// Pick an installed Windows UI font designed for the active writing system.
const wchar_t* UiFontFace();

}  // namespace vtsfloat::i18n
