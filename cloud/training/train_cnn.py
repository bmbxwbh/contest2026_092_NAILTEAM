import numpy as np
from tensorflow.keras import layers, models, utils

def load_data(path="data/dataset.npz"):
    d = np.load(path, allow_pickle=True)
    T = 98
    X = []
    for m in d["X"]:
        if m.shape[1] >= T: m = m[:, :T]
        else: m = np.pad(m, ((0,0),(0,T-m.shape[1])))
        X.append(m)
    X = np.array(X)[..., None]
    y = utils.to_categorical(d["y"], num_classes=len(d["labels"].item()))
    return X, y, d["labels"].item()

def build_model(num_classes):
    m = models.Sequential([
        layers.Input((13, 98, 1)),
        layers.Conv2D(16, 3, activation="relu", padding="same"),
        layers.MaxPooling2D(2),
        layers.Conv2D(32, 3, activation="relu", padding="same"),
        layers.GlobalAveragePooling2D(),
        layers.Dense(num_classes, activation="softmax"),
    ])
    m.compile(optimizer="adam", loss="categorical_crossentropy", metrics=["accuracy"])
    return m

if __name__ == "__main__":
    X, y, labels = load_data()
    m = build_model(len(labels))
    m.fit(X, y, epochs=30, batch_size=32, validation_split=0.2)
    m.save("data/scene_cnn.h5")
    print("trained, labels:", labels)
