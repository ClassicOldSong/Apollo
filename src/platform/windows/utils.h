#pragma once

#include <string>

#include <windows.h>
#include <SetupApi.h>
#include <wtsapi32.h>

#include "src/utility.h"
#include "src/logging.h"

std::string convertUtf8ToCurrentCodepage(const std::string& utf8Str);

std::string get_error_string(LONG error_code);

bool query_display_config(std::vector<DISPLAYCONFIG_PATH_INFO>& paths, std::vector<DISPLAYCONFIG_MODE_INFO>& modes, bool active_only);

bool is_user_session_locked();

bool test_no_access_to_ccd_api();

bool is_changing_settings_going_to_fail();
