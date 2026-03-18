#include <windows.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <newdev.h>
#include <setupapi.h>
#include <shellapi.h>

#include <algorithm>
#include <cwctype>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {
  constexpr std::wstring_view kHardwareId = L"ROOT\\VirtualAudioDriver";
  constexpr std::wstring_view kRootDeviceId = L"*VirtualAudioDriver";
  constexpr DWORD kInstallFlags = INSTALLFLAG_FORCE;

  std::wstring format_windows_error(DWORD error) {
    if (error == ERROR_SUCCESS) {
      return L"success";
    }

    wchar_t *message_buffer = nullptr;
    const DWORD format_result = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr,
      error,
      0,
      reinterpret_cast<LPWSTR>(&message_buffer),
      0,
      nullptr
    );
    if (format_result == 0 || message_buffer == nullptr) {
      return L"unknown error";
    }

    std::unique_ptr<wchar_t, decltype(&LocalFree)> owned_message(message_buffer, LocalFree);
    std::wstring message = owned_message.get();
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) {
      message.pop_back();
    }

    return message;
  }

  void print_windows_error(std::wstring_view prefix, DWORD error) {
    std::wcerr << prefix << L": " << error << L" (" << format_windows_error(error) << L")\n";
  }

  bool is_running_elevated() {
    HANDLE process_token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token)) {
      return false;
    }

    std::unique_ptr<void, decltype(&CloseHandle)> owned_token(process_token, CloseHandle);
    TOKEN_ELEVATION elevation {};
    DWORD bytes_returned = 0;
    if (!GetTokenInformation(process_token, TokenElevation, &elevation, sizeof(elevation), &bytes_returned)) {
      return false;
    }

    return elevation.TokenIsElevated != 0;
  }

  bool ensure_elevated(std::wstring_view command_name) {
    if (is_running_elevated()) {
      return true;
    }

    std::wcerr << L"Administrator privileges are required to " << command_name
               << L" the Apollo virtual microphone driver. Re-run this command from an elevated PowerShell or Command Prompt.\n";
    return false;
  }

  bool get_inf_class(const std::wstring &inf_path, GUID &class_guid, std::vector<wchar_t> &class_name) {
    DWORD required_size = 0;
    class_name.assign(MAX_CLASS_NAME_LEN + 1, L'\0');
    if (!SetupDiGetINFClassW(inf_path.c_str(), &class_guid, class_name.data(), static_cast<DWORD>(class_name.size()), &required_size)) {
      print_windows_error(L"SetupDiGetINFClassW() failed", GetLastError());
      return false;
    }

    if (required_size == 0) {
      std::wcerr << L"SetupDiGetINFClassW() returned an empty class name\n";
      return false;
    }

    class_name.resize(required_size);
    return true;
  }

  bool stage_driver_package(const std::wstring &inf_path) {
    if (SetupCopyOEMInfW(inf_path.c_str(), nullptr, SPOST_PATH, SP_COPY_NEWER, nullptr, 0, nullptr, nullptr)) {
      return true;
    }

    const DWORD error = GetLastError();
    if (error == ERROR_FILE_EXISTS) {
      return true;
    }

    print_windows_error(L"SetupCopyOEMInfW() failed", error);
    return false;
  }

  bool contains_hardware_id(const std::vector<wchar_t> &buffer) {
    const wchar_t *ptr = buffer.data();
    while (*ptr != L'\0') {
      if (_wcsicmp(ptr, kHardwareId.data()) == 0) {
        return true;
      }
      ptr += wcslen(ptr) + 1;
    }

    return false;
  }

  bool get_hardware_ids(HDEVINFO device_info_set, SP_DEVINFO_DATA &device_info, std::vector<wchar_t> &buffer) {
    DWORD required_size = 0;
    if (SetupDiGetDeviceRegistryPropertyW(device_info_set, &device_info, SPDRP_HARDWAREID, nullptr, nullptr, 0, &required_size)) {
      return false;
    }

    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || required_size == 0) {
      return false;
    }

    buffer.resize(required_size / sizeof(wchar_t));
    return SetupDiGetDeviceRegistryPropertyW(
      device_info_set,
      &device_info,
      SPDRP_HARDWAREID,
      nullptr,
      reinterpret_cast<PBYTE>(buffer.data()),
      required_size,
      nullptr
    ) == TRUE;
  }

  int remove_existing_devices() {
    if (!ensure_elevated(L"remove")) {
      return 1;
    }

    HDEVINFO device_info_set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES);
    if (device_info_set == INVALID_HANDLE_VALUE) {
      print_windows_error(L"SetupDiGetClassDevsW() failed", GetLastError());
      return 1;
    }

    int removed_count = 0;
    for (DWORD index = 0;; ++index) {
      SP_DEVINFO_DATA device_info {};
      device_info.cbSize = sizeof(device_info);

      if (!SetupDiEnumDeviceInfo(device_info_set, index, &device_info)) {
        if (GetLastError() == ERROR_NO_MORE_ITEMS) {
          break;
        }
        continue;
      }

      std::vector<wchar_t> hardware_ids;
      if (!get_hardware_ids(device_info_set, device_info, hardware_ids) || !contains_hardware_id(hardware_ids)) {
        continue;
      }

      SP_REMOVEDEVICE_PARAMS remove_params {};
      remove_params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
      remove_params.ClassInstallHeader.InstallFunction = DIF_REMOVE;
      remove_params.Scope = DI_REMOVEDEVICE_GLOBAL;

      if (!SetupDiSetClassInstallParamsW(device_info_set, &device_info, &remove_params.ClassInstallHeader, sizeof(remove_params))) {
        print_windows_error(L"SetupDiSetClassInstallParamsW(DIF_REMOVE) failed", GetLastError());
        continue;
      }

      if (!SetupDiCallClassInstaller(DIF_REMOVE, device_info_set, &device_info)) {
        print_windows_error(L"SetupDiCallClassInstaller(DIF_REMOVE) failed", GetLastError());
        continue;
      }

      removed_count++;
    }

    SetupDiDestroyDeviceInfoList(device_info_set);
    return removed_count;
  }

  int install_driver(const std::wstring &inf_path) {
    if (!ensure_elevated(L"install")) {
      return 1;
    }

    if (GetFileAttributesW(inf_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
      print_windows_error(L"INF path is not accessible", GetLastError());
      return 1;
    }

    remove_existing_devices();

    GUID class_guid {};
    std::vector<wchar_t> class_name;
    if (!get_inf_class(inf_path, class_guid, class_name)) {
      return 1;
    }

    if (!stage_driver_package(inf_path)) {
      return 1;
    }

    HDEVINFO device_info_set = SetupDiCreateDeviceInfoList(&class_guid, nullptr);
    if (device_info_set == INVALID_HANDLE_VALUE) {
      print_windows_error(L"SetupDiCreateDeviceInfoList() failed", GetLastError());
      return 1;
    }

    SP_DEVINFO_DATA device_info {};
    device_info.cbSize = sizeof(device_info);

    if (!SetupDiCreateDeviceInfoW(device_info_set, kRootDeviceId.data(), &class_guid, L"Apollo Virtual Microphone", nullptr, DICD_GENERATE_ID, &device_info)) {
      print_windows_error(L"SetupDiCreateDeviceInfoW() failed", GetLastError());
      SetupDiDestroyDeviceInfoList(device_info_set);
      return 1;
    }

    constexpr wchar_t hardware_ids[] = L"ROOT\\VirtualAudioDriver\0\0";
    if (!SetupDiSetDeviceRegistryPropertyW(device_info_set, &device_info, SPDRP_HARDWAREID, reinterpret_cast<const BYTE *>(hardware_ids), sizeof(hardware_ids))) {
      print_windows_error(L"SetupDiSetDeviceRegistryPropertyW(SPDRP_HARDWAREID) failed", GetLastError());
      SetupDiDestroyDeviceInfoList(device_info_set);
      return 1;
    }

    if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, device_info_set, &device_info)) {
      print_windows_error(L"SetupDiCallClassInstaller(DIF_REGISTERDEVICE) failed", GetLastError());
      SetupDiDestroyDeviceInfoList(device_info_set);
      return 1;
    }

    BOOL reboot_required = FALSE;
    if (!UpdateDriverForPlugAndPlayDevicesW(nullptr, kHardwareId.data(), inf_path.c_str(), kInstallFlags, &reboot_required)) {
      const auto error = GetLastError();
      print_windows_error(L"UpdateDriverForPlugAndPlayDevicesW() failed", error);
      SetupDiDestroyDeviceInfoList(device_info_set);
      return 1;
    }

    SetupDiDestroyDeviceInfoList(device_info_set);

    std::wcout << L"Apollo virtual microphone driver installed";
    if (reboot_required) {
      std::wcout << L" (reboot required)";
    }
    std::wcout << L"\n";

    return 0;
  }
}  // namespace

int wmain(int argc, wchar_t **argv) {
  if (argc < 2) {
    std::wcerr << L"Usage: apollovmicctl <install <inf-path>|uninstall>\n";
    return 1;
  }

  std::wstring command = argv[1];
  std::transform(command.begin(), command.end(), command.begin(), ::towlower);

  if (command == L"install") {
    if (argc < 3) {
      std::wcerr << L"Missing INF path for install\n";
      return 1;
    }

    return install_driver(argv[2]);
  }

  if (command == L"uninstall") {
    const int removed_count = remove_existing_devices();
    std::wcout << L"Removed " << removed_count << L" Apollo virtual microphone device(s)\n";
    return 0;
  }

  std::wcerr << L"Unknown command: " << command << L"\n";
  return 1;
}
