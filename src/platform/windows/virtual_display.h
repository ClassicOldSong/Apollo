#pragma once

#ifndef FILE_DEVICE_UNKNOWN
#define FILE_DEVICE_UNKNOWN 0x00000022
#endif

#include <ddk/d4iface.h>
#include <ddk/d4drvif.h>
#include <sudovda/sudovda.h>

namespace VDISPLAY {
	extern HANDLE SUDOVDA_DRIVER_HANDLE;

	LONG getDeviceSettings(const wchar_t* deviceName, DEVMODEW& devMode);
	LONG changeDisplaySettings(const wchar_t* deviceName, int width, int height, int refresh_rate);
	bool setPrimaryDisplay(const wchar_t* primaryDeviceName);

	bool startPingThread();
	bool openVDisplayDevice();
	std::wstring createVirtualDisplay(
		const char* s_client_uid,
		const char* s_client_name,
		const char* s_app_name,
		uint32_t width,
		uint32_t height,
		uint32_t fps,
		GUID& guid
	);
	bool removeVirtualDisplay(const GUID& guid);
}
