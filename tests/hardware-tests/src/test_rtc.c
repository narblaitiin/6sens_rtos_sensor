#include <zephyr/ztest.h>
#include "app_ds3231.h"

ZTEST(rtc_test, test_no_delay)
{   
    const struct device * dev  = app_ds3231_init();
    zassert(app_ds3231_set_time(dev, 1784133987) == 0, "Failed to set RTC time !");
    uint64_t time = app_get_timestamp();
    printk("Time is %llu \n", time);
    zassert_within(time, 1784133987000, 1500);
}

ZTEST(rtc_test, test_delay)
{   
    const struct device * dev  = app_ds3231_init();
    printk("Lauching RTC test : It will take a few seconds, please wait\n");
    zassert(app_ds3231_set_time(dev, 1784133987) == 0, "Failed to set RTC time !");
    k_sleep(K_SECONDS(10));
    uint64_t time = app_get_timestamp();
    printk("Time is %llu \n", time);
    zassert_within(time, 1784133997000, 1500);
}




ZTEST_SUITE(rtc_test, NULL, NULL, NULL, NULL, NULL);
