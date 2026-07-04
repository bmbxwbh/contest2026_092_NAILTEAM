if(CONFIG_SMART_SPEAKER_APP)
  nuttx_add_application(
    NAME smart_speaker
    SRCS
      src/speaker_main.c
      src/device_state.c
      src/display_manager.c
      src/audio_pipeline.c
      src/mfcc.c
      src/scene_recog.cpp
      src/wakeup.cpp
      src/anomaly.cpp
      src/ai_agent.c
      src/iot_hal.c
      src/iot_local_sim.c
      src/iot_miot_stub.c
      src/scene_ui.cpp
    INCLUDE_DIRECTORIES include
    STACKSIZE 4096)
endif()
