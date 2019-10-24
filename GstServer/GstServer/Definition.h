#pragma once

#if defined (_MSC_VER)
#if defined (DLL_EXPORTS)
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif
#else
#define MY_EXPORT
#endif
