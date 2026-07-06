/****************************************************************************
 * AI Scene-Aware Smart Speaker - Main Entry
 *
 * 基于 Gemini-S1 (R528, 双核 Cortex-A7) 的 AI 场景感知智能音箱应用。
 * 功能模块:
 *   1. 音频采集 — 从板载麦克风捕获音频 (NuttX audio API)
 *   2. 场景检测 — TFLite Micro 模型推理（占位骨架）
 *   3. 唤醒词检测 — "你好 openvela" 语音唤醒
 *   4. 云端 AI Agent — WiFi HTTP REST API 连接
 *   5. 米家智能家居 — MiHome API 设备控制占位
 *   6. HiFi4 DSP — 音频预处理加速
 *   7. LVGL UI — 2.8" SPI 屏幕场景显示
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <time.h>

/* NuttX audio 相关头文件 */
#ifdef CONFIG_AUDIO
#include <nuttx/audio/audio.h>
#endif

/* NuttX 网络相关头文件 */
#ifdef CONFIG_NET
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

/* LVGL 相关头文件 */
#ifdef CONFIG_GRAPHICS_LVGL
#include <lvgl/lvgl.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 音频设备路径 — R528 SUN8IW20 audio codec */
#define AUDIO_DEV_CAPTURE    "/dev/audio/pcm0c"
#define AUDIO_DEV_PLAYBACK   "/dev/audio/pcm0p"

/* 音频参数 */
#define AUDIO_SAMPLE_RATE    16000
#define AUDIO_CHANNELS       1
#define AUDIO_FRAME_SIZE     640   /* 20ms @16kHz 16bit mono */

/* 唤醒词 */
#define WAKE_WORD            "ni hao openvela"
#define WAKE_WORD_LEN        15

/* 云端 AI Agent 默认配置 */
#define CLOUD_AI_HOST        "192.168.1.100"
#define CLOUD_AI_PORT        8080
#define CLOUD_AI_PATH        "/api/v1/chat"

/* 米家 API 占位 */
#define MIHOME_API_HOST      "mihome-api.example.com"
#define MIHOME_API_PORT      443

/* 场景检测循环间隔 (ms) */
#define SCENE_DETECT_INTERVAL_MS  2000

/* 应用状态枚举 */
typedef enum
{
  APP_STATE_IDLE = 0,       /* 空闲待唤醒 */
  APP_STATE_LISTENING,      /* 唤醒后监听指令 */
  APP_STATE_PROCESSING,     /* 云端处理中 */
  APP_STATE_RESPONDING,     /* 播报回复 */
  APP_STATE_SCENE_DETECT    /* 场景检测运行中 */
} app_state_t;

/* 场景类型枚举 */
typedef enum
{
  SCENE_UNKNOWN = 0,
  SCENE_HOME,               /* 居家 */
  SCENE_SLEEP,              /* 睡眠 */
  SCENE_COOKING,            /* 烹饪 */
  SCENE_WORKING,            /* 工作 */
  SCENE_ENTERTAINMENT       /* 娱乐 */
} scene_type_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* 应用运行状态 */
static volatile app_state_t g_app_state   = APP_STATE_IDLE;
static volatile scene_type_t g_cur_scene  = SCENE_UNKNOWN;
static volatile bool         g_running    = true;

/* 音频文件描述符 */
#ifdef CONFIG_AUDIO
static int g_audio_cap_fd  = -1;
static int g_audio_play_fd = -1;
#endif

/* 网络套接字 */
#ifdef CONFIG_NET
static int g_cloud_sock = -1;
#endif

/* LVGL 显示对象 */
#ifdef CONFIG_GRAPHICS_LVGL
static lv_obj_t *g_scene_label = NULL;
static lv_obj_t *g_state_label = NULL;
#endif

/* 音频缓冲区 */
static uint8_t g_audio_buf[AUDIO_FRAME_SIZE];

/****************************************************************************
 * Name: audio_capture_init
 *
 * Description:
 *   初始化音频采集设备。打开 R528 的 /dev/audio/pcm0c，配置采样率、
 *   通道数和位深，准备接收板载麦克风数据。
 *
 ****************************************************************************/

#ifdef CONFIG_AUDIO
static int audio_capture_init(void)
{
  struct audio_caps_desc_s cap_desc;
  int ret;

  /* 打开音频采集设备 */
  g_audio_cap_fd = open(AUDIO_DEV_CAPTURE, O_RDWR | O_CLOEXEC);
  if (g_audio_cap_fd < 0)
    {
      fprintf(stderr, "[SPEAKER] Failed to open capture device %s: %d\n",
              AUDIO_DEV_CAPTURE, errno);
      return -errno;
    }

  /* 配置采集参数: 16kHz, 16bit, 单声道 */
  memset(&cap_desc, 0, sizeof(cap_desc));
  cap_desc.caps.ac_len            = sizeof(struct audio_caps_s);
  cap_desc.caps.ac_type           = AUDIO_TYPE_INPUT;
  cap_desc.caps.ac_channels       = AUDIO_CHANNELS;
  cap_desc.caps.ac_chmap          = 0;
  cap_desc.caps.ac_samplerate.lower = AUDIO_SAMPLE_RATE;
  cap_desc.caps.ac_samplerate.upper = AUDIO_SAMPLE_RATE;
  cap_desc.caps.ac_controls.b[0]  = 16; /* 16-bit */

  ret = ioctl(g_audio_cap_fd, AUDIOIOC_CONFIGURE,
              (unsigned long)(uintptr_t)&cap_desc);
  if (ret < 0)
    {
      fprintf(stderr, "[SPEAKER] Failed to configure capture: %d\n", errno);
      close(g_audio_cap_fd);
      g_audio_cap_fd = -1;
      return -errno;
    }

  printf("[SPEAKER] Audio capture initialized: %s @%dHz %dch 16bit\n",
         AUDIO_DEV_CAPTURE, AUDIO_SAMPLE_RATE, AUDIO_CHANNELS);
  return 0;
}
#else
static int audio_capture_init(void)
{
  printf("[SPEAKER] Audio subsystem not compiled in (CONFIG_AUDIO disabled)\n");
  return 0;
}
#endif

/****************************************************************************
 * Name: audio_capture_read
 *
 * Description:
 *   从麦克风读取一帧音频数据。用于唤醒词检测和场景推理输入。
 *
 ****************************************************************************/

#ifdef CONFIG_AUDIO
static int audio_capture_read(uint8_t *buf, int buf_size)
{
  ssize_t nread;

  if (g_audio_cap_fd < 0)
    {
      return -EINVAL;
    }

  nread = read(g_audio_cap_fd, buf, buf_size);
  if (nread < 0)
    {
      fprintf(stderr, "[SPEAKER] Audio read error: %d\n", errno);
      return -errno;
    }

  return (int)nread;
}
#else
static int audio_capture_read(uint8_t *buf, int buf_size)
{
  /* 无音频子系统时返回模拟数据 */
  memset(buf, 0, buf_size);
  return buf_size;
}
#endif

/****************************************************************************
 * Name: audio_playback_init
 *
 * Description:
 *   初始化音频播放设备。打开 /dev/audio/pcm0p，用于 TTS 语音播报。
 *
 ****************************************************************************/

#ifdef CONFIG_AUDIO
static int audio_playback_init(void)
{
  struct audio_caps_desc_s cap_desc;
  int ret;

  g_audio_play_fd = open(AUDIO_DEV_PLAYBACK, O_RDWR | O_CLOEXEC);
  if (g_audio_play_fd < 0)
    {
      fprintf(stderr, "[SPEAKER] Failed to open playback device %s: %d\n",
              AUDIO_DEV_PLAYBACK, errno);
      return -errno;
    }

  /* 配置播放参数 */
  memset(&cap_desc, 0, sizeof(cap_desc));
  cap_desc.caps.ac_len            = sizeof(struct audio_caps_s);
  cap_desc.caps.ac_type           = AUDIO_TYPE_OUTPUT;
  cap_desc.caps.ac_channels       = AUDIO_CHANNELS;
  cap_desc.caps.ac_chmap          = 0;
  cap_desc.caps.ac_samplerate.lower = AUDIO_SAMPLE_RATE;
  cap_desc.caps.ac_samplerate.upper = AUDIO_SAMPLE_RATE;
  cap_desc.caps.ac_controls.b[0]  = 16;

  ret = ioctl(g_audio_play_fd, AUDIOIOC_CONFIGURE,
              (unsigned long)(uintptr_t)&cap_desc);
  if (ret < 0)
    {
      fprintf(stderr, "[SPEAKER] Failed to configure playback: %d\n", errno);
      close(g_audio_play_fd);
      g_audio_play_fd = -1;
      return -errno;
    }

  printf("[SPEAKER] Audio playback initialized: %s\n", AUDIO_DEV_PLAYBACK);
  return 0;
}
#else
static int audio_playback_init(void)
{
  printf("[SPEAKER] Audio playback not compiled in\n");
  return 0;
}
#endif

/****************************************************************************
 * Name: dsp_preprocess
 *
 * Description:
 *   使用 R528 的 HiFi4 DSP 进行音频预处理（如果可用）。
 *   包括降噪 (NR)、自动增益控制 (AGC)、回声消除 (AEC)。
 *   当前为占位实现，实际应调用 DSP 固件 API。
 *
 ****************************************************************************/

#ifdef CONFIG_R528_AUDIO
static int dsp_preprocess(uint8_t *audio_buf, int buf_size)
{
  /* TODO: 调用 R528 HiFi4 DSP 固件接口进行音频预处理
   *   - NR:  降噪，去除环境底噪
   *   - AGC: 自动增益控制，归一化音量
   *   - AEC: 回声消除，去除扬声器回声
   *
   * R528 的 DSP 通过 SPI/I2S 与主核通信，
   * 实际调用方式为:
   *   ioctl(dsp_fd, DSP_IOC_PROCESS, &dsp_params);
   *
   * 当前仅做简单的过零率计算作为占位。
   */

  int zero_crossings = 0;
  int16_t *samples = (int16_t *)audio_buf;
  int num_samples  = buf_size / sizeof(int16_t);

  for (int i = 1; i < num_samples; i++)
    {
      if ((samples[i] ^ samples[i - 1]) & 0x8000)
        {
          zero_crossings++;
        }
    }

  /* 过零率可用于简单的 VAD（语音活动检测） */
  return zero_crossings;
}
#else
static int dsp_preprocess(uint8_t *audio_buf, int buf_size)
{
  /* 无 DSP 支持时跳过预处理 */
  return 0;
}
#endif

/****************************************************************************
 * Name: wake_word_detect
 *
 * Description:
 *   检测唤醒词 "你好 openvela"。
 *   当前为简单能量阈值检测占位，实际应使用 TFLite Micro
 *   语音关键词检测模型。
 *
 ****************************************************************************/

static bool wake_word_detect(uint8_t *audio_buf, int buf_size)
{
#ifdef CONFIG_TFLITE_MICRO
  /* TODO: 调用 TFLite Micro 语音关键词检测模型
   *
   * 实际流程:
   *   1. 提取 MFCC 特征 (从 16kHz PCM)
   *   2. 输入 TFLite Micro 模型推理
   *   3. 判断是否匹配 "ni hao openvela" 关键词
   *
   *   tflite::MicroInterpreter interpreter(model, resolver, tensor_arena,
   *                                        kTensorArenaSize);
   *   interpreter.Invoke();
   *   float *output = interpreter.output(0)->data.f;
   *   if (output[keyword_id] > threshold) return true;
   */

  /* 占位: 基于过零率的简单语音活动检测 */
  int zcr = dsp_preprocess(audio_buf, buf_size);
  int num_samples = buf_size / sizeof(int16_t);
  float zcr_rate = (float)zcr / num_samples;

  /* 简单启发式: 过零率在特定范围可能为语音 */
  if (zcr_rate > 0.05f && zcr_rate < 0.5f)
    {
      printf("[SPEAKER] Wake word candidate detected (zcr=%.3f)\n", zcr_rate);
      return true;
    }

  return false;
#else
  /* 无 TFLite Micro 时，使用简单能量检测作为占位 */
  int16_t *samples = (int16_t *)audio_buf;
  int num_samples  = buf_size / sizeof(int16_t);
  int64_t energy   = 0;

  for (int i = 0; i < num_samples; i++)
    {
      energy += (int64_t)samples[i] * samples[i];
    }

  float rms = sqrtf((float)energy / num_samples);

  /* 能量超过阈值视为可能的唤醒 */
  if (rms > 500.0f)
    {
      printf("[SPEAKER] Wake word energy detected (rms=%.1f)\n", rms);
      return true;
    }

  return false;
#endif
}

/****************************************************************************
 * Name: scene_detect_inference
 *
 * Description:
 *   运行场景检测推理。使用 TFLite Micro 模型对音频特征进行分类，
 *   输出当前场景类型（居家/睡眠/烹饪/工作/娱乐）。
 *   当前为占位实现。
 *
 ****************************************************************************/

static scene_type_t scene_detect_inference(uint8_t *audio_buf, int buf_size)
{
#ifdef CONFIG_TFLITE_MICRO
  /* TODO: 实际的 TFLite Micro 场景检测推理流程
   *
   *   1. 从音频缓冲区提取特征 (MFCC / spectrogram)
   *   2. 构造 TFLite Micro 输入张量
   *   3. 执行推理
   *   4. 解析输出概率分布，选择最大概率场景
   *
   *   场景标签:
   *     0 - SCENE_UNKNOWN
   *     1 - SCENE_HOME        (居家: 安静/谈话声)
   *     2 - SCENE_SLEEP        (睡眠: 极低环境声)
   *     3 - SCENE_COOKING      (烹饪: 厨房噪声/水流声)
   *     4 - SCENE_WORKING      (工作: 键盘声/低语)
   *     5 - SCENE_ENTERTAINMENT(娱乐: 音乐/电视声)
   */
#endif

  /* 占位: 基于过零率的简单场景推断 */
  int16_t *samples = (int16_t *)audio_buf;
  int num_samples  = buf_size / sizeof(int16_t);
  int64_t energy   = 0;
  int    zcr       = 0;

  for (int i = 0; i < num_samples; i++)
    {
      energy += (int64_t)samples[i] * samples[i];
      if (i > 0 && ((samples[i] ^ samples[i - 1]) & 0x8000))
        {
          zcr++;
        }
    }

  float rms     = sqrtf((float)energy / num_samples);
  float zcr_rate = (float)zcr / num_samples;

  /* 简单规则引擎占位 */
  if (rms < 100.0f)
    {
      return SCENE_SLEEP;
    }
  else if (rms < 300.0f && zcr_rate < 0.1f)
    {
      return SCENE_HOME;
    }
  else if (zcr_rate > 0.3f)
    {
      return SCENE_ENTERTAINMENT;
    }
  else if (rms > 1000.0f)
    {
      return SCENE_COOKING;
    }
  else
    {
      return SCENE_WORKING;
    }
}

/****************************************************************************
 * Name: cloud_ai_connect
 *
 * Description:
 *   通过 WiFi 连接云端 AI Agent 服务。
 *   使用 HTTP REST API 发送语音文本/意图，获取 AI 回复。
 *
 ****************************************************************************/

#ifdef CONFIG_NET
static int cloud_ai_connect(const char *host, int port)
{
  struct sockaddr_in addr;
  int ret;

  g_cloud_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (g_cloud_sock < 0)
    {
      fprintf(stderr, "[SPEAKER] Failed to create cloud socket: %d\n", errno);
      return -errno;
    }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port   = htons(port);

  /* 尝试将主机名解析为 IP 地址 */
  struct hostent *he = gethostbyname(host);
  if (he && he->h_addr_list[0])
    {
      memcpy(&addr.sin_addr, he->h_addr_list[0],
             he->h_length);
    }
  else
    {
      /* 解析失败时使用 inet_addr 作为回退 */
      addr.sin_addr.s_addr = inet_addr(host);
    }

  ret = connect(g_cloud_sock, (struct sockaddr *)&addr, sizeof(addr));
  if (ret < 0)
    {
      fprintf(stderr, "[SPEAKER] Failed to connect to cloud AI %s:%d: %d\n",
              host, port, errno);
      close(g_cloud_sock);
      g_cloud_sock = -1;
      return -errno;
    }

  printf("[SPEAKER] Connected to cloud AI Agent at %s:%d\n", host, port);
  return 0;
}
#else
static int cloud_ai_connect(const char *host, int port)
{
  printf("[SPEAKER] Network not compiled in (CONFIG_NET disabled)\n");
  return 0;
}
#endif

/****************************************************************************
 * Name: cloud_ai_send_request
 *
 * Description:
 *   向云端 AI Agent 发送 HTTP REST API 请求。
 *   发送 JSON 格式的用户意图/文本，接收 AI 回复。
 *
 ****************************************************************************/

#ifdef CONFIG_NET
static int cloud_ai_send_request(const char *json_payload,
                                 char *response, int resp_size)
{
  char http_req[1024];
  int  req_len;
  int  ret;

  if (g_cloud_sock < 0)
    {
      fprintf(stderr, "[SPEAKER] Cloud AI not connected\n");
      return -ENOTCONN;
    }

  /* 构造 HTTP POST 请求 */
  req_len = snprintf(http_req, sizeof(http_req),
    "POST %s HTTP/1.1\r\n"
    "Host: %s:%d\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: %d\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "%s",
    CLOUD_AI_PATH, CLOUD_AI_HOST, CLOUD_AI_PORT,
    (int)strlen(json_payload), json_payload);

  ret = send(g_cloud_sock, http_req, req_len, 0);
  if (ret < 0)
    {
      fprintf(stderr, "[SPEAKER] Failed to send cloud request: %d\n", errno);
      return -errno;
    }

  /* 接收 HTTP 响应（简化版，仅读取原始数据） */
  ret = recv(g_cloud_sock, response, resp_size - 1, 0);
  if (ret < 0)
    {
      fprintf(stderr, "[SPEAKER] Failed to receive cloud response: %d\n",
              errno);
      return -errno;
    }

  response[ret] = '\0';
  printf("[SPEAKER] Cloud AI response received (%d bytes)\n", ret);
  return ret;
}
#else
static int cloud_ai_send_request(const char *json_payload,
                                 char *response, int resp_size)
{
  /* 无网络时的模拟回复 */
  const char *mock_resp = "{\"reply\":\"收到，正在处理中\"}";
  int len = strlen(mock_resp);
  if (len >= resp_size) len = resp_size - 1;
  memcpy(response, mock_resp, len);
  response[len] = '\0';
  printf("[SPEAKER] Cloud AI mock response (no network)\n");
  return len;
}
#endif

/****************************************************************************
 * Name: mihome_control_device
 *
 * Description:
 *   米家智能家居设备控制占位。
 *   实际应通过米家开放平台 API 控制设备。
 *
 ****************************************************************************/

#ifdef CONFIG_NET
static int mihome_control_device(const char *device_id, const char *action)
{
  char json_payload[512];
  char response[1024];
  int  ret;

  printf("[SPEAKER] MiHome control: device=%s action=%s\n", device_id, action);

  /* 构造米家 API 请求 JSON */
  snprintf(json_payload, sizeof(json_payload),
    "{\"device_id\":\"%s\",\"action\":\"%s\"}", device_id, action);

  /* TODO: 实际应连接米家开放平台 API
   * 1. 通过 OAuth2 获取 access_token
   * 2. 调用 /home/device/control 接口
   * 3. 解析响应判断控制结果
   *
   * 当前使用通用 cloud_ai_send_request 作为占位。
   */
  ret = cloud_ai_send_request(json_payload, response, sizeof(response));
  if (ret < 0)
    {
      fprintf(stderr, "[SPEAKER] MiHome control failed: %d\n", ret);
      return ret;
    }

  printf("[SPEAKER] MiHome control response: %s\n", response);
  return 0;
}
#else
static int mihome_control_device(const char *device_id, const char *action)
{
  printf("[SPEAKER] MiHome control (no network): device=%s action=%s\n",
         device_id, action);
  return 0;
}
#endif

/****************************************************************************
 * Name: scene_execute_automation
 *
 * Description:
 *   根据检测到的场景自动执行智能家居联动。
 *   例如: 睡眠场景 -> 关灯、关空调; 烹饪场景 -> 开厨房灯。
 *
 ****************************************************************************/

static void scene_execute_automation(scene_type_t scene)
{
  switch (scene)
    {
      case SCENE_SLEEP:
        printf("[SPEAKER] Scene: Sleep -> dim lights, set AC to 26C\n");
        mihome_control_device("light_bedroom", "off");
        mihome_control_device("ac_bedroom", "set_temp_26");
        break;

      case SCENE_COOKING:
        printf("[SPEAKER] Scene: Cooking -> turn on kitchen light\n");
        mihome_control_device("light_kitchen", "on");
        mihome_control_device("fan_kitchen", "on");
        break;

      case SCENE_HOME:
        printf("[SPEAKER] Scene: Home -> normal lighting\n");
        mihome_control_device("light_living", "on_brightness_70");
        break;

      case SCENE_WORKING:
        printf("[SPEAKER] Scene: Working -> focus mode\n");
        mihome_control_device("light_study", "on_brightness_100");
        mihome_control_device("ac_study", "set_temp_24");
        break;

      case SCENE_ENTERTAINMENT:
        printf("[SPEAKER] Scene: Entertainment -> ambient lighting\n");
        mihome_control_device("light_living", "on_brightness_40");
        mihome_control_device("tv_living", "on");
        break;

      default:
        printf("[SPEAKER] Scene: Unknown -> no automation\n");
        break;
    }
}

/****************************************************************************
 * Name: lvgl_ui_init
 *
 * Description:
 *   初始化 LVGL UI 界面。
 *   在 2.8" SPI LCD 上显示当前场景状态和交互信息。
 *
 ****************************************************************************/

#ifdef CONFIG_GRAPHICS_LVGL
static void lvgl_ui_init(void)
{
  lv_obj_t *scr = lv_scr_act();

  /* 创建场景标题标签 */
  g_scene_label = lv_label_create(scr);
  lv_label_set_text(g_scene_label, "Scene: Initializing...");
  lv_obj_set_style_text_font(g_scene_label, &lv_font_montserrat_16, 0);
  lv_obj_align(g_scene_label, LV_ALIGN_TOP_LEFT, 10, 10);

  /* 创建应用状态标签 */
  g_state_label = lv_label_create(scr);
  lv_label_set_text(g_state_label, "State: IDLE");
  lv_obj_set_style_text_font(g_state_label, &lv_font_montserrat_16, 0);
  lv_obj_align(g_state_label, LV_ALIGN_TOP_LEFT, 10, 40);

  printf("[SPEAKER] LVGL UI initialized on 2.8\" SPI LCD\n");
}
#else
static void lvgl_ui_init(void)
{
  printf("[SPEAKER] LVGL not compiled in (CONFIG_GRAPHICS_LVGL disabled)\n");
}
#endif

/****************************************************************************
 * Name: lvgl_ui_update
 *
 * Description:
 *   更新 LVGL UI 显示内容，反映当前场景和状态。
 *
 ****************************************************************************/

#ifdef CONFIG_GRAPHICS_LVGL
static void lvgl_ui_update(scene_type_t scene, app_state_t state)
{
  const char *scene_names[] = {
    "Unknown", "Home", "Sleep", "Cooking", "Working", "Entertainment"
  };

  const char *state_names[] = {
    "IDLE", "LISTENING", "PROCESSING", "RESPONDING", "SCENE_DETECT"
  };

  if (g_scene_label)
    {
      lv_label_set_text_fmt(g_scene_label, "Scene: %s",
                            scene_names[scene]);
    }

  if (g_state_label)
    {
      lv_label_set_text_fmt(g_state_label, "State: %s",
                            state_names[state]);
    }

  /* 触发 LVGL 刷新 */
  lv_task_handler();
}
#else
static void lvgl_ui_update(scene_type_t scene, app_state_t state)
{
  /* 无 LVGL 时仅打印到串口 */
  const char *scene_names[] = {
    "Unknown", "Home", "Sleep", "Cooking", "Working", "Entertainment"
  };
  const char *state_names[] = {
    "IDLE", "LISTENING", "PROCESSING", "RESPONDING", "SCENE_DETECT"
  };
  printf("[SPEAKER] UI: Scene=%s State=%s\n",
         scene_names[scene], state_names[state]);
}
#endif

/****************************************************************************
 * Name: cloud_ai_disconnect
 *
 * Description:
 *   断开云端 AI Agent 连接。
 *
 ****************************************************************************/

#ifdef CONFIG_NET
static void cloud_ai_disconnect(void)
{
  if (g_cloud_sock >= 0)
    {
      close(g_cloud_sock);
      g_cloud_sock = -1;
      printf("[SPEAKER] Disconnected from cloud AI Agent\n");
    }
}
#else
static void cloud_ai_disconnect(void)
{
}
#endif

/****************************************************************************
 * Name: audio_cleanup
 *
 * Description:
 *   关闭音频设备，释放资源。
 *
 ****************************************************************************/

#ifdef CONFIG_AUDIO
static void audio_cleanup(void)
{
  if (g_audio_cap_fd >= 0)
    {
      close(g_audio_cap_fd);
      g_audio_cap_fd = -1;
    }

  if (g_audio_play_fd >= 0)
    {
      close(g_audio_play_fd);
      g_audio_play_fd = -1;
    }

  printf("[SPEAKER] Audio devices closed\n");
}
#else
static void audio_cleanup(void)
{
}
#endif

/****************************************************************************
 * Name: scene_detect_thread
 *
 * Description:
 *   场景检测线程。周期性采集音频并运行推理，
 *   检测环境场景变化并触发智能家居联动。
 *
 ****************************************************************************/

static void *scene_detect_thread(void *arg)
{
  printf("[SPEAKER] Scene detection thread started\n");

  while (g_running)
    {
      /* 采集一帧音频 */
      int nread = audio_capture_read(g_audio_buf, AUDIO_FRAME_SIZE);
      if (nread > 0)
        {
          /* DSP 预处理 */
          dsp_preprocess(g_audio_buf, nread);

          /* 场景推理 */
          scene_type_t new_scene = scene_detect_inference(g_audio_buf, nread);

          /* 场景变化时触发联动 */
          if (new_scene != g_cur_scene)
            {
              printf("[SPEAKER] Scene changed: %d -> %d\n",
                     g_cur_scene, new_scene);
              g_cur_scene = new_scene;

              /* 执行场景自动化 */
              scene_execute_automation(new_scene);

              /* 更新 UI */
              lvgl_ui_update(new_scene, g_app_state);
            }
        }

      /* 控制检测频率 */
      usleep(SCENE_DETECT_INTERVAL_MS * 1000);
    }

  printf("[SPEAKER] Scene detection thread exited\n");
  return NULL;
}

/****************************************************************************
 * Name: voice_interaction_thread
 *
 * Description:
 *   语音交互线程。持续采集音频进行唤醒词检测，
 *   唤醒后连接云端 AI Agent 处理语音指令。
 *
 ****************************************************************************/

static void *voice_interaction_thread(void *arg)
{
  printf("[SPEAKER] Voice interaction thread started\n");

  while (g_running)
    {
      /* 采集一帧音频 */
      int nread = audio_capture_read(g_audio_buf, AUDIO_FRAME_SIZE);
      if (nread <= 0)
        {
          usleep(10000); /* 10ms */
          continue;
        }

      /* DSP 预处理 */
      dsp_preprocess(g_audio_buf, nread);

      if (g_app_state == APP_STATE_IDLE)
        {
          /* 唤醒词检测 */
          if (wake_word_detect(g_audio_buf, nread))
            {
              printf("[SPEAKER] Wake word detected: \"%s\"!\n", WAKE_WORD);
              g_app_state = APP_STATE_LISTENING;
              lvgl_ui_update(g_cur_scene, g_app_state);

              /* 模拟语音交互流程 */
              g_app_state = APP_STATE_PROCESSING;
              lvgl_ui_update(g_cur_scene, g_app_state);

              /* 连接云端 AI Agent */
              int ret = cloud_ai_connect(CLOUD_AI_HOST, CLOUD_AI_PORT);
              if (ret == 0)
                {
                  char response[1024];
                  const char *req_json =
                    "{\"text\":\"你好 openvela\",\"scene\":1}";

                  cloud_ai_send_request(req_json, response, sizeof(response));
                  cloud_ai_disconnect();
                }

              /* 回到空闲状态 */
              g_app_state = APP_STATE_IDLE;
              lvgl_ui_update(g_cur_scene, g_app_state);
            }
        }

      usleep(10000); /* 10ms 间隔 */
    }

  printf("[SPEAKER] Voice interaction thread exited\n");
  return NULL;
}

/****************************************************************************
 * Name: signal_handler
 *
 * Description:
 *   信号处理函数，用于优雅退出。
 *
 ****************************************************************************/

static void signal_handler(int sig)
{
  printf("[SPEAKER] Received signal %d, shutting down...\n", sig);
  g_running = false;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main / scene_aware_speaker_main
 *
 * Description:
 *   AI 场景感知智能音箱应用主入口。
 *   初始化各子系统，启动场景检测和语音交互线程。
 *
 ****************************************************************************/

int main(int argc, char *argv[])
{
  pthread_t scene_tid;
  pthread_t voice_tid;
  pthread_attr_t attr;
  struct sched_param param;
  int ret;

  printf("[SPEAKER] ============================================\n");
  printf("[SPEAKER] AI Scene-Aware Smart Speaker\n");
  printf("[SPEAKER] Board: Gemini-S1 (R528, Dual-core Cortex-A7)\n");
  printf("[SPEAKER] ============================================\n");

  /* 注册信号处理 */
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  /* 1. 初始化音频采集 */
  ret = audio_capture_init();
  if (ret < 0)
    {
      fprintf(stderr, "[SPEAKER] Audio capture init failed: %d\n", ret);
      /* 音频初始化失败不致命，继续运行（降级模式） */
    }

  /* 2. 初始化音频播放 */
  ret = audio_playback_init();
  if (ret < 0)
    {
      fprintf(stderr, "[SPEAKER] Audio playback init failed: %d\n", ret);
    }

  /* 3. 初始化 LVGL UI */
  lvgl_ui_init();

  /* 4. 连接云端 AI Agent (首次尝试，失败不阻塞) */
#ifdef CONFIG_NET
  ret = cloud_ai_connect(CLOUD_AI_HOST, CLOUD_AI_PORT);
  if (ret < 0)
    {
      printf("[SPEAKER] Cloud AI not available, will retry later\n");
    }
  else
    {
      cloud_ai_disconnect(); /* 仅测试连通性，后续按需连接 */
    }
#endif

  /* 5. 启动场景检测线程 */
  pthread_attr_init(&attr);
  param.sched_priority = 80;
  pthread_attr_setschedparam(&attr, &param);
  pthread_attr_setstacksize(&attr, 8192);

  ret = pthread_create(&scene_tid, &attr, scene_detect_thread, NULL);
  if (ret != 0)
    {
      fprintf(stderr, "[SPEAKER] Failed to create scene detect thread: %d\n",
              ret);
    }
  else
    {
      pthread_setname_np(scene_tid, "scene_detect");
      printf("[SPEAKER] Scene detection thread created\n");
    }

  /* 6. 启动语音交互线程 */
  param.sched_priority = 90;
  pthread_attr_setschedparam(&attr, &param);
  pthread_attr_setstacksize(&attr, 8192);

  ret = pthread_create(&voice_tid, &attr, voice_interaction_thread, NULL);
  if (ret != 0)
    {
      fprintf(stderr, "[SPEAKER] Failed to create voice thread: %d\n", ret);
    }
  else
    {
      pthread_setname_np(voice_tid, "voice_interact");
      printf("[SPEAKER] Voice interaction thread created\n");
    }

  pthread_attr_destroy(&attr);

  /* 7. 主循环: 维持 LVGL 刷新和状态监控 */
  printf("[SPEAKER] Main loop started\n");
  lvgl_ui_update(g_cur_scene, g_app_state);

  while (g_running)
    {
#ifdef CONFIG_GRAPHICS_LVGL
      /* LVGL 任务处理 (刷新显示、处理输入) */
      lv_task_handler();
#endif

      /* 每 50ms 检查一次 */
      usleep(50000);
    }

  /* 8. 清理退出 */
  printf("[SPEAKER] Shutting down...\n");

  /* 等待线程退出 */
  g_running = false;
  pthread_join(scene_tid, NULL);
  pthread_join(voice_tid, NULL);

  /* 关闭音频设备 */
  audio_cleanup();

  /* 断开云端连接 */
  cloud_ai_disconnect();

  printf("[SPEAKER] AI Scene-Aware Smart Speaker stopped.\n");
  return 0;
}
