#pragma once

#include "Definition.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef void(*FFCallBack)();
	
	EXPORT_API void RTPInit(
		int width,
		int height,
		int framerate,
		const char* hostname,
		int length,
		int port,
		FFCallBack callback);
	
	EXPORT_API void RTPStreamVideo();
	EXPORT_API void RTPFeedData(unsigned char* data, int sizeInBytes);
	EXPORT_API void RTSClose();

#ifdef __cplusplus
}
#endif