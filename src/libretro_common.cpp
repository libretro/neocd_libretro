#include "libretro_common.h"
#include "neogeocd.h"

// Collection of callbacks to the things we need
LibretroCallbacks libretro;

// Collection of global variables
Globals globals;

// Pointer to the emulator object
NeoGeoCD* neocd = nullptr;
