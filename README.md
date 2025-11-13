# Code for 6Sens Project : testing the LoRaWAN application with integration in TTN with real value of environmental data of the node and the velocity of the sensor.

## Overview
This application contains example code to allow testing of LoRaWAN Network Application for the last code before final code.
This code also allows to test the transmission of battery level, temperature, and humidity information from the sensor to the TTN app.

At the same time, samples from the geophone are also sent to the TTN app for a short (1 s) or long (10 s) duration. Priority is given to data from the sensor.

Two files are provided before the final code:

- app_ttn_tx: sends a number of samples in mV the size of the LTA_WINDOW_SIZE window (10s, 10ms sampling interval) to LoRaWAN

- app_sta_lta: calculates the ratio between the amplitude in mV of the samples from a short window (STA_WINDOW_SIZE) and a long window (LTA_WINDOW_SIZE).

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
The following commands clean build folder, build and flash the sample:

**Command to use**
````
west build -t pristine

west build -p always -b mdbt50q_lora_dev applications/6sens_rtos_sensor

west flash --runner jlink