"""
This script provides allow to connect via a J-Link RTT console to the SASTRESS board, to download vibration data
Requirements : 
- pip install pylink-square rich
"""


import pylink
import argparse
import time
import base64
from rich.progress import Progress
import os

# def connect(jlink_serial: int):
#     """
#     Establish a connection to the SASTRESS board.

#     Args:
#         jlink_serial (int): The serial number of the JLINK debugger to connect to.
#     Returns:
#         pylink.JLink: An initialized and connected JLink object.
#     Raises:
#         SystemExit: If the JLINK debugger cannot be opened or if the target device
#                     cannot be connected to.
#     """
#     print(pylink.JLink().connected_emulators())
#     print("Connecting to JLINK serial = {}".format(jlink_serial))
#     jlink = pylink.JLink()
#     jlink.open(serial_no=jlink_serial)
#     #jlink.open(serial_no=682809515)
#     if not jlink.connected():
#         print("ERROR : JLink is not connected. Maybe the serial number is wrong ?")
#         exit(1)
    
#     print("Connecting to sastress board ")
#     jlink.connect("NRF52840_XXAA")
#     if not jlink.target_connected():
#         print("ERROR : Could connect to JLINK debugger but not to the SASTRESS board.")
#         exit(1)
        
#     return jlink

def connect(jlink_serial: int | None):
    if jlink_serial is None:
        jlink_serial = autodetect_jlink_serial()

    print(f"Connecting to JLINK serial = {jlink_serial}")

    jlink = pylink.JLink()
    jlink.open(serial_no=jlink_serial)

    # IMPORTANT: Select SWD interface before connecting
    jlink.set_tif(pylink.enums.JLinkInterfaces.SWD)

    # Optional: lower speed for reliability
    jlink.set_speed(1000)

    print("Connecting to sastress board")
    jlink.connect("NRF52840_XXAA")

    if not jlink.target_connected():
        print("ERROR: J-Link connected but NOT to the target MCU.")
        exit(1)

    return jlink

def fs_downloader(jlink: pylink.JLink, export_folder: str):
    """
    Reads data from J-Link RTT (Real-Time Transfer) interface and saves files received via base64-encoded chunks.
    This function establishes an RTT connection, monitors incoming data, and reconstructs files that are
    transmitted as base64-encoded segments.
    Args:
        jlink (pylink.JLink): An initialized J-Link object used for RTT communication.
        export_folder (str): The folder in which files will be exported
    Returns:
        None
    Raises:
        IOError: If file operations (open/close/write) fail during data saving.
        base64.binascii.Error: If base64 decoding fails for a data chunk (caught and logged).
    Note:
        Expected RTT message format:
        - FILE:<filename> - Signals the start of a new file transfer
        - D:<base64_data> - Contains base64-encoded file data chunks
        - TOTAL_ENCODED: - Signals the end of file transmission
        - DUMP_END: - Signals the end of file system dump
        - Other lines are printed as log output
    """
    jlink.rtt_start()
    print("Waiting for RTT to settle", end="")
    while jlink.rtt_get_status() == 0:
        time.sleep(1)
    print("OK")

    # Initialize progress bar
    p = Progress()
    p.start()
    t = None
    
    current_file = None
    data = ""
    while True:
        # Append characters to data until a new line is found
        logs = jlink.rtt_read(0,200)
        data += ''.join([chr(i) for i in logs])
        if "\n" not in data:
            continue
        else:
            # When a new line character `\n` is found, parse the lines
            lines = data.split("\n")
            data = lines.pop()
            for line in lines:
                # FILE: <filename>
                if line.startswith("FILE:"):
                    filename = line.split(":")[1]
                    print("Found new file :", filename)
                    current_file = open(os.path.join(export_folder,filename), "wb")
                    t = p.add_task(filename, total=64*1024)
                # D: <base64 encoded data>
                elif line.startswith("D:") and current_file:
                    b64 = line.split(":")[1].replace("\n","").replace("\t","")
                    try: 
                        decoded = base64.b64decode(b64)
                        current_file.write(decoded)
                        p.update(t, advance=100)
                    except base64.binascii.Error as e:
                        print("Error trying to decode {} : {}".format(b64, e))
                # TOTAL_ENCODED : <int> -> Marks the end of the current file
                elif line.startswith("TOTAL_ENCODED:"):
                    print("End of file")
                    if current_file:
                        current_file.close()
                        p.remove_task(t)
                elif line.startswith("DUMP_END"):
                    break
                else:
                    print(line)

def main():
    parser = argparse.ArgumentParser(description='Read flash memory from SASTRESS board using JLink RTT')
    parser.add_argument('--serial', type=int, default=1050342163,
                      help='JLink debugger serial number (written on the board, under board revision)')
    parser.add_argument('-o', '--outputfolder', type=str, help='The folder in which the target filesystem should be dumped', default='lfs')
    args = parser.parse_args()
    jlink = connect(args.serial)
    export_folder = args.outputfolder
    os.makedirs(export_folder, exist_ok=True)
    fs_downloader(jlink, export_folder)

if __name__ == "__main__":
    main()