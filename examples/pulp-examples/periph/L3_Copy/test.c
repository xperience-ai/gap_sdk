/*
 * Copyright (C) 2017 ETH Zurich, University of Bologna and GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 */

#include "rt/rt_api.h"
#include "stdio.h"
#include "Gap8.h"

#define BUFF_SIZE 1024
#define COUNT 10

static L2_MEM signed char buff[BUFF_SIZE];
static L2_MEM signed char buff_rx[BUFF_SIZE];
static L2_MEM int count_enqueued = 0;
static L2_MEM int count_done = 0;
static L2_MEM rt_file_t *file;
static L2_MEM int curr_pos = 0;
static L2_MEM signed int checksum_tx = 0;
static L2_MEM signed int checksum_rx = 0;

L2_MEM rt_hyperram_t *hyperram;
L2_MEM unsigned char *hyperram_buff;

static signed int calculCheckSum (signed char *buffer, int size)
{
    signed int check = 0;
    for( int i=0; i<size; i++ ){
        check += buffer[i];
    }
    return check;
}

static void file_check(int total_size)
{
    int offset = 0;
    int iter = 0;
    int sizeLast = 0;
    if (total_size%BUFF_SIZE){
        iter = (total_size/BUFF_SIZE) + 1;
        sizeLast = total_size%BUFF_SIZE;
    }else{
        iter = (total_size/BUFF_SIZE);
    }
    for(int i=0; i<iter; i++){
        if (sizeLast && i == (iter-1)){
            rt_hyperram_read(hyperram, buff_rx, hyperram_buff+offset, sizeLast, NULL);
            offset += sizeLast;
            checksum_rx += calculCheckSum(buff_rx, sizeLast);
        }else{
            rt_hyperram_read(hyperram, buff_rx, hyperram_buff+offset, BUFF_SIZE, NULL);
            offset += BUFF_SIZE;
            checksum_rx += calculCheckSum(buff_rx, BUFF_SIZE);
        }
    }
    printf("checksum_rx: %X, offset: %d\n", checksum_rx, offset);
}

static void copy_file()
{
    int size_total = 0;
    int size = 0;
    do {
        size = rt_fs_read(file, buff, BUFF_SIZE, NULL);
        size = ((size + 3) & ~3);
        if(size) {
            rt_hyperram_write(hyperram, buff, hyperram_buff+size_total, size, NULL);
            checksum_tx += calculCheckSum(buff, size);
        }
        size_total += size;
    } while(size);
    printf("total read size: %d\n", size_total);
    printf("checksum_tx: %X\n", checksum_tx);
    file_check(size_total);
}

static void handle_error()
{
    int error = rt_error_current();
    printf("Caught error (error code 0x%x): %s\n", error, rt_error_str(error));
    exit(-1);
}

static void handle_async_error(void *arg, rt_event_t *event, int error, void *object)
{
    printf("Received error (error code 0x%x, event %p, object: %p): %s\n", error, event, object, rt_error_str(error));
    exit(-1);
}


int main()
{
    printf("Entering main controller\n");

    rt_error_conf(NULL, handle_async_error, NULL);

    if (rt_event_alloc(NULL, 16)) return -1;

    rt_padframe_profile_t *profile_hyper = rt_pad_profile_get("hyper");
    if (profile_hyper == NULL) {
        printf("pad config error\n");
        return 1;
    }
    rt_padframe_set(profile_hyper);
    printf ("pad config done\n");

    rt_fs_conf_t conf;
    rt_fs_conf_init(&conf);

    rt_fs_t *fs = rt_fs_mount("hyperflash", &conf, NULL);
    if (fs == NULL) handle_error();
    printf("fs mounted\n");

    file = rt_fs_open(fs, "Makefile", 0, NULL);
    if (file == NULL){
        printf("file open failed\n");
        return -1;
    }
    printf("file open success, size: %d\n", file->size);

    rt_hyperram_conf_t hyperram_conf;
    rt_hyperram_conf_init(&hyperram_conf);

    /* Open and initialize Hyper RAM device according to device name */
    hyperram = rt_hyperram_open("hyperram", &hyperram_conf, NULL);
    if (hyperram == NULL) return -1;

    /* Allocate Hyper RAM buffer with BUFF_SIZE */
    hyperram_buff = rt_hyperram_alloc(hyperram, file->size);
    if (hyperram_buff == NULL) return -1;

    copy_file();

    printf("\n");

    rt_fs_unmount(fs, NULL);
    if ( checksum_tx != checksum_rx )
        return 1;

    printf("Test success\n");
    return 0;

}
