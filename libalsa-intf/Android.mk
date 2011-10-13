LOCAL_PATH:= $(call my-dir)

ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)
# Any prebuilt files with default TAGS can use the below:
prebuilt_files := snd_soc_msm/HiFi snd_soc_msm/FM_Digital_Radio \
                  snd_soc_msm/HiFi_Rec snd_soc_msm/Voice_Call_IP \
		  snd_soc_msm/Voice_Call snd_soc_msm/FM_REC \
		  snd_soc_msm/HiFi_Low_Power snd_soc_msm/FM_A2DP_REC
$(call add-prebuilt-files, ETC, $(prebuilt_files))
include $(CLEAR_VARS)
#LOCAL_SRC_FILES:= aplay.c alsa_pcm.c alsa_mixer.c
LOCAL_SRC_FILES:= aplay.c
LOCAL_MODULE:= aplay
LOCAL_SHARED_LIBRARIES:= libc libcutils libalsa-intf
LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
LOCAL_MODULE_TAGS:= debug
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
#LOCAL_SRC_FILES:= arec.c alsa_pcm.c
LOCAL_SRC_FILES:= arec.c
LOCAL_MODULE:= arec
LOCAL_SHARED_LIBRARIES:= libc libcutils libalsa-intf
LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
LOCAL_MODULE_TAGS:= debug
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= amix.c
LOCAL_MODULE:= amix
LOCAL_SHARED_LIBRARIES := libc libcutils libalsa-intf
LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
LOCAL_MODULE_TAGS:= debug
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_COPY_HEADERS_TO   := mm-audio/libalsa-intf
LOCAL_COPY_HEADERS      := alsa_audio.h
LOCAL_COPY_HEADERS      += alsa_ucm.h
LOCAL_COPY_HEADERS      += msm8960_use_cases.h
LOCAL_SRC_FILES:= alsa_mixer.c alsa_pcm.c alsa_ucm.c
LOCAL_MODULE:= libalsa-intf
LOCAL_SHARED_LIBRARIES:= libc libcutils #libutils #libmedia libhardware_legacy
LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
ifeq ($(strip $(QC_PROP)),true)
LOCAL_CFLAGS := -DQC_PROP
LOCAL_SHARED_LIBRARIES += libacdbloader
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-audio/audio-acdb-util
endif
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
ifeq ($(TARGET_SIMULATOR),true)
 LOCAL_LDLIBS += -ldl
else
 LOCAL_SHARED_LIBRARIES += libdl
endif
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
endif
