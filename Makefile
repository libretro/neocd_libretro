STATIC_LINKING := 0
AR             := ar

ifneq ($(V),1)
   Q := @
endif

ifneq ($(SANITIZER),)
   CFLAGS   := -fsanitize=$(SANITIZER) $(CFLAGS)
   CXXFLAGS := -fsanitize=$(SANITIZER) $(CXXFLAGS)
   LDFLAGS  := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
else ifneq ($(findstring win,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring MSYS,$(MSYSTEM)),)
   platform = win
endif
endif

# system platform
system_platform = unix
ifeq ($(shell uname -a),)
	EXE_EXT = .exe
	system_platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
	system_platform = osx
	arch = intel
ifeq ($(shell uname -p),arm)
	arch = arm
endif
ifeq ($(shell uname -p),powerpc)
	arch = ppc
endif
else ifneq ($(findstring MINGW,$(shell uname -a)),)
	system_platform = win
endif

CORE_DIR    += .
TARGET_NAME := neocd
LIBM		    = -lm

ifeq ($(STATIC_LINKING), 1)
EXT := a
endif

ifneq (,$(findstring unix,$(platform)))
	EXT ?= so
   TARGET := $(TARGET_NAME)_libretro.$(EXT)
   fpic := -fPIC
   SHARED := -shared -Wl,--version-script=$(CORE_DIR)/link.T -Wl,--no-undefined
   LIBS += -lpthread
else ifeq ($(platform), linux-portable)
   TARGET := $(TARGET_NAME)_libretro.$(EXT)
   fpic := -fPIC -nostdlib
   SHARED := -shared -Wl,--version-script=$(CORE_DIR)/link.T
	LIBM :=
else ifneq (,$(findstring osx,$(platform)))
   TARGET := $(TARGET_NAME)_libretro.dylib
   fpic := -fPIC
   SHARED := -dynamiclib

ifeq ($(UNIVERSAL),1)
ifeq ($(ARCHFLAGS),)
ARCHFLAGS = -arch i386 -arch x86_64
ifeq ($(shell uname -p),powerpc)
   ARCHFLAGS = -arch ppc -arch ppc64
endif
ifeq ($(shell uname -p),arm)
   ARCHFLAGS = -arch arm64
endif
endif
   CFLAGS += $(ARCHFLAGS)
   CXXFLAGS += $(ARCHFLAGS)
   LFLAGS += $(ARCHFLAGS)
endif

   ifeq ($(CROSS_COMPILE),1)
		TARGET_RULE   = -target $(LIBRETRO_APPLE_PLATFORM) -isysroot $(LIBRETRO_APPLE_ISYSROOT)
		CFLAGS   += $(TARGET_RULE)
		CPPFLAGS += $(TARGET_RULE)
		CXXFLAGS += $(TARGET_RULE)
		LDFLAGS  += $(TARGET_RULE)
   endif

else ifneq (,$(findstring ios,$(platform)))
   TARGET := $(TARGET_NAME)_libretro_ios.dylib
	fpic := -fPIC
	SHARED := -dynamiclib
	DEFINES := -DIOS
  MINVERSION=

ifeq ($(IOSSDK),)
   IOSSDK := $(shell xcodebuild -version -sdk iphoneos Path)
endif
ifeq ($(platform),ios-arm64)
  CC = clang -arch arm64 -isysroot $(IOSSDK) -stdlib=libc++
  CXX = clang++ -arch arm64 -isysroot $(IOSSDK) -stdlib=libc++
else
  CC = clang -arch armv7 -isysroot $(IOSSDK)
  CXX = clang++ -arch armv7 -isysroot $(IOSSDK)
endif

ifeq ($(platform),$(filter $(platform),ios9 ios-arm64))
  MINVERSION = -miphoneos-version-min=9.0
else
  MINVERSION = -miphoneos-version-min=5.0
endif
  CFLAGS       += $(MINVERSION)
  CXXFLAGS     += $(MINVERSION)

else ifeq ($(platform), tvos-arm64)
   TARGET := $(TARGET_NAME)_libretro_tvos.dylib
   fpic := -fPIC
   SHARED := -dynamiclib
   DEFINES := -DIOS -stdlib=libc++

ifeq ($(IOSSDK),)
   IOSSDK := $(shell xcodebuild -version -sdk appletvos Path)
endif
   CC  = cc -arch arm64  -isysroot $(IOSSDK)
   CXX = c++ -arch arm64 -isysroot $(IOSSDK)

else ifneq (,$(findstring qnx,$(platform)))
	TARGET := $(TARGET_NAME)_libretro_qnx.so
   fpic := -fPIC
   SHARED := -shared -Wl,--version-script=$(CORE_DIR)/link.T -Wl,--no-undefined
else ifeq ($(platform), emscripten)
   TARGET := $(TARGET_NAME)_libretro_emscripten.bc
   fpic := -fPIC
   SHARED := 
   CFLAGS += -DSYNC_CDROM=1
   CXXFLAGS += -DSYNC_CDROM=1
   STATIC_LINKING = 1
else ifeq ($(platform), libnx)
   include $(DEVKITPRO)/libnx/switch_rules
   TARGET := $(TARGET_NAME)_libretro_$(platform).a
   DEFINES := -DSWITCH=1 -D__SWITCH__ -DARM
   CFLAGS := $(DEFINES) -fPIE -I$(LIBNX)/include/ -ffunction-sections -fdata-sections -ftls-model=local-exec
   CFLAGS += -march=armv8-a -mtune=cortex-a57 -mtp=soft -mcpu=cortex-a57+crc+fp+simd -ffast-math
   CXXFLAGS := $(ASFLAGS) $(CFLAGS)
   STATIC_LINKING = 1
else ifeq ($(platform), vita)
   TARGET := $(TARGET_NAME)_libretro_$(platform).a
   CC = arm-vita-eabi-gcc
   CXX = arm-vita-eabi-g++
   AR = arm-vita-eabi-ar
   CFLAGS += -DVITA -march=armv7-a -mfpu=neon -mfloat-abi=hard -DSYNC_CDROM=1
   CXXFLAGS += -DVITA -Wl,-q -Wall  -march=armv7-a -mfpu=neon -mfloat-abi=hard -mword-relocations -DSYNC_CDROM=1
	STATIC_LINKING = 1
# Nintendo WiiU
else ifeq ($(platform), wiiu)
   TARGET := $(TARGET_NAME)_libretro_$(platform).a
   CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
   CXX = $(DEVKITPPC)/bin/powerpc-eabi-g++$(EXE_EXT)
   AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
   CFLAGS += -DGEKKO -DHW_RVL -DWIIU -mcpu=750 -meabi -mhard-float  -DSYNC_CDROM=1 -D_BSD_SOURCE
   CXXFLAGS += -DGEKKO -DHW_RVL -DWIIU -mcpu=750 -meabi -mhard-float  -DSYNC_CDROM=1 -D_BSD_SOURCE
   STATIC_LINKING=1
else ifeq ($(platform), ctr)
   TARGET := $(TARGET_NAME)_libretro_$(platform).a
   CC = $(DEVKITARM)/bin/arm-none-eabi-gcc$(EXE_EXT)
   CXX = $(DEVKITARM)/bin/arm-none-eabi-g++$(EXE_EXT)
   AR = $(DEVKITARM)/bin/arm-none-eabi-ar$(EXE_EXT)
   CFLAGS += -D_3DS -DARM11 -march=armv6k -mtune=mpcore -mfloat-abi=hard -DSYNC_CDROM=1 -D_BSD_SOURCE
   CXXFLAGS += -D_3DS -DARM11 -march=armv6k -mtune=mpcore -mfloat-abi=hard -DSYNC_CDROM=1 -D_BSD_SOURCE
   STATIC_LINKING = 1
# Lightweight PS3 Homebrew SDK
else ifeq ($(platform), psl1ght)
   EXT=a
   TARGET := $(TARGET_NAME)_libretro_$(platform).$(EXT)
   CC = $(PS3DEV)/ppu/bin/ppu-gcc$(EXE_EXT)
   CXX = $(PS3DEV)/ppu/bin/ppu-g++$(EXE_EXT)
   CC_AS = $(PS3DEV)/ppu/bin/ppu-gcc$(EXE_EXT)
   AR = $(PS3DEV)/ppu/bin/ppu-ar$(EXE_EXT)
   CFLAGS += -D__PSL1GHT__ -mcpu=cell -D_XOPEN_SOURCE=500  -DSYNC_CDROM=1
   CXXFLAGS += -D__PSL1GHT__ -mcpu=cell -DDISABLE_AUDIO_THREAD=1 -D_XOPEN_SOURCE=500  -DSYNC_CDROM=1
   STATIC_LINKING = 1
else
   CC ?= gcc
   TARGET := $(TARGET_NAME)_libretro.dll
   SHARED := -shared -static-libgcc -static-libstdc++ -s -Wl,--version-script=$(CORE_DIR)/link.T -Wl,--no-undefined
endif

LDFLAGS += $(LIBM)

ifeq ($(DEBUG), 1)
   CFLAGS += -O0 -g -DDEBUG
   CXXFLAGS += -O0 -g -DDEBUG
else ifeq ($(platform), emscripten)
   CFLAGS += -O2
   CXXFLAGS += -O2
else
   CFLAGS += -Ofast -DNDEBUG
   CXXFLAGS += -Ofast -DNDEBUG
endif

CFLAGS += -DHAVE_COMPRESSION -DHAVE_ZLIB -DHAVE_7ZIP -D_7ZIP_ST -DHAVE_FLAC
CXXFLAGS += -std=c++11 -fno-exceptions -fno-rtti

include Makefile.common

OBJECTS := $(SOURCES_C:.c=.o) $(SOURCES_CXX:.cpp=.o)

CFLAGS   += -Wall -D__LIBRETRO__ $(fpic) $(INCFLAGS) 
CXXFLAGS += -Wall -D__LIBRETRO__ $(fpic) $(INCFLAGS)

all: $(TARGET)

$(TARGET): $(OBJECTS)
ifeq ($(platform), emscripten)
	@$(if $(Q), $(shell echo echo LD $@),)
	$(CXX) $(fpic) -r $(SHARED) -o $@ $(OBJECTS) $(LIBS) $(LDFLAGS)
else ifeq ($(STATIC_LINKING), 1)
	$(AR) rcs $@ $(OBJECTS)
else
	@$(if $(Q), $(shell echo echo LD $@),)
	$(CXX) $(fpic) $(SHARED) -o $@ $(OBJECTS) $(LIBS) $(LDFLAGS)
endif


%.o: %.c
	$(CC) $(CFLAGS) $(fpic) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(fpic) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean
