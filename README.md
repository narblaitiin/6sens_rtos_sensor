# Code for 6Sens Project

## Overview

This application allow the monitoring of geological events, via a vibration sensor.

It features :
- Vibration acquisition and analysis with the STA/LTA algorithm
- LoRa communication to send the anomaly detected, samples linked to anomalies, and sensor status (temperature, battery, humidity)
- Zephyr filesystem support for storing acquired samples, and the sensor logs

The version of Zephyr RTOS used is the version v4.0.0.

## Board used
Original MDBT50Q board, powered by battery/solar panel. (see 6sens_prj repository/hardware part, for more information.)

## General Information of Application
You will need to register new devices in your application (with OTAA activation method). Once this is done, retain the TTN Device Address(4 Bytes), the TTN Network Key(16 Bytes) and the TTN Application Key (16 Bytes). You also have to make sure that the activation method is OTAA.

    - After you account was created, you have to create a new application
    - After that, you have to add a new end device on this application. You have to complete the various fields using the available data below in manually mode :
        Frequency Plan                  Europe 863-870 MHz (SF9 for RX2 - Recommended)
        LoRaWAN Version                 MAC V1.0.4
        Regional Parameter Version      RP002 Regional Parameters 1.0.4
        Activation by personalization   OTAA
        Application ID                  give a name
        JoinEUI                         00 00 00 00 00 00 00 00
        DevUI number 1                  random value for 8-byte address
        Device                          random value for 4-byte address
        AppKey                          random value for 16-byte address
        NwkSKey                         random value for 16-byte address
        AppSKey                         random value for 16-byte address

## Building and Running

To build the application, first follow [Zephy Quickstart](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) to install all the dependencies.

Then, run in the `zephyrproject` folder the following commands :

**Command to use**
```bash
# First, export your NODE_ID. Test node ID is 666
export NODE_ID=X

# Clean the existing build
west build -t pristine

# Build the application
west build -p always -b mdbt50q_lora_dev applications/6sens_rtos_sensor

# Flash it
west flash --runner jlink
```

## Testing

Units tests for the sensors are available in the `tests` folder.

They are separated in two types :

- `hardware-tests`, which verify that the sensor hardware is working correctly
- `software-tests`, which verify if the sensor algorithms are correct (STA/LTA) 

Check the README at the root of `hardware-tests` or `software-tests` for more info.

## Analysing data

All the scripts are located in the `analysis_scripts` folder.

### 1. Download the Data

First, grab the raw files off your board using download_data.py. This script connects via J-Link RTT, decodes the base64 chunks, and reconstructs your board's filesystem locally.

Run the script and specify the output folder :

`python download_data.py -o experiment/node_1`

Then, push the "dump button" on the board (for now, named `button1`).

The folder node_1 should contain the following folders :
- _data_, which contains recorded anomalies, identified by their timestamp (`signal_<timestamp>.dat`)
- _logs_, which contains the sensor logs

### 2. Visualize the data

Use display_samples.py to inspect signal graphs and timeline events. The script supports three modes:

#### Mode `all`: Overview (All Signals on One Page)

This script generates an A4 pdf containing all the recorded signals. 
It should be given the folder which contains the signals (signal_XXXX.dat) as input.
The pdf is generated in a folder named `out`, but this can be specified with the `-o` option.

`python display_samples.py all experiment/node_1/data`

#### Mode `single`: Signals alone

Generates a separate figure for every individual .dat file.

`python display_samples.py single experiment/node_1/data -o out/`

#### Mode 'timeline`: Timeline View

Shows detected events across multiple sensor nodes on a shared timeline.

`python display_samples.py timeline experiment/ -o out/`

Note for Timeline Mode: Point folder to the top-level parent folder (e.g., experiment/) containing your node_1, node_2, etc. folders.

### 3. Analyze Co-Detections

Run get_codetect.py to aggregate anomalies that occur close in time across multiple nodes.
The script scans directories matching node* and groups anomalies across nodes if they occur less than 5 seconds apart.

Run the script in the `experiment` folder :

`python get_codetect.py`