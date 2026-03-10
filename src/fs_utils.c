/*
 * Copyright (c) 2025
 * Hugo Reymond, Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */
#include "fs_utils.h"
#include <zephyr/sys/base64.h>

//  ========== globals =====================================================================
#define TEST_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(lfs_storage)

//  ========== globals =====================================================================
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(lfs_storage);

static struct fs_mount_t lfs_storage_mnt = {
    .type = FS_LITTLEFS,
    .mnt_point = "/lfs",
    .fs_data = &lfs_storage,
    .storage_dev = (void *)FIXED_PARTITION_ID(lfs_storage),
};

//  ========== mount_lfs() ============================================================
int mount_lfs() {
    return fs_mount(&lfs_storage_mnt);
}

//  ========== is_lfs_mounted() ============================================================
bool is_lfs_mounted() {
    int mount_index = 0;
    char name[30];
    int rc = 0;
    while (rc == 0)
    {
        rc = fs_readmount(&mount_index, (const char * *) &name);
        if (strcmp(name, "/lfs") == 0)
        {
            return true;
        }
    }
    return false; 
}

//  ========== dump_fs() ============================================================
void dump_fs(bool clean)
{
    if(!is_lfs_mounted()) {
        mount_lfs();
    }
    // Get the lfs folder
    struct fs_dir_t root_dir;
    fs_dir_t_init(&root_dir);
    int rc = 0;
    rc = fs_opendir(&root_dir, "/lfs");
    switch (rc)
    {
    case -EINVAL:
        printk("Bad directory given...\n");
        break;
    case 0:
        break;
    default:
        printk("Error : error code=%d\n", rc);
        break;
    }

    printk("Reading content of lfs dir\n");
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
        snprintf(file_path, sizeof(file_path), "%s%s", "/lfs/", dir_entry.name);

        dump_file(file_path);
        if(clean) {
            rc = fs_unlink(file_path);
            if (rc < 0)
            {
                printk("Could not delete %s. error: %d\n", file_path, rc);
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
    // printf("Opening file %s\n", file_path);
    struct fs_file_t file;
    fs_file_t_init(&file);
    rc = fs_open(&file, file_path, FS_O_READ);
    if (rc < 0)
    {
        printk("file open failed. error: %d\n", rc);
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
            printf("Error encoding to base 64\n");
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
        printk("file closed failed. error: %d\n", rc);
    }
    return;
}
