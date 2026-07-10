#include <zephyr/ztest.h>
#include "lorawan.h"
#include "data_types.h"

ZTEST(lorawan_test, test_init_join){
    int ret = lora_init();
    zassert(ret == 0, "Error, could not initialize lora");
    ret = lora_joinnet();
    zassert(ret == 0, "Error, could not join a lorawan network");
}

// Disabled for now, as if the test fail, it stays forever in the function, and we cannot test the other features...
// ZTEST(lorawan_test, test_send){
//     struct bth_payload_t payload;
//     payload.battery = 1;
//     payload.temperature = 2;
//     payload.humidity = 3;

//     int ret = lora_send_packet(BTH, (uint8_t *) &payload, sizeof(struct bth_payload_t));


//     zassert(ret == 0, "Something wrong happened when trying to send a message !");
//     printk("BTH Lora message sent !\nExpected JSON decoded message is :\n");
// }

ZTEST_SUITE(lorawan_test, NULL, NULL, NULL, NULL, NULL);
