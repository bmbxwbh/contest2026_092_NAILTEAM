############################################################################
# Contest 2026 team 092 NAILTEAM smart speaker - Makefile
# Target: Gemini-S1 (R528, dual Cortex-A7)
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = smart_speaker
PRIORITY  = $(CONFIG_SMART_SPEAKER_PRIORITY)
STACKSIZE = $(CONFIG_SMART_SPEAKER_STACKSIZE)
MODULE    = $(CONFIG_EXAMPLES_SMART_SPEAKER)

MAINSRC = src/speaker_main.c

CSRCS = \
	src/device_state.c \
	src/display_manager.c \
	src/audio_pipeline.c \
	src/mfcc.c \
	src/ai_agent.c \
	src/iot_hal.c \
	src/iot_local_sim.c \
	src/iot_miot_stub.c

CXXSRCS = \
	src/scene_recog.cpp \
	src/wakeup.cpp \
	src/anomaly.cpp \
	src/scene_ui.cpp

include $(APPDIR)/Application.mk
