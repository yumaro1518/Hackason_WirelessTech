import csv, re, pathlib

infile  = "raw.txt"
outfile = "measurement.csv"

pattern = re.compile(r'^(\d+),\s*(\d+)')   # 数字,数字 を抽出

with open(infile, encoding="utf-8") as fin, \
     open(outfile, "w", newline="") as fout:

    writer = csv.writer(fout)
    writer.writerow(["timestamp_us", "adc"])   # ヘッダー

    for line in fin:
        m = pattern.match(line.strip())
        if m:
            writer.writerow([m.group(1), m.group(2)])

print("written:", pathlib.Path(outfile).resolve())
