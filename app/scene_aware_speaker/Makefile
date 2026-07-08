############################################################################
# AI Scene-Aware Smart Speaker - Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = scene_aware_speaker
PRIORITY  = $(CONFIG_SCENE_AWARE_SPEAKER_PRIORITY)
STACKSIZE = $(CONFIG_SCENE_AWARE_SPEAKER_STACKSIZE)
MODULE    = $(CONFIG_EXAMPLES_SCENE_AWARE_SPEAKER)

# Main source file

MAINSRC = scene_aware_speaker_main.c

# Module source files

CSRCS += audio_preprocess.c
CSRCS += feature_extract.c
CSRCS += scene_detect.c
CSRCS += wake_word.c
CSRCS += sensor_fusion.c
CSRCS += ai_agent.c
CSRCS += mihome_sim.c
CSRCS += lvgl_ui.c

include $(APPDIR)/Application.mk
