TOP_LOCAL_PATH := $(call my-dir)

include $(TOP_LOCAL_PATH)/src/ext/libevent/Android.mk
include $(TOP_LOCAL_PATH)/src/ext/yaml-cpp/src/Android.mk

LOCAL_PATH := $(TOP_LOCAL_PATH)

include $(CLEAR_VARS)

LOCAL_STATIC_LIBRARIES := event2 \
    yaml-cpp

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/src \
    src/ext/libevent/include \
    src/ext/libevent/android \
    $(LOCAL_PATH)/src/ext/yaml-cpp/include \
    $(LOCAL_PATH)/src/ext/boost/smart_ptr/include \
    $(LOCAL_PATH)/src/ext/boost/config/include \
    $(LOCAL_PATH)/src/ext/boost/assert/include \
    $(LOCAL_PATH)/src/ext/boost/core/include \
    $(LOCAL_PATH)/src/ext/boost/throw_exception/include \
    $(LOCAL_PATH)/src/ext/boost/iterator/include \
    $(LOCAL_PATH)/src/ext/boost/mpl/include \
    $(LOCAL_PATH)/src/ext/boost/preprocessor/include \
    $(LOCAL_PATH)/src/ext/boost/type_traits/include \
    $(LOCAL_PATH)/src/ext/boost/static_assert/include \
    $(LOCAL_PATH)/src/ext/boost/detail/include \
    $(LOCAL_PATH)/src/ext/boost/utility/include

LOCAL_CFLAGS := $(LOCAL_C_INCLUDES:%=-I%)

LOCAL_MODULE:= ight

LOCAL_CFLAGS := -DIGHT_ANDROID

LOCAL_SRC_FILES := \
    src/common/log.c \
    src/common/poller.cpp \
    src/common/stringvector.cpp \
    src/common/utils.c \
    src/ext/strtonum.c \
    src/net/connection.cpp \
    src/report/base.cpp \
    src/report/file.cpp

include $(BUILD_STATIC_LIBRARY)
