#pragma once

#include <string>
#include <windows.h>

std::string convertUtf8ToCurrentCodepage(const std::string& utf8Str) {
	if (GetACP() == CP_UTF8) {
		return std::string(utf8Str);
	}
	// Step 1: Convert UTF-8 to UTF-16
	int utf16Len = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
	if (utf16Len == 0) {
			return std::string(utf8Str);
	}

	std::wstring utf16Str(utf16Len, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &utf16Str[0], utf16Len);

	// Step 2: Convert UTF-16 to the current Windows codepage
	int codepageLen = WideCharToMultiByte(GetACP(), 0, utf16Str.c_str(), -1, NULL, 0, NULL, NULL);
	if (codepageLen == 0) {
			return std::string(utf8Str);
	}

	std::string codepageStr(codepageLen, '\0');
	WideCharToMultiByte(GetACP(), 0, utf16Str.c_str(), -1, &codepageStr[0], codepageLen, NULL, NULL);

	return codepageStr;
}