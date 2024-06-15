LOCAL_PATH := $(call my-dir)

CORE_DIR   := $(LOCAL_PATH)/..

include $(CORE_DIR)/Makefile.common

COREFLAGS := -DINLINE=inline -D__LIBRETRO__
COREFLAGS += -DHAVE_CHD -DUSE_FILE32API -DHAVE_ZLIB -DZ7_ST -DZSTD_DISABLE_ASM
COREFLAGS += -DHAVE_FLAC -DFLAC__HAS_OGG=0 -DFLAC_API_EXPORTS -DFLAC__NO_DLL
COREFLAGS += -DHAVE_LROUND -DHAVE_STDINT_H -DHAVE_STDLIB_H -DHAVE_SYS_PARAM_H

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
   COREFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

include $(CLEAR_VARS)
LOCAL_MODULE    := retro
LOCAL_SRC_FILES := $(SOURCES_CXX) $(SOURCES_C)
LOCAL_CXXFLAGS  := -fomit-frame-pointer -std=c++11 -fno-exceptions -fno-rtti $(COREFLAGS) $(INCFLAGS)
LOCAL_CFLAGS    := -fomit-frame-pointer $(COREFLAGS) $(INCFLAGS)
LOCAL_LDFLAGS   := -Wl,-version-script=$(CORE_DIR)/link.T
LOCAL_LDLIBS    := -lz
LOCAL_ARM_MODE  := arm
include $(BUILD_SHARED_LIBRARY)
