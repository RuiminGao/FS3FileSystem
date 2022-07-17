#ifndef FS3_DRIVER_PI_INCLUDED
#define FS3_DRIVER_PI_INCLUDED

#include "fs3_network.h"
#include "fs3_driver.h"

struct File {
    char *path;
    int16_t fd;
    int is_open;
    int size;
    int loc;
    struct Block *bhead;
    struct File *next;
};

struct Block {
    int track;
    int sector;
    struct Block *next;
};

int syscall(int opcode, int sector, int track, int ret, char *buf);

struct File * get_file_by_path(char *path);

struct File * get_file_by_fd(int16_t fd);

struct File * create_file(char *path);

struct Block * create_blk();

struct Block * next_blk(struct Block *bptr);

void delete_files();

void fix_track(int track);

void read_from_sector(int track, int sector, char *buf);

void write_to_sector(int track, int sector, char *buf);

#endif