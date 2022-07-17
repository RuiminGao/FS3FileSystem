// LRU cache implemented by BST & Queue

#ifndef FS3_CACHE_PI_INCLUDED
#define FS3_CACHE_PI_INCLUDED

#include "fs3_cache.h"

struct Cache {
    int track;
    int sector;
    char data[FS3_SECTOR_SIZE];
    struct Cache *left;
    struct Cache *right;
    struct Cache *prev;
    struct Cache *next;
} *croot, *chead, *ctail;

int sector_less(int track0, int sector0, int track1, int sector1);

struct Cache * create_cache(int track, int sector, char *buf);

struct Cache * pop_predecessor(struct Cache *cptr);

struct Cache * remove_cache(struct Cache *cptr);

struct Cache * pop_lru(struct Cache *cptr);

struct Cache * insert_cache(struct Cache *cptr, int track, int sector, char *buf);

void move_to_tail(struct Cache *cptr);

#endif