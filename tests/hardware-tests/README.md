# Hardware Test

Tests for the sensor hardware. 

## Launch tests :

In this folder, run the following commands : 
```sh
# Build the tests
west build  -b mdbt50q_lora_dev --pristine  

# Flash the board
west flash --runner jlink 

# Check the results with JLinkRTTViewer
JLinkRTTViewer
```

Also, I tried using twister (Zephyr test utility), but it does not works well with RTT.
Latest test was :

```sh
west twister -T tests/ --platform mdbt50q_lora_dev --device-testing --west-runner="jlink"  --device-serial-pty PATH/start_test_logger.py --flash-before --west-flash="--reset
```