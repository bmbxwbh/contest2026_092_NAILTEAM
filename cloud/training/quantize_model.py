import tensorflow as tf

def representative_dataset():
    import numpy as np
    for _ in range(100):
        yield [np.random.rand(1, 13, 98, 1).astype(np.float32)]

def quantize(h5_path="data/scene_cnn.h5", out="data/scene_cnn.tflite"):
    m = tf.keras.models.load_model(h5_path)
    conv = tf.lite.TFLiteConverter.from_keras_model(m)
    conv.optimizations = [tf.lite.Optimize.DEFAULT]
    conv.representative_dataset = representative_dataset
    conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    conv.inference_input_type = tf.int8
    conv.inference_output_type = tf.int8
    tflite = conv.convert()
    with open(out, "wb") as f:
        f.write(tflite)
    print(f"quantized int8 model: {out} ({len(tflite)} bytes)")

if __name__ == "__main__":
    quantize()
