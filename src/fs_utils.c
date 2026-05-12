/*
 * Copyright (c) 2025
 * Hugo Reymond, Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */
#include "fs_utils.h"
#include <zephyr/sys/base64.h>

#include "config.h" // for log level
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_backend.h>
LOG_MODULE_REGISTER(filesystem);

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage2);
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
    .storage_dev = (void *)FIXED_PARTITION_ID(storage_partition),
};

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
void dump_fs(char * mnt_name, bool clean)
{
    if(!is_lfs_mounted(mnt_name)) {
        LOG_ERR("No mount to folder %s, cannot dump FS", mnt_name);
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
            if (rc < 0)
            {
                LOG_ERR("Could not delete %s. error: %d", file_path, rc);
            }
        }
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
