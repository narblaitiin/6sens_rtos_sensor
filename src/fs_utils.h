/*
 * Copyright (c) 2025
 * Hugo Reymond, Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef APP_DOWLOAD_H
#define APP_DOWLOAD_H

//  ========== includes ====================================================================
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>

//  ========== defines =====================================================================
#define FILE_PREFIX             "/lfs/geophone"
#define FILE_EXT                ".dat"
#define MAX_FILE_SIZE           (64 * 1024)   // 512 KB per file (adjustable)
#define STORAGE_BUFFER_SIZE     64

// Buttons for dumping 
#define DUMP_BUTTON  DT_ALIAS(sw1)
#define CLEAR_BUTTON  DT_ALIAS(sw2)
//  ========== prototypes ==================================================================
/**
 * @brief mount the Flash storage at /lfs mountpoint
 * 
 * @retval 0 on success
 * @retval <0 a negative error code on error, see <zephyr/fs/fs.h> for more info
 */
int mount_lfs();

/**
 * @brief check if a storage is mounted at /<mnt_name>
 * 
 * @param mnt_name the name of the partition to check (e.g "/log" or "/data")
 * 
 * @retval true if a storage is mounted at /<mnt_name>
 * @retval false else
 */
bool is_lfs_mounted();

/**
 * @brief Dump the content of the filesystem `lfs` to the console
 *
 * Dumps the content of the filesystem directory `lfs` to the console
 * 
 * The output format is the following :
 *
 * - FILE:<filename> indicates that the function opens a new file
 * - D:<data> represent base64 encoded data from the previous file opened. Dumping the file content takes multiples D:<data> message, as each message correspond to 100 bytes
 * - TOTAL_ENCODED:<number> indicates the total number of encoded bytes, i.e the file size
 * 
 * Output that do not start with the FILE:, D:, or TOTAL_ENCODED:, is log information
 * 
 * @param mnt_name the name of the partition to dump (e.g "/log" or "/data")
 * @param clean set to true to remove files after dumping, false else
 */
void dump_fs(char * mnt_name, bool clean);

/**
 * @brief Remove the content of a partition/folder
 * 
 * @param mnt_name the name of the partition to clean (e.g "/log" or "/data")
 */
void rm_fs_content(char * mnt_name);

/**
 * @brief Dump the content of a given file
 *
 * @param file_path Path to the file to dump (absolute path on the mounted filesystem, e.g. "/lfs/geophone0.dat").
 * 
 * The output format is the following :
 *
 * - D:<data> represent base64 encoded data from the file opened. Dumping the file content takes multiples D:<data> message, as each message correspond to 100 bytes
 * - TOTAL_ENCODED:<number> indicates the total number of encoded bytes, i.e the file size
 * 
 * Output that do not start with the D:, or TOTAL_ENCODED:, is log information
 */
void dump_file(char * file_path);

/**
 * @brief Setup the button 1 and 2 
 *  - Button 1 dumps the content of the /data and /log volume when pushed
 *  - Button 2 remove the content of the /data volume and /log volume
 * 
 *  
 *  */ 
int setup_buttons();

/** 
 *  @brief start the thread waiting for dump/rm and setup buttons 1 and 2
 * 
 *  This thread is needed as printing from ISR can lead to deadlock with SEGGER RTT console
 */
void start_dump_rm_thread();
#endif // APP_DOWNLOAD_H