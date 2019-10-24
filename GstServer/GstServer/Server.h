#pragma once

#include "Definition.h"


#ifdef __cplusplus
extern "C" {
#endif

	typedef void(*event_cb_t)();

	EXPORT_API void Init(event_cb_t cb,const char* confFile);
	EXPORT_API void RTSPStream(bool video_enabled, bool audio_enabled, bool auth);
	EXPORT_API void FeedData(unsigned char* data, int  sizeInBytes);
	EXPORT_API void RTSPClose();
	EXPORT_API void UDPStream(bool video_enabled, bool audio_enabled);
	EXPORT_API void UDPStop();
	
#ifdef __cplusplus
}
#endif 