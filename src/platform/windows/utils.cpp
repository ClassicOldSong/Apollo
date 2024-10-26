#include "utils.h"

#include <SetupApi.h>

#include "src/utility.h"
#include "src/logging.h"

std::wstring acpToUtf16(const std::string& origStr) {
	auto acp = GetACP();

	int utf16Len = MultiByteToWideChar(acp, 0, origStr.c_str(), origStr.size(), NULL, 0);
	if (utf16Len == 0) {
		return L"";
	}

	std::wstring utf16Str(utf16Len, L'\0');
	MultiByteToWideChar(acp, 0, origStr.c_str(), origStr.size(), &utf16Str[0], utf16Len);

	return utf16Str;
}

std::string utf16ToAcp(const std::wstring& utf16Str) {
	auto acp = GetACP();

	int codepageLen = WideCharToMultiByte(acp, 0, utf16Str.c_str(), utf16Str.size(), NULL, 0, NULL, NULL);
	if (codepageLen == 0) {
		return "";
	}

	std::string codepageStr(codepageLen, '\0');
	WideCharToMultiByte(acp, 0, utf16Str.c_str(), utf16Str.size(), &codepageStr[0], codepageLen, NULL, NULL);

	return codepageStr;
}

std::string utf8ToAcp(const std::string& utf8Str) {
	if (GetACP() == CP_UTF8) {
		return std::string(utf8Str);
	}

	int utf16Len = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), utf8Str.size(), NULL, 0);
	if (utf16Len == 0) {
		return std::string(utf8Str);
	}

	std::wstring utf16Str(utf16Len, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), utf8Str.size(), &utf16Str[0], utf16Len);

	return utf16ToAcp(utf16Str);
}

std::string acpToUtf8(const std::string& origStr) {
	if (GetACP() == CP_UTF8) {
		return std::string(origStr);
	}

	auto utf16Str = acpToUtf16(origStr);

	int utf8Len = WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), utf16Str.size(), NULL, 0, NULL, NULL);
	if (utf8Len == 0) {
		return std::string(origStr);
	}

	std::string utf8Str(utf8Len, '\0');
	WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), utf16Str.size(), &utf8Str[0], utf8Len, NULL, NULL);

	return utf8Str;
}

// Modified from https://github.com/FrogTheFrog/Sunshine/blob/b6f8573d35eff7c55da6965dfa317dc9722bd4ef/src/platform/windows/display_device/windows_utils.cpp

std::string
get_error_string(LONG error_code) {
	std::stringstream error;
	error << "[code: ";
	switch (error_code) {
		case ERROR_INVALID_PARAMETER:
			error << "ERROR_INVALID_PARAMETER";
			break;
		case ERROR_NOT_SUPPORTED:
			error << "ERROR_NOT_SUPPORTED";
			break;
		case ERROR_ACCESS_DENIED:
			error << "ERROR_ACCESS_DENIED";
			break;
		case ERROR_INSUFFICIENT_BUFFER:
			error << "ERROR_INSUFFICIENT_BUFFER";
			break;
		case ERROR_GEN_FAILURE:
			error << "ERROR_GEN_FAILURE";
			break;
		case ERROR_SUCCESS:
			error << "ERROR_SUCCESS";
			break;
		default:
			error << error_code;
			break;
	}
	error << ", message: " << std::system_category().message(static_cast<int>(error_code)) << "]";
	return error.str();
}

bool
query_display_config(std::vector<DISPLAYCONFIG_PATH_INFO>& paths, std::vector<DISPLAYCONFIG_MODE_INFO>& modes, bool active_only) {
	LONG result = ERROR_SUCCESS;

	// When we want to enable/disable displays, we need to get all paths as they will not be active.
	// This will require some additional filtering of duplicate and otherwise useless paths.
	UINT32 flags = active_only ? QDC_ONLY_ACTIVE_PATHS : QDC_ALL_PATHS;
	flags |= QDC_VIRTUAL_MODE_AWARE;  // supported from W10 onwards

	do {
		UINT32 path_count { 0 };
		UINT32 mode_count { 0 };

		result = GetDisplayConfigBufferSizes(flags, &path_count, &mode_count);
		if (result != ERROR_SUCCESS) {
			BOOST_LOG(error) << get_error_string(result) << " failed to get display paths and modes!";
			return false;
		}

		paths.resize(path_count);
		modes.resize(mode_count);
		result = QueryDisplayConfig(flags, &path_count, paths.data(), &mode_count, modes.data(), nullptr);

		// The function may have returned fewer paths/modes than estimated
		paths.resize(path_count);
		modes.resize(mode_count);

		// It's possible that between the call to GetDisplayConfigBufferSizes and QueryDisplayConfig
		// that the display state changed, so loop on the case of ERROR_INSUFFICIENT_BUFFER.
	} while (result == ERROR_INSUFFICIENT_BUFFER);

	if (result != ERROR_SUCCESS) {
		BOOST_LOG(error) << get_error_string(result) << " failed to query display paths and modes!";
		return false;
	}

	return true;
}

bool
is_user_session_locked() {
	LPWSTR buffer { nullptr };
	const auto cleanup_guard {
		util::fail_guard([&buffer]() {
			if (buffer) {
				WTSFreeMemory(buffer);
			}
		})
	};

	DWORD buffer_size_in_bytes { 0 };
	if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTSGetActiveConsoleSessionId(), WTSSessionInfoEx, &buffer, &buffer_size_in_bytes)) {
		if (buffer_size_in_bytes > 0) {
			const auto wts_info { reinterpret_cast<const WTSINFOEXW *>(buffer) };
			if (wts_info && wts_info->Level == 1) {
				const bool is_locked { wts_info->Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_LOCK };
				BOOST_LOG(debug) << "is_user_session_locked: " << is_locked;
				return is_locked;
			}
		}

		BOOST_LOG(warning) << "Failed to get session info in is_user_session_locked.";
	}
	else {
		BOOST_LOG(error) << get_error_string(GetLastError()) << " failed while calling WTSQuerySessionInformationW!";
	}

	return false;
}

bool
test_no_access_to_ccd_api() {
	std::vector<DISPLAYCONFIG_PATH_INFO> paths;
	std::vector<DISPLAYCONFIG_MODE_INFO> modes;
	if (!query_display_config(paths, modes, true)) {
		BOOST_LOG(debug) << "test_no_access_to_ccd_api failed in query_display_config.";
		return true;
	}

	// Here we are supplying the retrieved display data back to SetDisplayConfig (with VALIDATE flag only, so that we make no actual changes).
	// Unless something is really broken on Windows, this call should never fail under normal circumstances - the configuration is 100% correct, since it was
	// provided by Windows.
	const UINT32 flags { SDC_VALIDATE | SDC_USE_SUPPLIED_DISPLAY_CONFIG | SDC_VIRTUAL_MODE_AWARE };
	const LONG result { SetDisplayConfig(paths.size(), paths.data(), modes.size(), modes.data(), flags) };

	BOOST_LOG(debug) << "test_no_access_to_ccd_api result: " << get_error_string(result);
	return result == ERROR_ACCESS_DENIED;
}

bool
is_changing_settings_going_to_fail() {
	return is_user_session_locked() || test_no_access_to_ccd_api();
}
