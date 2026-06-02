/*
 * Copyright (c) 2025
 * Hugo Reymond, Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */
#include "fs_utils.h"
#include <zephyr/sys/base64.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "config.h" // for log level
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_backend.h>
LOG_MODULE_REGISTER(filesystem);

K_THREAD_STACK_DEFINE(dump_stack, 2048);
// declare thread data structure
struct k_thread dump_tdata;

#define TEST_PARTITION_OFFSET_DATA FIXED_PARTITION_OFFSET(data_storage)
#define TEST_PARTITION_OFFSET_LOG FIXED_PARTITION_OFFSET(log_storage)


FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage2);
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);


static struct fs_mount_t data_storage_mnt = {
    .type = FS_LITTLEFS,
    .mnt_point = "/data",
    .fs_data = &storage,
    .storage_dev = (void *)FIXED_PARTITION_ID(data_storage),
};

static struct fs_mount_t log_storage_mnt = {
    .type = FS_LITTLEFS,
    .mnt_point = "/log",
    .fs_data = &storage2,
    .storage_dev = (void *)FIXED_PARTITION_ID(log_storage),
};


// Buttons
static const struct gpio_dt_spec dump_btn = GPIO_DT_SPEC_GET(DUMP_BUTTON, gpios);
static const struct gpio_dt_spec clr_btn = GPIO_DT_SPEC_GET(CLEAR_BUTTON, gpios);

// Led - TODO make a dedicated driver to avoid conflicts ?
static const struct gpio_dt_spec led_0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

// ===== Callback =====
static struct gpio_callback btn_dump_callback;
static struct gpio_callback btn_clear_callback;
static bool clear_called = false;
K_EVENT_DEFINE(clr_dump_evt); // 1 for dump, 2 for clear

//  ========== mount_lfs() ============================================================
int mount_lfs() {
    LOG_INF("Mounting data partition.");
    int err = fs_mount(&data_storage_mnt);
    if (err) {
        LOG_ERR("Could not mount data partition. Error %d", err);
        return err;
    }

    LOG_INF("Mounting log partition.");
    err = fs_mount(&log_storage_mnt);
    if (err) {
        LOG_ERR("Could not mount log partition. Error %d", err);
        return err;
    }

    return err;
}

//  ========== is_lfs_mounted() ============================================================
bool is_lfs_mounted(char * mnt_name) {
    int mount_index = 0;
    const char * p;
    int rc = 0;
    while (rc == 0)
    {
        rc = fs_readmount(&mount_index, &p);
        if (strcmp(p, mnt_name) == 0)
        {
            return true;
        }
    }
    return false; 
}

//  ========== dump_fs() ============================================================
void rm_fs_content(char * mnt_name)
{
    int rc = 0;
    char file_path[260];
    LOG_INF("Cleaning partition %s\n", mnt_name);
    if(!is_lfs_mounted(mnt_name)) {
        LOG_ERR("No mount to folder %s, cannot rm FS", mnt_name);
    }

     // Get the partition folder
    struct fs_dir_t root_dir;
    fs_dir_t_init(&root_dir);
    rc = fs_opendir(&root_dir, mnt_name);
    
    switch (rc)
    {
    case -EINVAL:
        LOG_ERR("Bad directory given...");
        break;
    case 0:
        break;
    default:
        LOG_ERR("Error : error code=%d", rc);
        break; // TODO ADD return on error
    }

    printk("Reading content of %s dir\n", mnt_name);
    
    struct fs_dirent dir_entry;
    while (true)
    {
        rc = fs_readdir(&root_dir, &dir_entry);
        if (dir_entry.name[0] == 0 || rc < 0)
        {
            break;
        }
        
        
        snprintf(file_path, sizeof(file_path), "%s/%s", mnt_name, dir_entry.name);
        printk("Removing:%s\n", file_path);
        rc = fs_unlink(file_path);
        if (rc < 0)
        {
            LOG_ERR("Could not delete %s. error: %d", file_path, rc);
        }
    
    }
}
//  ========== dump_fs() ============================================================
void dump_fs(char * mnt_name, bool clean)
{   
    if(!is_lfs_mounted(mnt_name)) {
        LOG_ERR("No mount to folder %s, cannot dump FS", mnt_name);
        return;
    }
    // Get the lfs folder
    struct fs_dir_t root_dir;
    
    fs_dir_t_init(&root_dir);
    int rc = 0;
    rc = fs_opendir(&root_dir, mnt_name);
    switch (rc)
    {
    case -EINVAL:
        LOG_ERR("Bad directory given...");
        break;
    case 0:
        break;
    default:
        LOG_ERR("Error : error code=%d", rc);
        break; // TODO ADD return on error
    }

    printk("Reading content of %s dir\n", mnt_name);
    printk("DUMP_START:%s\n", mnt_name);
    struct fs_dirent dir_entry;
    while (true)
    {
        rc = fs_readdir(&root_dir, &dir_entry);
        if (dir_entry.name[0] == 0 || rc < 0)
        {
            break;
        }
        printk("FILE:%s\n", dir_entry.name);
        
        char file_path[261];
        snprintf(file_path, sizeof(file_path), "%s/%s", mnt_name, dir_entry.name);

        dump_file(file_path);
        if(clean) {
            rc = fs_unlink(file_path);
            if (rc != 0)
            {
                LOG_ERR("Could not delete %s. error: %d", file_path, rc);
            }
        }
        continue;
    }
    rc = fs_closedir(&root_dir);
    if(rc < 0) {
        LOG_WRN("Could not close %s folder", mnt_name);
    } 
    printk("DUMP_END\n");
}

//  ========== dump_file() ============================================================
void dump_file(char * file_path)
{   
    int rc;
    unsigned char buffer[100];
    unsigned char base64_encoded[200];
    struct fs_file_t file;
    fs_file_t_init(&file);
    rc = fs_open(&file, file_path, FS_O_READ);
    if (rc < 0)
    {
        LOG_ERR("file open failed. error: %d", rc);
        return;
    }
    
    int total_encoded = 0;
    int written = 1;
    while(written > 0) {
        printk("D:");
        written = fs_read(&file, buffer, 100);
        size_t encoded = 0;
        rc = base64_encode(base64_encoded, 200, &encoded, buffer, written);
        base64_encoded[encoded] = 0;
        if(rc != 0) {
            LOG_ERR("Error encoding to base 64");
            return;
        }
        printk("%s", base64_encoded);
        total_encoded += encoded;
        printk("\n");
        k_sleep(K_MSEC(15));
    }

    printk("TOTAL_ENCODED:%d\n", total_encoded);
    
    rc = fs_close(&file);
    if (rc < 0)
    {
        LOG_ERR("file closed failed. error: %d", rc);
    }
    return;
}


int setup_button(const struct gpio_dt_spec btn) {
    // --- Button ---
    if (!gpio_is_ready_dt(&btn)) {
        LOG_ERR("Button not ready");
        return -1;
    }

    int ret = gpio_pin_configure_dt(&btn,
                                GPIO_INPUT | btn.dt_flags);
    if (ret < 0) {
        LOG_ERR("Button config failed");
        return -1;
    }

    ret = gpio_pin_interrupt_configure_dt(&btn,
                                          GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Interrupt config failed");
        return -1;
    }
    return 0;
}

static void dump_button_called() {
    k_event_set(&clr_dump_evt, 0b01);
}


static void clear_button_called() {
   k_event_set(&clr_dump_evt, 0b10);
}


int setup_buttons() {
    LOG_INF("Setting up buttons");
    int ret = setup_button(dump_btn);
    if(ret) {
        LOG_ERR("Error setting up dump button");
        return -1;
    }

    gpio_init_callback(&btn_dump_callback,
                       dump_button_called,
                       BIT(dump_btn.pin));

    ret = gpio_add_callback(dump_btn.port, &btn_dump_callback);
    if(ret) {
        LOG_ERR("Error registering dump callback : %d", ret);
        return -1;
    }
    
    ret = setup_button(clr_btn);
    if(ret) {
        LOG_ERR("Error setting up clear button : %d", ret);
        return -1;
    }
    
    gpio_init_callback(&btn_clear_callback,
                       clear_button_called,
                       BIT(clr_btn.pin));

    ret = gpio_add_callback(clr_btn.port, &btn_clear_callback);
    if(ret) {
        LOG_ERR("Error registering dump callback : %d", ret);
        return -1;
    }
    LOG_INF("DONE");
    return 0;
}

void app_storage() {
    uint32_t  events;

    while(true) {
        events = k_event_wait(&clr_dump_evt, 0x003, true, K_MINUTES(1));

        // One minute elapsed, reset remove button
        if (events == 0 ) {
            gpio_pin_set_dt(&led_0, 0);
            clear_called = false;
        }
        // Dump event
        else if ((events & 0b01) == 0b01) {
            LOG_INF("Dumping flash data...");
            dump_fs("/data", false);
            dump_fs("/log", false);
        } 
        // Remove event
        else if ((events & 0b10) == 0b10) {
            LOG_INF("Clear button pushed...");
            if(clear_called == false) {
                LOG_WRN("TRYING TO REMOVE THE CONTENT OF /data FS\n");
                LOG_WRN("Are you sure ? Press again in the following 10s if yes\n");
                clear_called = true;
                gpio_pin_set_dt(&led_0, 1);
                continue;
            }

            LOG_INF("Clear button pushed twice, removing fs content");
            rm_fs_content("/data");
            rm_fs_content("/log");

            for(int i = 0; i < 30; i++) {
                gpio_pin_set_dt(&led_0, 0);
                k_msleep(100);
                gpio_pin_set_dt(&led_0, 1);
                k_msleep(100);
            }
        }
        k_msleep(500);
    }
}



void start_dump_rm_thread() {
    int ret = setup_buttons(); 
    if(ret) {
        LOG_ERR("Could no setup buttons, we are not starting the dump/rm thread");
    }

    k_thread_create(&dump_tdata, dump_stack,
                    K_THREAD_STACK_SIZEOF(dump_stack),
                    app_storage, NULL, NULL, NULL,
                    1, 0, K_NO_WAIT);
}