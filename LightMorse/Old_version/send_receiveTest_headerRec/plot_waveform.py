# ─── plot_header_waveform.py ───
import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path

CSV_FILE = "header_log.csv"     # 適宜パス修正
IMG_FILE = "header_waveform.png"

# ① CSV を DataFrame に読み込む
df = pd.read_csv(CSV_FILE)

# ② タイムスタンプ (µs) → 経過秒へ変換
t0 = df["timestamp_us"].iloc[0]
df["time_s"] = (df["timestamp_us"] - t0) / 1e6

# ③ プロット
fig, ax = plt.subplots(figsize=(10, 4))
ax.plot(df["time_s"], df["adc"])
ax.set_xlabel("Time [s]")
ax.set_ylabel("ADC value")
ax.set_title("Photodiode Raw Waveform (-.-.- Header)")
ax.grid(True)
fig.tight_layout()

# ④ 画面に表示 & PNG 保存
plt.show()
fig.savefig(IMG_FILE, dpi=200)
print(f"→ 画像を {Path(IMG_FILE).resolve()} に保存しました")