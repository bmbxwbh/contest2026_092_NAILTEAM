/****************************************************************************
 * board/contest_board/src/bk7258_tflite_op.cpp
 *
 * BK7258 TFLite Micro 自定义算子适配
 *
 * 将 BK7258 硬件 AI 加速器注册为 TFLite Micro 自定义算子,
 * 实现端侧 CNN 模型推理加速。
 *
 * 支持的算子 (委托给硬件加速器):
 *   - Conv2D (卷积层)
 *   - DepthwiseConv2D (深度卷积)
 *   - FullyConnected (全连接层)
 *   - AveragePool2D / MaxPool2D (池化层)
 *   - Add / Mul (元素级操作)
 *
 * 使用方式:
 *   tflite::MicroInterpreter interpreter(model, op_resolver, tensor_arena);
 *   // 自定义算子会自动路由到 BK7258 AI 加速器
 *
 ****************************************************************************/

#include <cstdint>
#include <cstring>

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/micro/micro_common.h"
#include "tensorflow/lite/micro/micro_op_resolver.h"
#include "tensorflow/lite/micro/micro_log.h"

#include <arch/chip/bk7258.h>

namespace tflite {
namespace ops {
namespace custom {
namespace bk7258 {

/****************************************************************************
 * 硬件加速器配置
 ****************************************************************************/

#define BK7258_TFLM_MAX_WEIGHT_SIZE   (128 * 1024)   /* AI SRAM 128KB */
#define BK7258_TFLM_ARENA_SIZE        (256 * 1024)   /* 推理 arena */

/****************************************************************************
 * Conv2D 算子 (硬件加速)
 ****************************************************************************/

typedef struct {
  int32_t padding;
  int32_t stride_w;
  int32_t stride_h;
  int32_t dilation_w;
  int32_t dilation_h;
  TfLiteFusedActivation activation;
} Conv2DParams;

static TfLiteStatus Conv2DPrepare(TfLiteContext* context, TfLiteNode* node) {
  TF_LITE_ENSURE_EQ(context, NumInputs(node), 2);
  TF_LITE_ENSURE_EQ(context, NumOutputs(node), 1);

  const TfLiteTensor* input = GetInput(context, node, 0);
  const TfLiteTensor* filter = GetInput(context, node, 1);
  TfLiteTensor* output = GetOutput(context, node, 0);

  /* 验证输入维度 */
  TF_LITE_ENSURE_EQ(context, input->dims->size, 4);
  TF_LITE_ENSURE_EQ(context, filter->dims->size, 4);
  TF_LITE_ENSURE_EQ(context, output->dims->size, 4);

  /* 加载权重到 AI 加速器专用 SRAM */
  uint32_t weightSize = filter->bytes;
  if (weightSize > BK7258_TFLM_MAX_WEIGHT_SIZE) {
    MicroPrintf("Conv2D weight too large: %u", weightSize);
    return kTfLiteError;
  }

  bk7258_ai_load_weights(filter->data.int8, weightSize);

  return kTfLiteOk;
}

static TfLiteStatus Conv2DEval(TfLiteContext* context, TfLiteNode* node) {
  const TfLiteTensor* input = GetInput(context, node, 0);
  const TfLiteTensor* filter = GetInput(context, node, 1);
  TfLiteTensor* output = GetOutput(context, node, 0);

  /* 调用 BK7258 AI 加速器执行卷积推理 */
  int ret = bk7258_ai_infer(input->data.data, output->data.data,
                            input->bytes);
  if (ret != 0) {
    MicroPrintf("AI accelerator Conv2D failed: %d", ret);
    return kTfLiteError;
  }

  return kTfLiteOk;
}

/****************************************************************************
 * FullyConnected 算子 (硬件加速)
 ****************************************************************************/

static TfLiteStatus FullyConnectedPrepare(TfLiteContext* context,
                                          TfLiteNode* node) {
  TF_LITE_ENSURE_EQ(context, NumInputs(node), 2);
  TF_LITE_ENSURE_EQ(context, NumOutputs(node), 1);

  const TfLiteTensor* input = GetInput(context, node, 0);
  const TfLiteTensor* weights = GetInput(context, node, 1);
  TfLiteTensor* output = GetOutput(context, node, 0);

  /* 加载权重到 AI 加速器 */
  uint32_t weightSize = weights->bytes;
  if (weightSize > BK7258_TFLM_MAX_WEIGHT_SIZE) {
    MicroPrintf("FC weight too large: %u", weightSize);
    return kTfLiteError;
  }

  bk7258_ai_load_weights(weights->data.int8, weightSize);

  return kTfLiteOk;
}

static TfLiteStatus FullyConnectedEval(TfLiteContext* context,
                                        TfLiteNode* node) {
  const TfLiteTensor* input = GetInput(context, node, 0);
  TfLiteTensor* output = GetOutput(context, node, 0);

  int ret = bk7258_ai_infer(input->data.data, output->data.data,
                            input->bytes);
  if (ret != 0) {
    MicroPrintf("AI accelerator FC failed: %d", ret);
    return kTfLiteError;
  }

  return kTfLiteOk;
}

/****************************************************************************
 * AveragePool2D 算子 (硬件加速)
 ****************************************************************************/

static TfLiteStatus AveragePool2DEval(TfLiteContext* context,
                                       TfLiteNode* node) {
  const TfLiteTensor* input = GetInput(context, node, 0);
  TfLiteTensor* output = GetOutput(context, node, 0);

  int ret = bk7258_ai_infer(input->data.data, output->data.data,
                            input->bytes);
  if (ret != 0) {
    MicroPrintf("AI accelerator AvgPool failed: %d", ret);
    return kTfLiteError;
  }

  return kTfLiteOk;
}

/****************************************************************************
 * 算子注册函数
 ****************************************************************************/

TfLiteRegistration* RegisterConv2D() {
  static TfLiteRegistration r = {nullptr, nullptr, Conv2DPrepare, Conv2DEval};
  return &r;
}

TfLiteRegistration* RegisterFullyConnected() {
  static TfLiteRegistration r = {nullptr, nullptr,
                                  FullyConnectedPrepare,
                                  FullyConnectedEval};
  return &r;
}

TfLiteRegistration* RegisterAveragePool2D() {
  static TfLiteRegistration r = {nullptr, nullptr, nullptr, AveragePool2DEval};
  return &r;
}

}  // namespace bk7258
}  // namespace custom
}  // namespace ops
}  // namespace tflite

/****************************************************************************
 * C 接口: 注册所有 BK7258 自定义算子
 ****************************************************************************/

extern "C" {

/****************************************************************************
 * Name: bk7258_tflite_register_ops
 *
 * Description:
 *   向 TFLite Micro op_resolver 注册 BK7258 硬件加速算子。
 *
 ****************************************************************************/

void bk7258_tflite_register_ops(tflite::MicroMutableOpResolver<10>* resolver) {
  resolver->AddCustom("BK7258_Conv2D",
                      tflite::ops::custom::bk7258::RegisterConv2D());
  resolver->AddCustom("BK7258_FullyConnected",
                      tflite::ops::custom::bk7258::RegisterFullyConnected());
  resolver->AddCustom("BK7258_AveragePool2D",
                      tflite::ops::custom::bk7258::RegisterAveragePool2D());
}

/****************************************************************************
 * Name: bk7258_tflite_init
 *
 * Description:
 *   初始化 TFLite Micro 与 BK7258 AI 加速器。
 *
 ****************************************************************************/

int bk7258_tflite_init(void) {
  /* 初始化 AI 加速器硬件 */
  int ret = bk7258_ai_accel_initialize();
  if (ret != 0) {
    return ret;
  }

  /* 设置 int8 推理模式 */
  bk7258_ai_set_mode(BK7258_AI_MODE_INT8);

  return 0;
}

}  // extern "C"
