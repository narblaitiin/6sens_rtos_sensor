#!/usr/bin/env python3
import numpy as np
from scipy.signal import spectrogram
import matplotlib.pyplot as plt
import glob
import argparse

file_folder = "lfs"
fs = 100  # sampling rate (Hz)

def show_spectrogram(data_folder: str, show_raw_signal: bool):
    # Concatenate data from all file in the data folder
    all_data = []
    for fpath in sorted(glob.glob(data_folder + "/geophone_*.dat")):
        data = np.fromfile(fpath, dtype=np.int16)
        all_data.append(data)
    data = np.array(np.concatenate(all_data), dtype=np.float64)
    print(len(data))
    # data = data[180*fs:600*fs]

    # Separate data into chunks of 128 samples 
    batch_size = 128
    sta_window_size = 10 #  Number of samples in the STA windows 10 *10ms = 0.1 s
    lta_sta_factor = 10
    lta_window_size = sta_window_size * lta_sta_factor
    n_batches = data.size // batch_size
    trimmed = data[: n_batches * batch_size]
    batches = trimmed.reshape(n_batches, batch_size)

    # Compute, for each chunk, its min, max, and average value.
    mins = batches.min(axis=1)
    maxs = batches.max(axis=1)
    mean = batches.mean(axis=1)

    data_centered = data - data.mean()
    squared = np.power(data_centered,2)
    abs = np.abs(data_centered)
    # Compute lta/sta and er ratios
    lta = np.array([np.average(abs[max(0, i-lta_window_size):i+1]) for i in range(len(abs))])
    sta = np.array([np.average(abs[max(0, i-sta_window_size):i+1]) for i in range(len(abs))])

    lta_sta = sta/ lta

    er = [squared[max(0, i-sta_window_size):i+1].sum()/squared[i:i+sta_window_size+1].sum() for i in range(sta_window_size, len(squared) - sta_window_size)]
    print(er[1:10])
    er = [1]*sta_window_size + er + [1]*sta_window_size
    mer = np.power(er*abs,3)

    t_samples = np.arange(0, len(data)/fs, 1/fs)
    # Compute spectrogram
    f, t_spect, Sxx = spectrogram(data, fs=fs, nperseg=256, noverlap=128)
    print("Fin du signal à ", t_samples[-1], "s")
    # Display raw signal
    if show_raw_signal:
        plt.plot(data)
        plt.ylabel('Amplitude')
        plt.xlabel('Time [s]')
        plt.show()

    fig, (ax0, ax1, ax2, ax3, ax4, ax5, ax6) = plt.subplots(7, 1, sharex='all', figsize=(10, 8))
    fig.canvas.manager.set_window_title("Plots for data in " + data_folder) 
    ax0.plot(t_samples, data)
    ax0.set_ylabel('Amplitude')
    ax0.set_xlabel('Time [s]')
    ax0.set_title('Signal brut')

    # Spectrogram on the first subplot
    # im = ax1.pcolormesh(t_spect, f, 10 * np.log10(Sxx), shading='gouraud')
    ax1.set_ylabel('Frequency [Hz]')
    ax1.set_title('Spectrogramme')

    # Truncate series to common length to avoid shape mismatches
    n = min(len(t_spect), len(maxs), len(mins))
    t_trunc = t_spect[:n]

    # Max, Min, Mean on the second subplot
    ax2.plot(t_trunc, maxs[:n], 'r-', linewidth=1.5, label='Max')
    ax2.plot(t_trunc, mins[:n], 'b-', linewidth=1.5, label='Min')
    ax2.plot(t_trunc, mean[:n], 'g-', linewidth=1.5, label='Mean absolute')
    # fig.colorbar(im, ax=ax1, label='Power (dB)')
    ax2.set_title('Moyenne/Max/Min sur ' + str(batch_size) + ' samples')
    ax2.set_ylabel('Amplitude')
    ax2.set_xlabel('Time [s]')
    ax2.legend()

    # STA LTA on the third subplot
    STA_length = sta_window_size * 1/fs
    LTA_length = lta_window_size * 1/fs
    ax3.plot(t_samples, list(sta), 'r-' , linewidth=1.5, label=f'STA ({STA_length}s)')
    ax3.plot(t_samples, list(lta), 'b-', linewidth=1.5, label=f'LTA ({LTA_length}s)')
    ax3.set_title('STA LTA')
    ax3.set_xlabel('Time [s]')
    ax3.legend()

    # STA/LTA ratio on the fourth subplot
    ax4.plot(t_samples, list(lta_sta),  linewidth=1.5, label='STA/LTA')
    ax4.set_title('Ratio STA/LTA')
    ax4.set_ylabel('STA/LTA ratio')
    ax4.set_xlabel('Time [s]')
    ax4.legend()

    # Energy on the fifth subplot
    ax5.plot(t_samples, er,  linewidth=1.5, label='ER1')
    ax5.plot(t_samples, [0] + er[:-1],  linewidth=1.5, label='ER2')
    ax5.set_ylabel('Energy ratio')
    ax5.set_xlabel('Time [s]')
    ax5.set_title('Energy Ratio')
    ax5.legend()

    # MER on the fifth subplot
    ax6.plot(t_samples, mer,  linewidth=1.5)
    ax6.set_ylabel('MER ratio')
    ax6.set_xlabel('Time [s]')
    ax6.set_title('MER ratio (Window size=' + str(STA_length) +"s)")
    
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Script to analyze vibration data extracted from SASTRESS sensor. "
                    "Display spectrogram, metrics (min, max, mean), STA/LTA, and MER (modified energy ratios)."
    )
    parser.add_argument("-d", "--dir", default="lfs", help="Data folder path (default: lfs)")
    parser.add_argument("--raw", help="Show raw signal (default: false)", action='store_true')
    args = parser.parse_args()

    show_spectrogram(args.dir, args.raw)