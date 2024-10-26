#pragma once

#include <string>
#include <vector>

#include <windows.h>
#include <wtsapi32.h>

std::wstring acpToUtf16(const std::string& origStr);
std::string utf16ToAcp(const std::wstring& utf16Str);
std::string utf8ToAcp(const std::string& utf8Str);
std::string acpToUtf8(const std::string& currentStr);

std::string get_error_string(LONG error_code);

bool query_display_config(std::vector<DISPLAYCONFIG_PATH_INFO>& paths, std::vector<DISPLAYCONFIG_MODE_INFO>& modes, bool active_only);

bool is_user_session_locked();

bool test_no_access_to_ccd_api();

bool is_changing_settings_going_to_fail();
