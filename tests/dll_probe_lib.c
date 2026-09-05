#define AETHER_BUILD_DLL
#define AETHER_IMPLEMENTATION
#include "aether/aether.h"

/* Nothing else needed here: AETHER_BUILD_DLL makes every AETHER_API symbol
   dllexport, so compiling the implementation with it defined is the whole
   probe. dll_probe_consumer.c links against the resulting DLL and calls
   across the boundary with AETHER_DLL (dllimport) instead. */
