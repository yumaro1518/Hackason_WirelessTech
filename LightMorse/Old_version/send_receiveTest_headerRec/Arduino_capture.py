#!/usr/bin/env python3
"""
arduino_capture_stable.py
-------------------------
・Arduino 側スケッチ  hdr_scope_csv_10tries.ino から
   # timestamp(us),adc
   ...
   ## FINISHED
  を受信
・画面にリアルタイムプロット
・同時に data_YYYYMMDD_HHMMSS.csv へ追記
・## FINISHED を受け取るか Ctrl-C で安全終了

使い方:
  python arduino_capture_stable.py -p /dev/tty.usbmodemF412FA653DF02
     (ポート名は ls /dev/tty.usbmodem* などで確認)
  省略時は -p auto で“最新の usbmodem”を自動検出
"""

import argparse, csv, glob, os, sys, time
from collections import deque
from datetime import datetime

import serial
import matplotlib.pyplot as plt

# -------------------------------------------------  ポート検出
def latest_usbmodem() -> str:
    ports = glob.glob("/dev/tty.usbmodem*")
    if not ports:
        sys.exit("[ERR] usbmodem デバイスが見つかりません")
    return max(ports, key=os.path.getmtime)   # 更新時刻が新しい＝最新

# -------------------------------------------------  メイン
def main(port: str, baud: int):
    # ==== シリアルオープン ====
    if port == "auto":
        port = latest_usbmodem()
        print(f"[INFO] auto→ {port}")

    try:
        ser = serial.Serial(
            port, baud, timeout=1,
            exclusive=False,   # macOS の “Resource busy” 回避
            dsrdtr=False,      # DTR を弄らない→自動リセット抑止
            rtscts=False
        )
    except serial.SerialException as e:
        sys.exit(f"[ERROR] {e}")

    # ==== CSV 準備 ====
    csv_name = f"data_{datetime.now():%Y%m%d_%H%M%S}.csv"
    csv_fh   = open(csv_name, "w", newline="")
    writer   = csv.writer(csv_fh)
    writer.writerow(["timestamp_us", "adc"])

    # ==== プロット準備 ====
    plt.ion()
    fig, ax = plt.subplots(figsize=(10,4))
    buf = deque([0]*500, maxlen=500)
    ln, = ax.plot(buf)
    ax.set_ylim(0, 1023)
    ax.set_xlabel("Samples (latest ←)")
    ax.set_ylabel("ADC")
    fig.tight_layout()

    print("[INFO] Logging…  Ctrl-C で終了")
    try:
        while True:
            line = ser.readline().decode(errors="ignore").strip()
            if not line:
                continue
            if line.startswith("## FINISHED"):
                print("[INFO] FINISHED を受信、終了処理へ")
                break
            if line[0] == '#':
                continue

            try:
                t_us, v = map(int, line.split(','))
            except ValueError:
                print(f"[WARN] パース失敗: {line}")
                continue

            writer.writerow([t_us, v]); csv_fh.flush()

            buf.append(v)
            ln.set_ydata(buf)
            ax.relim(); ax.autoscale_view(scalex=False, scaley=False)
            fig.canvas.draw(); fig.canvas.flush_events()
    except KeyboardInterrupt:
        print("\n[INTERRUPT] ユーザ中断")
    finally:
        ser.close(); csv_fh.close()
        plt.ioff(); plt.show()
        print(f"[DONE] CSV 保存先: {os.path.abspath(csv_name)}")

# -------------------------------------------------  エントリ
if __name__ == "__main__":
    ag = argparse.ArgumentParser()
    ag.add_argument("-p", "--port", default="auto",
                    help="'auto' なら最新 /dev/tty.usbmodem* を自動使用")
    ag.add_argument("-b", "--baud", type=int, default=115200,
                    help="ボーレート (default 115200)")
    args = ag.parse_args()
    main(args.port, args.baud)