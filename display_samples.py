from dataclasses import dataclass
from glob import glob
from struct import *
import json
import matplotlib.pyplot as plt
from datetime import datetime, timedelta
import argparse
from math import sqrt, ceil

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
            
            max_ampl = unpacked[1]
            min_ampl = unpacked[2]
            mean_ampl = unpacked[3]
            # Skip two bytes for padding
            ratio = unpacked[5]
            ## Skip 4 bytes for what ??
            signal = unpacked[8:]
            s = Sample(timestamp, max_ampl, min_ampl, mean_ampl, ratio, signal)
            print(datetime.fromtimestamp(unpacked[0]/1000), " - max: ", str(s.max_ampl), "min:", str(s.min_ampl), " mean", str(s.mean_ampl))
            samples.append(s)
    return samples

def display_sample(sample: Sample):
    fig = plt.figure(str(datetime.fromtimestamp(sample.timestamp/1000) + timedelta(hours=2)) + " ratio : " + str(sample.ratio))
    plt.plot(sample.signal)
    plt.show()

def display_all_samples(samples: list[Sample]):
    nb_sample = len(samples)
    plot_dims = ceil(sqrt(nb_sample))
    fig, axes = plt.subplots(plot_dims, plot_dims, sharex=True, sharey=True)
    signal_size = len(samples[0].signal)
    time = [10*i for i in range(signal_size)]
    for i in range(nb_sample):
        x = i % plot_dims
        y =  i // plot_dims
        sample = samples[i]
        print(axes[x])
        date = datetime.fromtimestamp(sample.timestamp/1000) + timedelta(hours=2)
        axes[x][y].set_title(date.strftime("%H:%M:%S"))
        axes[x][y].plot(time, sample.signal)
    plt.tight_layout(pad=0.1, h_pad=0.3)
    plt.show()

if __name__ == "__main__":
    parser = argparse.ArgumentParser("Display samples from the SASTRESS Sensor")
    parser.add_argument("folder")
    args = parser.parse_args()
    print("Opening samples")
    samples = parse_samples(args.folder)
    print("Found", len(samples), "samples")
    display_all_samples(samples)
    exit()
    for sample in samples:
        display_sample(sample)
