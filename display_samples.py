from dataclasses import dataclass
from glob import glob
from struct import *
import json
import matplotlib.pyplot as plt
from datetime import datetime, timedelta
import argparse

@dataclass
class Sample:
    timestamp: int
    max_ampl: int
    min_ampl: int
    mean_ampl: int
    ratio : float
    signal : list[int]

def parse_samples(folder) -> list[Sample]: 
    samples = []
    for f_name in glob(f"{folder}/*"):
        print(f"Opening {f_name}")
        with open(f_name, 'rb') as f:
            data = f.read()
            if len(data) != 1024:
                print(f"{f_name} is not the expected size : got {len(data)}, expected 1024")
                continue

            unpacked = unpack('<Q4Hf502H',data)
            timestamp = unpacked[0]
            print(datetime.fromtimestamp(unpacked[0]/1000))
            print(timestamp)
            max_ampl = unpacked[1]
            min_ampl = unpacked[2]
            mean_ampl = unpacked[3]
            # Skip two bytes for padding
            ratio = unpacked[5]
            ## Skip 4 bytes for what ??
            signal = unpacked[8:]
            s = Sample(timestamp, max_ampl, min_ampl, mean_ampl, ratio, signal)
            samples.append(s)
    return samples

def display_sample(sample: Sample):
    fig = plt.figure(str(datetime.fromtimestamp(sample.timestamp/1000) + timedelta(hours=2)) + " ratio : " + str(sample.ratio))
    plt.plot(sample.signal)
    plt.show()

def display_all_samples(samples: list[Sample]):
    pass

if __name__ == "__main__":
    parser = argparse.ArgumentParser("Display samples from the SASTRESS Sensor")
    parser.add_argument("folder")
    args = parser.parse_args()
    print("Opening samples")
    samples = parse_samples(args.folder)
    for sample in samples:
        display_sample(sample)