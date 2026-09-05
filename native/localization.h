#pragma once

#include <string>

namespace vtsfloat::i18n {

enum class UiLanguage {
    SimplifiedChinese,
    English,
    Japanese,
    Korean,
    Russian,
    Custom,
};

UiLanguage DetectSystemLanguage();
UiLanguage LanguageFromCode(const std::wstring& code, UiLanguage fallback);
const wchar_t* LanguageCode(UiLanguage language);
const wchar_t* LanguageDisplayName(UiLanguage language);

void SetLanguage(UiLanguage language);
UiLanguage GetLanguage();

// The custom language is stored in a UTF-16 INI beside the executable. The
// generated template uses English values as both its reference and fallback.
bool ReloadCustomLanguage();
std::wstring CustomLanguageFilePath();

// Returns the localized text for a Simplified-Chinese source string. Missing
// entries deliberately fall back to the source text so an incomplete preview
// can never render an empty control.
const wchar_t* Tr(const wchar_t* simplifiedChinese);

// Pick an installed Windows UI font designed for the active writing system.
const wchar_t* UiFontFace();

}  // namespace vtsfloat::i18n
