#include <windows.h>
#include <iostream>
#include <vector>
#include <setupapi.h>
#include <initguid.h>
#include <combaseapi.h>
#include <thread>

#include <wrl/client.h>
#include <dxgi.h>
#include <highlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>

#include "virtual_display.h"

using namespace SUDOVDA;

namespace VDISPLAY {
// {dff7fd29-5b75-41d1-9731-b32a17a17104}
static const GUID DEFAULT_DISPLAY_GUID = { 0xdff7fd29, 0x5b75, 0x41d1, { 0x97, 0x31, 0xb3, 0x2a, 0x17, 0xa1, 0x71, 0x04 } };

HANDLE SUDOVDA_DRIVER_HANDLE = INVALID_HANDLE_VALUE;

LONG getDeviceSettings(const wchar_t* deviceName, DEVMODEW& devMode) {
	devMode.dmSize = sizeof(DEVMODEW);
	return EnumDisplaySettingsW(deviceName, ENUM_CURRENT_SETTINGS, &devMode);
}

LONG changeDisplaySettings2(const wchar_t* deviceName, int width, int height, int refresh_rate) {
	UINT32 pathCount = 0;
	UINT32 modeCount = 0;
	if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount)) {
		wprintf(L"[SUDOVDA] Failed to query display configuration size.\n");
		return ERROR_INVALID_PARAMETER;
	}

	std::vector<DISPLAYCONFIG_PATH_INFO> pathArray(pathCount);
	std::vector<DISPLAYCONFIG_MODE_INFO> modeArray(modeCount);

	if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, pathArray.data(), &modeCount, modeArray.data(), nullptr) != ERROR_SUCCESS) {
		wprintf(L"[SUDOVDA] Failed to query display configuration.\n");
		return ERROR_INVALID_PARAMETER;
	}

	for (UINT32 i = 0; i < pathCount; i++) {
		DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName = {};
		sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
		sourceName.header.size = sizeof(sourceName);
		sourceName.header.adapterId = pathArray[i].sourceInfo.adapterId;
		sourceName.header.id = pathArray[i].sourceInfo.id;

		if (DisplayConfigGetDeviceInfo(&sourceName.header) != ERROR_SUCCESS) {
			continue;
		}

		auto* sourceInfo = &pathArray[i].sourceInfo;
		auto* targetInfo = &pathArray[i].targetInfo;

		if (std::wstring_view(sourceName.viewGdiDeviceName) == std::wstring_view(deviceName)) {
			wprintf(L"[SUDOVDA] Display found: %ls\n", deviceName);
			for (UINT32 j = 0; j < modeCount; j++) {
				if (
					modeArray[j].infoType == DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE &&
					modeArray[j].adapterId.HighPart == sourceInfo->adapterId.HighPart &&
					modeArray[j].adapterId.LowPart == sourceInfo->adapterId.LowPart &&
					modeArray[j].id == sourceInfo->id
				) {
					auto* sourceMode = &modeArray[j].sourceMode;

					wprintf(L"[SUDOVDA] Current mode found: [%dx%dx%d]\n", sourceMode->width, sourceMode->height, targetInfo->refreshRate);

					sourceMode->width = width;
					sourceMode->height = height;

					targetInfo->refreshRate = {(UINT32)refresh_rate, 1000};

					// Apply the changes
					LONG status = SetDisplayConfig(
						pathCount,
						pathArray.data(),
						modeCount,
						modeArray.data(),
						SDC_APPLY
						| SDC_USE_SUPPLIED_DISPLAY_CONFIG
						| SDC_SAVE_TO_DATABASE
					);
					if (status != ERROR_SUCCESS) {
						wprintf(L"[SUDOVDA] Failed to apply display settings.\n");
					} else {
						wprintf(L"[SUDOVDA] Display settings updated successfully.\n");
					}

					return status;
				}
			}

			wprintf(L"[SUDOVDA] Mode [%dx%dx%d] not found for display: %ls\n", width, height, refresh_rate, deviceName);
			return ERROR_INVALID_PARAMETER;
		}
	}

	wprintf(L"[SUDOVDA] Display not found: %ls\n", deviceName);
	return ERROR_DEVICE_NOT_CONNECTED;
}

LONG changeDisplaySettings(const wchar_t* deviceName, int width, int height, int refresh_rate) {
	DEVMODEW devMode = {0};
	devMode.dmSize = sizeof(devMode);

	// Old method to set at least baseline refresh rate
	if (EnumDisplaySettingsW(deviceName, ENUM_CURRENT_SETTINGS, &devMode)) {
		DWORD targetRefreshRate = refresh_rate / 1000;
		DWORD altRefreshRate = targetRefreshRate;

		if (refresh_rate % 1000) {
			if (refresh_rate % 1000 >= 900) {
				targetRefreshRate += 1;
			} else {
				altRefreshRate += 1;
			}
		} else {
			altRefreshRate -= 1;
		}

		wprintf(L"[SUDOVDA] Applying baseline display mode [%dx%dx%d] for %ls.\n", width, height, targetRefreshRate, deviceName);

		devMode.dmPelsWidth = width;
		devMode.dmPelsHeight = height;
		devMode.dmDisplayFrequency = targetRefreshRate;
		devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

		auto res = ChangeDisplaySettingsExW(deviceName, &devMode, NULL, CDS_UPDATEREGISTRY, NULL);

		if (res != ERROR_SUCCESS) {
			wprintf(L"[SUDOVDA] Failed to apply baseline display mode, trying alt mode: [%dx%dx%d].\n", width, height, altRefreshRate);
			devMode.dmDisplayFrequency = altRefreshRate;
			res = ChangeDisplaySettingsExW(deviceName, &devMode, NULL, CDS_UPDATEREGISTRY, NULL);
			if (res != ERROR_SUCCESS) {
				wprintf(L"[SUDOVDA] Failed to apply alt baseline display mode.\n");
			}
		}

		if (res == ERROR_SUCCESS) {
			wprintf(L"[SUDOVDA] Baseline display mode applied successfully.");
		}
	}

	// Use new method to set refresh rate if fine tuned
	return changeDisplaySettings2(deviceName, width, height, refresh_rate);
}


std::wstring getPrimaryDisplay() {
	DISPLAY_DEVICEW displayDevice;
	displayDevice.cb = sizeof(DISPLAY_DEVICE);

	std::wstring primaryDeviceName;

	int deviceIndex = 0;
	while (EnumDisplayDevicesW(NULL, deviceIndex, &displayDevice, 0)) {
		if (displayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
			primaryDeviceName = displayDevice.DeviceName;
			break;
		}
		deviceIndex++;
	}

	return primaryDeviceName;
}

bool setPrimaryDisplay(const wchar_t* primaryDeviceName) {
	DEVMODEW primaryDevMode{};
	if (!getDeviceSettings(primaryDeviceName, primaryDevMode)) {
		return false;
	};

	int offset_x = primaryDevMode.dmPosition.x;
	int offset_y = primaryDevMode.dmPosition.y;

	LONG result;

	DISPLAY_DEVICEW displayDevice;
	displayDevice.cb = sizeof(DISPLAY_DEVICEA);
	int device_index = 0;

	while (EnumDisplayDevicesW(NULL, device_index, &displayDevice, 0)) {
		device_index++;
		if (!(displayDevice.StateFlags & DISPLAY_DEVICE_ACTIVE)) {
			continue;
		}

		DEVMODEW devMode{};
		if (getDeviceSettings(displayDevice.DeviceName, devMode)) {
			devMode.dmPosition.x -= offset_x;
			devMode.dmPosition.y -= offset_y;
			devMode.dmFields = DM_POSITION;

			result = ChangeDisplaySettingsExW(displayDevice.DeviceName, &devMode, NULL, CDS_UPDATEREGISTRY | CDS_NORESET, NULL);
			if (result != DISP_CHANGE_SUCCESSFUL) {
				wprintf(L"[SUDOVDA] Changing config for display %ls failed!\n\n", displayDevice.DeviceName);
				return false;
			}
		}
	}

	// Update primary device's config to ensure it's primary
	primaryDevMode.dmPosition.x = 0;
	primaryDevMode.dmPosition.y = 0;
	primaryDevMode.dmFields = DM_POSITION;
	result = ChangeDisplaySettingsExW(primaryDeviceName, &primaryDevMode, NULL, CDS_UPDATEREGISTRY | CDS_NORESET | CDS_SET_PRIMARY, NULL);
	if (result != DISP_CHANGE_SUCCESSFUL) {
		wprintf(L"[SUDOVDA] Changing config for primary display %ls failed!\n\n", primaryDeviceName);
		return false;
	}

	wprintf(L"[SUDOVDA] Applying primary display %ls ...\n\n", primaryDeviceName);

	result = ChangeDisplaySettingsExW(NULL, NULL, NULL, 0, NULL);
	if (result != DISP_CHANGE_SUCCESSFUL) {
		wprintf(L"[SUDOVDA] Applying display coinfig failed!\n\n");
		return false;
	}

	return true;
}

bool findDisplayIds(const wchar_t* displayName, LUID& adapterId, uint32_t& targetId) {
	UINT32 pathCount;
	UINT32 modeCount;
	if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount)) {
		return false;
	}

	std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
	std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
	if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr)) {
		return false;
	}

	auto path = std::find_if(paths.begin(), paths.end(), [&displayName](DISPLAYCONFIG_PATH_INFO _path) {
		DISPLAYCONFIG_PATH_SOURCE_INFO sourceInfo = _path.sourceInfo;

		DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName = {};
		sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
		sourceName.header.size = sizeof(sourceName);
		sourceName.header.adapterId = sourceInfo.adapterId;
		sourceName.header.id = sourceInfo.id;

		if (DisplayConfigGetDeviceInfo(&sourceName.header) != ERROR_SUCCESS) {
			return false;
		}

		return std::wstring_view(displayName) == sourceName.viewGdiDeviceName;
	});

	if (path == paths.end()) {
		return false;
	}

	adapterId = path->sourceInfo.adapterId;
	targetId = path->targetInfo.id;

	return true;
}

bool getDisplayHDR(const LUID& adapterId, const uint32_t& targetId) {
	DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO hdrInfo = {};
	hdrInfo.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
	hdrInfo.header.size = sizeof(hdrInfo);
	hdrInfo.header.adapterId = adapterId;
	hdrInfo.header.id = targetId;

	if (DisplayConfigGetDeviceInfo(&hdrInfo.header) != ERROR_SUCCESS) {
		return false;
	}

	return hdrInfo.advancedColorSupported && hdrInfo.advancedColorEnabled;
}

bool setDisplayHDR(const LUID& adapterId, const uint32_t& targetId, bool enableAdvancedColor) {
	DISPLAYCONFIG_SET_ADVANCED_COLOR_STATE setHdrInfo = {};
	setHdrInfo.header.type = DISPLAYCONFIG_DEVICE_INFO_SET_ADVANCED_COLOR_STATE;
	setHdrInfo.header.size = sizeof(setHdrInfo);
	setHdrInfo.header.adapterId = adapterId;
	setHdrInfo.header.id = targetId;
	setHdrInfo.enableAdvancedColor = enableAdvancedColor;

	return DisplayConfigSetDeviceInfo(&setHdrInfo.header) == ERROR_SUCCESS;
}

bool getDisplayHDRByName(const wchar_t* displayName) {
	LUID adapterId;
	uint32_t targetId;

	if (!findDisplayIds(displayName, adapterId, targetId)) {
		return false;
	}

	return getDisplayHDR(adapterId, targetId);
}

bool setDisplayHDRByName(const wchar_t* displayName, bool enableAdvancedColor) {
	LUID adapterId;
	uint32_t targetId;

	if (!findDisplayIds(displayName, adapterId, targetId)) {
		return false;
	}

	return setDisplayHDR(adapterId, targetId, enableAdvancedColor);
}

void closeVDisplayDevice() {
	if (SUDOVDA_DRIVER_HANDLE == INVALID_HANDLE_VALUE) {
		return;
	}

	CloseHandle(SUDOVDA_DRIVER_HANDLE);

	SUDOVDA_DRIVER_HANDLE = INVALID_HANDLE_VALUE;
}

DRIVER_STATUS openVDisplayDevice() {
	uint32_t retryInterval = 20;
	while (true) {
		SUDOVDA_DRIVER_HANDLE = OpenDevice(&SUVDA_INTERFACE_GUID);
		if (SUDOVDA_DRIVER_HANDLE == INVALID_HANDLE_VALUE) {
			if (retryInterval > 320) {
				printf("[SUDOVDA] Open device failed!\n");
				return DRIVER_STATUS::FAILED;
			}
			retryInterval *= 2;
			Sleep(retryInterval);
			continue;
		}

		break;
	}

	if (!CheckProtocolCompatible(SUDOVDA_DRIVER_HANDLE)) {
		printf("[SUDOVDA] SUDOVDA protocol not compatible with driver!\n");
		closeVDisplayDevice();
		return DRIVER_STATUS::VERSION_INCOMPATIBLE;
	}

	return DRIVER_STATUS::OK;
}

bool startPingThread(std::function<void()> failCb) {
	if (SUDOVDA_DRIVER_HANDLE == INVALID_HANDLE_VALUE) {
		return false;
	}

	VIRTUAL_DISPLAY_GET_WATCHDOG_OUT watchdogOut;
	if (GetWatchdogTimeout(SUDOVDA_DRIVER_HANDLE, watchdogOut)) {
		printf("[SUDOVDA] Watchdog: Timeout %d, Countdown %d\n", watchdogOut.Timeout, watchdogOut.Countdown);
	} else {
		printf("[SUDOVDA] Watchdog fetch failed!\n");
		return false;
	}

	if (watchdogOut.Timeout) {
		auto sleepInterval = watchdogOut.Timeout * 1000 / 3;
		std::thread ping_thread([sleepInterval, failCb = std::move(failCb)]{
			uint8_t fail_count = 0;
			for (;;) {
				if (!sleepInterval) return;
				if (!PingDriver(SUDOVDA_DRIVER_HANDLE)) {
					fail_count += 1;
					if (fail_count > 3) {
						failCb();
						return;
					}
				};
				Sleep(sleepInterval);
			}
		});

		ping_thread.detach();
	}

	return true;
}

bool setRenderAdapterByName(const std::wstring& adapterName) {
	if (SUDOVDA_DRIVER_HANDLE == INVALID_HANDLE_VALUE) {
		return false;
	}

	Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
	if (!SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
		return false;
	}

	Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
	DXGI_ADAPTER_DESC desc;
	int i = 0;
	while (SUCCEEDED(factory->EnumAdapters(i, &adapter))) {
		i += 1;

		if (!SUCCEEDED(adapter->GetDesc(&desc))) {
			continue;
		}

		if (std::wstring_view(desc.Description) != adapterName) {
			continue;
		}

		if (SetRenderAdapter(SUDOVDA_DRIVER_HANDLE, desc.AdapterLuid)) {
			return true;
		}
	}

	return false;
}

std::wstring createVirtualDisplay(
	const char* s_client_uid,
	const char* s_client_name,
	uint32_t width,
	uint32_t height,
	uint32_t fps,
	const GUID& guid
) {
	if (SUDOVDA_DRIVER_HANDLE == INVALID_HANDLE_VALUE) {
		return std::wstring();
	}

	VIRTUAL_DISPLAY_ADD_OUT output;
	if (!AddVirtualDisplay(SUDOVDA_DRIVER_HANDLE, width, height, fps, guid, s_client_name, s_client_uid, output)) {
		printf("[SUDOVDA] Failed to add virtual display.\n");
		return std::wstring();
	}

	uint32_t retryInterval = 20;
	wchar_t deviceName[CCHDEVICENAME]{};
	while (!GetAddedDisplayName(output, deviceName)) {
		Sleep(retryInterval);
		if (retryInterval > 320) {
			printf("[SUDOVDA] Cannot get name for newly added virtual display!\n");
			return std::wstring();
		}
		retryInterval *= 2;
	}

	wprintf(L"[SUDOVDA] Virtual display added successfully: %ls\n", deviceName);
	printf("[SUDOVDA] Configuration: W: %d, H: %d, FPS: %d\n", width, height, fps);

	return std::wstring(deviceName);
}

bool removeVirtualDisplay(const GUID& guid) {
	if (SUDOVDA_DRIVER_HANDLE == INVALID_HANDLE_VALUE) {
		return false;
	}

	if (RemoveVirtualDisplay(SUDOVDA_DRIVER_HANDLE, guid)) {
		printf("[SUDOVDA] Virtual display removed successfully.\n");
		return true;
	} else {
		return false;
	}
}
}