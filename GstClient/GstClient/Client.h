#pragma once

#include "Definition.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef void(*event_cb_t)();
	typedef void(*frame_cb_t)(unsigned char*,int width, int height, int bpp);

	EXPORT_API void Init(event_cb_t start_cb, event_cb_t stop_cb, frame_cb_t frame_cb, const char* conf_file );
	EXPORT_API void RTSPStream(bool video_enabled, bool audio_enabled, bool auth);
	EXPORT_API void RTSPClose();

	EXPORT_API void UDPStream(bool video_enabled, bool audio_enabled);
	EXPORT_API void UDPClose();

#ifdef __cplusplus
}
#endif