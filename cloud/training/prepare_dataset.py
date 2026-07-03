import librosa, numpy as np, os, json

def extract_mfcc(path, sr=16000, n_mfcc=13):
    y, _ = librosa.load(path, sr=sr, mono=True)
    mfcc = librosa.feature.mfcc(y=y, sr=sr, n_mfcc=n_mfcc)
    mfcc = (mfcc - mfcc.mean()) / (mfcc.std() + 1e-6)
    return mfcc

def build_dataset(data_dir, out_path):
    X, y = [], []
    labels = {}
    for idx, cls in enumerate(sorted(os.listdir(data_dir))):
        labels[cls] = idx
        cdir = os.path.join(data_dir, cls)
        for f in os.listdir(cdir):
            if f.endswith(".wav"):
                X.append(extract_mfcc(os.path.join(cdir, f)))
                y.append(idx)
    np.savez(out_path, X=np.array(X, dtype=object), y=np.array(y), labels=labels)
    print(f"dataset saved: {len(X)} samples, labels={labels}")

if __name__ == "__main__":
    build_dataset("data/raw", "data/dataset.npz")
