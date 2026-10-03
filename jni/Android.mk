LOCAL_PATH := $(call my-dir)
NEED_RWAV      := 1
NEED_RVORBIS   := 1

CORE_DIR   := $(LOCAL_PATH)/..

include $(CORE_DIR)/Makefile.common

COREFLAGS := -DINLINE=inline -D__LIBRETRO__ $(LIBRETRO_COMMON_FLAGS)

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
LOCAL_ARM_MODE  := arm
include $(BUILD_SHARED_LIBRARY)
