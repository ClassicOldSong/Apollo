#include <windows.h>
#include <iostream>
#include <vector>
#include <setupapi.h>
#include <initguid.h>
#include <combaseapi.h>
#include <thread>

#include <wrl/client.h>
#include <dxgi.h>

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

LONG changeDisplaySettings(const wchar_t* deviceName, int width, int height, int refresh_rate) {
	DEVMODEW devMode = {0};
	devMode.dmSize = sizeof(devMode);

	if (EnumDisplaySettingsW(deviceName, ENUM_CURRENT_SETTINGS, &devMode)) {
		devMode.dmPelsWidth = width;
		devMode.dmPelsHeight = height;
		devMode.dmDisplayFrequency = refresh_rate;
		devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

		return ChangeDisplaySettingsExW(deviceName, &devMode, NULL, CDS_UPDATEREGISTRY, NULL);
	}

	return 0;
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
				return false;
			}
		}
	}

	// Update primary device's config to ensure it's primary
	primaryDevMode.dmPosition.x = 0;
	primaryDevMode.dmPosition.y = 0;
	primaryDevMode.dmFields = DM_POSITION;
	ChangeDisplaySettingsExW(primaryDeviceName, &primaryDevMode, NULL, CDS_UPDATEREGISTRY | CDS_NORESET | CDS_SET_PRIMARY, NULL);

	result = ChangeDisplaySettingsExW(NULL, NULL, NULL, 0, NULL);
	if (result != DISP_CHANGE_SUCCESSFUL) {
		return false;
	}

	return true;
}

void closeVDisplayDevice() {
	if (SUDOVDA_DRIVER_HANDLE == INVALID_HANDLE_VALUE) {
		return;
	}

	CloseHandle(SUDOVDA_DRIVER_HANDLE);

	SUDOVDA_DRIVER_HANDLE = INVALID_HANDLE_VALUE;
}

DRIVER_STATUS openVDisplayDevice() {
	SUDOVDA_DRIVER_HANDLE = OpenDevice(&SUVDA_INTERFACE_GUID);
	if (SUDOVDA_DRIVER_HANDLE == INVALID_HANDLE_VALUE) {
		return DRIVER_STATUS::FAILED;
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
	const char* s_app_name,
	uint32_t width,
	uint32_t height,
	uint32_t fps,
	GUID& guid
) {
	if (SUDOVDA_DRIVER_HANDLE == INVALID_HANDLE_VALUE) {
		return std::wstring();
	}

	if (!s_app_name || !strlen(s_app_name) || !strcmp(s_app_name, "unknown")) {
		s_app_name = "ApolloVDisp";
	}

	if (!s_client_name || !strlen(s_client_name) || !strcmp(s_client_name, "unknown")) {
		s_client_name = s_app_name;
	}

	if (s_client_uid && strcmp(s_client_uid, "unknown")) {
		size_t len = strlen(s_client_uid);
		if (len > sizeof(GUID)) {
			len = sizeof(GUID);
		}
		memcpy((void*)&guid, (void*)s_client_uid, len);
	} else {
		s_client_uid = "unknown";
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
		if (retryInterval > 160) {
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