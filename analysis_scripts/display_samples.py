from dataclasses import dataclass
from glob import glob
import os
from struct import *
import json
from typing import List
import matplotlib.pyplot as plt
from datetime import datetime, timedelta
import argparse
from math import sqrt, ceil
import subprocess

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
    files = glob(f"{folder}/*")
    for f_name in sorted(files) :
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
            # print(datetime.fromtimestamp(unpacked[0]/1000), " - max: ", str(s.max_ampl), "min:", str(s.min_ampl), " mean", str(s.mean_ampl))
            samples.append(s)
    return samples

def squared_avg(samples: List[int]):
    sum = 0 
    for x in samples:
        x = x - 1700
        sum += x*x
    return sum/len(samples)

def display_sample(sample: Sample):
    fig = plt.figure(str(datetime.fromtimestamp(sample.timestamp/1000) + timedelta(hours=2)) + " ratio : " + str(sample.ratio))
    # On attends 2.5s avant de stocker la donnée, et on stocke 5s
    # t_detect est donc au milieu de notre graphe
    t_detect = 2500
    # sta_window = t_detect - 1s
    start_sta = t_detect - 1000
    lta = squared_avg(sample.signal[0:t_detect//10])
    sta = squared_avg(sample.signal[start_sta//10:t_detect//10])

    time = [10*i for i in range(len(sample.signal))]
    plt.plot(time, sample.signal)
    ax = fig.get_axes()[0]
    ax.add_patch(plt.Rectangle((start_sta, 0), 1000, 2800, ls="--", ec="c", fc="none"))
    fig.suptitle( f"Ratio with LTA=2.5s : {sta/lta}")
    plt.show()

def display_all_samples(samples: list[Sample], folder: str):
    nb_sample = len(samples)
    y_plot_dims = ceil(sqrt(nb_sample))
    x_plot_dims = ceil(nb_sample / y_plot_dims)
    start_date = datetime.fromtimestamp(samples[0].timestamp/1000)
    
    fig, axes = plt.subplots(x_plot_dims, y_plot_dims, sharex=True, sharey=True)
    

    fig.suptitle(f"Signal recorded on {start_date.strftime("%d/%m/%Y")} - folder : {folder}")
    signal_size = len(samples[0].signal)
    time = [10*i for i in range(signal_size)]
    for i in range(nb_sample):
        x = i % x_plot_dims
        y =  i // x_plot_dims
        sample = samples[i]
        print(axes[x])
        date = datetime.fromtimestamp(sample.timestamp/1000)
        axes[x][y].set_title(date.strftime("%H:%M:%S"))
        axes[x][y].plot(time, sample.signal)
    fig.set_size_inches(11.69,8.27) # set to A4
    plt.tight_layout(pad=2, h_pad=0.3)


def display_all(input_folder: str, output_folder: str):
    """
    Show all recorded signals of a sensor on a single A4 page
    Save that page in PDF
    """
    samples = parse_samples(input_folder)
    display_all_samples(samples, input_folder)
    filename = input_folder.replace("data", "").replace("/","-")
    plt.savefig(os.path.join(output_folder, filename + ".pdf"))
    plt.show()
    
def display_single(input_folder: str):
    """
    Show one figure per recorded signal.
    """
    samples = parse_samples(input_folder)
    for sample in samples:
        display_sample(sample)

def display_timeline(input_folder: str, output_folder: str):
    nodes_folder = glob("node_*", root_dir=input_folder)
    
    # Populate the sample dictionnary 
    samples_d : dict[str: Sample] = {}
    for node in nodes_folder:
        node_data_folder = os.path.join(input_folder, node, "data")
        # Pas beau : C'est pour récupérer le dossier nommé `data``
        if not os.path.exists(node_data_folder) : 
            print(f"ERROR : could not find any folder named `data` in {input_folder}/{node} : {node_data_folder}")
            raise FileNotFoundError
        samples_d[node] = parse_samples(node_data_folder) 
        print(f'Found {len(samples_d)} for { node_data_folder }')
    

    fig, axes = plt.subplots(len(nodes_folder), 1, sharex=True)
    
    fig.suptitle(f"Signal detected and ratio for different nodes - folder : {input_folder}")

    for axe, node_id in zip(axes, samples_d):
        samples = samples_d[node_id]
        t = [datetime.fromtimestamp(s.timestamp/1000) for s in samples]
        ratio = [s.ratio for s in samples]
        axe.scatter(t, ratio)
        axe.set_ylabel(f"Ratio for {node_id}")
    fig.set_size_inches(11.69,8.27) # set to A4
    plt.savefig(os.path.join(output_folder, input_folder.replace("/", "-")  + "-timeline.pdf"))
    plt.tight_layout(pad=2, h_pad=0.3)
    plt.show()


if __name__ == "__main__":
    parser = argparse.ArgumentParser("Display samples from the SASTRESS Sensor",
                                     description="""
        Display samples from the SASTRESS sensor in the .dat format
        It shows different data given the argument :  : 
        - all : display all recorded signals of a given sensor on a single A4 page 
        - single : display one figure per recorded signal.
        - timeline : show the timeline of event detected
        """)
        
    parser.add_argument("command", default="all", choices=["all", "single", "timeline"])
    parser.add_argument("folder", help="""
            The folder containing the data. 
            With all and single command : the folder containing the .dat files. Ex : experiment/node_1/data
            With timeline : a folder containing folder named "node_*", one per sensor. Ex : experiment/, which contains node_1 and node_2 folders
            """)
    parser.add_argument("-output", "-o", default="out", help="The output folder for pdf export")
    args = parser.parse_args()
    print("Opening samples")
    
    os.makedirs(args.output, exist_ok=True)
    if args.command == "all":
        display_all(args.folder, args.output)
    elif args.command == "single":
        display_single(args.folder)
    elif args.command == "timeline":
        display_timeline(args.folder, args.output)