#pragma once
#ifdef _WIN32
  #ifdef ARDENT_RUNTIME_EXPORTS
    #define ARDENT_RT_API __declspec(dllexport)
  #else
    #define ARDENT_RT_API __declspec(dllimport)
  #endif
#else
  #define ARDENT_RT_API
#endif

extern "C" {
// Placeholder C ABI for future runtime functions.
// Allocate an arena-backed phrase (future implementation).
ARDENT_RT_API const char* ardent_rt_version();
ARDENT_RT_API int ardent_rt_add(int a, int b);
}
