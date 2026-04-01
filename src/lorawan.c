#include "data_types.h"
#include "lorawan.h"
#include "app_ds3231.h"

//  ========== LoRaWAN callbacks ===========================================================
static void dl_callback(uint8_t port, uint8_t data_pending,
			int16_t rssi, int8_t snr,
			uint8_t len, const uint8_t *hex_data)
{
	printk("Port %d, Pending %d, RSSI %ddB, SNR %ddBm\n", port, data_pending, rssi, snr);
}

static void lorawan_datarate_changed(enum lorawan_datarate dr)
{
    uint8_t unused, max_size;
    lorawan_get_payload_sizes(&unused, &max_size);
    printk("new datarate: DR_%d, max payload %d\n", dr, max_size);
}

// initialize LoRaWAN protocol and register the device
const struct device *lora_dev = NULL;
struct lorawan_join_config join_cfg;

struct lorawan_downlink_cb downlink_cb = {
    .port = LW_RECV_PORT_ANY,
    .cb = dl_callback
};


/*** Initialize Lora chip, and register callbacks
 */
int lora_init() {
	lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));
	if (!device_is_ready(lora_dev)) {
		printk("%s: device not ready\n", lora_dev->name);
		return -1;
	}

	int ret = lorawan_start();
	if (ret < 0) {
		printk("lorawan_start failed. error: %d", ret);
		return -1;
	}

	lorawan_register_downlink_callback(&downlink_cb);
	lorawan_register_dr_changed_callback(lorawan_datarate_changed);

    return 0;
}

/*** Join a lorawan network
 * 
 *  Configuration is hardcoded in `lorawan.h` for now
 * 
 *  Return 0 on success
 */
int lora_joinnet() {
    if(lora_dev == NULL) {
        if(lora_init()) {
            printk("[ERROR] Could not initalize LoRa\n");
            return -1;
        }
    }

    uint8_t dev_eui[] = LORAWAN_DEV_EUI;
	uint8_t join_eui[] = LORAWAN_JOIN_EUI;
	uint8_t app_key[] = LORAWAN_APP_KEY;

    join_cfg.mode = LORAWAN_ACT_OTAA;
	join_cfg.dev_eui = dev_eui;
	join_cfg.otaa.join_eui = join_eui;
	join_cfg.otaa.app_key = app_key;
	join_cfg.otaa.nwk_key = app_key;
	join_cfg.otaa.dev_nonce = 0u;

    printk("joining network over OTAA\n");
	int ret = lorawan_join(&join_cfg);
	if (ret < 0) {
		printk("lorawan_join_network failed. error %d\n", ret);
	}
    
    return ret;
}

int lora_send_packet(PACKET_TYPE type, uint8_t * payload, int payload_size) {
    return lora_send_timestamp(type, app_get_timestamp(), payload, payload_size);
}

int lora_send_timestamp(PACKET_TYPE type, uint64_t timestamp, uint8_t * payload, int payload_size) {
    if(lora_dev == NULL) {
        if(lora_init()) {
            printk("[ERROR] Could not initalize LoRa\n");
            return -1;
        }
        if(lora_joinnet()) {
            printk("[ERROR] Could not join LoRa network\n");
            return -1;
        }
    }
    struct packet_t packet;
    packet.type = type;
    packet.timestamp = timestamp;
    const int header_size = sizeof(type) + sizeof(timestamp);
    
    for(int i= 0; i< payload_size ; i++) {
        packet.payload[i] = payload[i];
    }

    if(payload_size + header_size > 255) {
        printk("[ERROR] Trying to send more than 255 bytes !\n");
    }

    printk("Size of packet %d\n", header_size + payload_size);
    printk("Sending Payload of type : %d\n", type);
    printk("Packet Timestamp : %llu\n", packet.timestamp);
    printk("Timestamp hexa: %llx\n", packet.timestamp);
    printk("Size of packet-type : %d\n", sizeof(PACKET_TYPE));
    printk("Entire payload : ");
    int8_t * p = ((int8_t *) (&packet));
    for(int i = 0; i < payload_size + header_size; i++) {
        printk("%X ", (int8_t) *p);
        p++;
    }
    printk("\n");

    int ret = lorawan_send(LORAWAN_PORT, (int8_t *) &packet, payload_size + header_size, LORAWAN_MSG_UNCONFIRMED);
    printk("Message sent\n");
    if (ret < 0) {
        printk("lorawan_send failed: %d\n", ret);
    }

    // return ret == -EAGAIN ? 0 : ret;
    return ret;
}