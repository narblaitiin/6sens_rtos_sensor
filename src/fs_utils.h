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

//  ========== prototypes ==================================================================
/**
 * @brief mount the Flash storage at /lfs mountpoint
 * 
 * @retval 0 on success
 * @retval <0 a negative error code on error, see <zephyr/fs/fs.h> for more info
 */
int mount_lfs();

/**
 * @brief check if a storage is mounted at /lfs
 * 
 * @retval true if a storage is mounted at /lfs
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
 * @param clean set to true to remove files after dumping, false else
 */
void dump_fs(bool clean);

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


#endif // APP_DOWNLOAD_H