////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_cache.c
//  Description    : This is the implementation of the cache for the 
//                   FS3 filesystem interface.
//
//  Author         : Patrick McDaniel
//  Last Modified  : Sun 17 Oct 2021 09:36:52 AM EDT
//

// Includes
#include <cmpsc311_log.h>
#include <string.h>
#include <stdlib.h>

// Project Includes
#include <fs3_cache.h>
#include <fs3_cache_pi.h>

//
// Support Macros/Data
int cache_size = 0;
int cache_capacity = 0;

int insert_count = 0;
int get_count = 0;
int hit_count = 0;
int miss_count = 0;

//
// Implementation

int less(int track0, int sector0, int track1, int sector1) {
    if (track0 != track1) return track0 < track1;
    return sector0 < sector1;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_init_cache
// Description  : Initialize the cache with a fixed number of cache lines
//
// Inputs       : cachelines - the number of cache lines to include in cache
// Outputs      : 0 if successful, -1 if failure

int fs3_init_cache(uint16_t cachelines) {
    croot = NULL;
    chead = NULL;
    ctail = NULL;
    cache_size = 0;
    cache_capacity = cachelines;
    insert_count = 0;
    get_count = 0;
    hit_count = 0;
    miss_count = 0;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_close_cache
// Description  : Close the cache, freeing any buffers held in it
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int fs3_close_cache(void)  {
    while (chead) {
        struct Cache *cptr = chead->next;
        free(chead);
        chead = cptr;
    }
    return 0;
}

struct Cache * create_cache(int track, int sector, char *buf) {
    struct Cache *cptr = (struct Cache *) malloc(sizeof(struct Cache));

    // load data
    cptr->track = track;
    cptr->sector = sector;
    memcpy(cptr->data, buf, FS3_SECTOR_SIZE);
    cptr->left = NULL;
    cptr->right = NULL;

    // put into queue
    if (chead == NULL) {
        cptr->prev = NULL;
        cptr->next = NULL;
        chead = ctail = cptr;
    } else {
        cptr->prev = ctail;
        cptr->next = NULL;
        ctail->next = cptr;
        ctail = cptr;
    }

    cache_size++;
    return cptr;
}

struct Cache * remove_cache(struct Cache *cptr) {
    // resolve bst
    struct Cache *curr;
    if (!cptr->left && !cptr->right) curr = NULL;
    else if (!cptr->left) curr = cptr->right;
    else if (!cptr->right) curr = cptr->left;
    else {
        struct Cache *parent = NULL;
        struct Cache *pred = cptr->left;

        // find predecessor
        while (pred->right) {
            parent = pred;
            pred = pred->right;
        }
        
        // remove predecessor
        if (parent) {
            parent->right = pred->left;
            pred->left = cptr->left;
            pred->right = cptr->right;
        } else {
            pred->right = cptr->right;
        }

        curr = pred;
    }

    // free cache, remove from queue
    cache_size--;
    chead = chead->next;
    chead->prev = NULL;
    free(cptr);

    return curr;
}

struct Cache * pop_lru(struct Cache *cptr) {
    if (cptr == chead) {
        return remove_cache(cptr);
    } else if (less(cptr->track, cptr->sector, chead->track, chead->sector)) {
        cptr->right = pop_lru(cptr->right);
    } else {
        cptr->left = pop_lru(cptr->left);
    }
    return cptr;
}

void move_to_tail(struct Cache *cptr) {
    if (cptr != ctail) {
        if (chead == cptr) {
            chead = chead->next;
            chead->prev = NULL;
        } else {
            cptr->prev->next = cptr->next;
            if (cptr->next) cptr->next->prev = cptr->prev;
        }
        cptr->prev = ctail;
        ctail->next = cptr;
        ctail = cptr;
        ctail->next = NULL;
    }
}

struct Cache * insert_cache(struct Cache *cptr, int track, int sector, char *buf) {
    if (cptr == NULL) {
        return create_cache(track, sector, buf);
    } else if (less(cptr->track, cptr->sector, track, sector)) {
        cptr->right = insert_cache(cptr->right, track, sector, buf);
    } else if (less(track, sector, cptr->track, cptr->sector)) {
        cptr->left = insert_cache(cptr->left, track, sector, buf);
    } else {
        // update cache
        memcpy(cptr->data, buf, FS3_SECTOR_SIZE);
        // move to tail (recent)
        move_to_tail(cptr);
    }
    return cptr;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_put_cache
// Description  : Put an element in the cache
//
// Inputs       : trk - the track number of the sector to put in cache
//                sct - the sector number of the sector to put in cache
// Outputs      : 0 if inserted, -1 if not inserted

int fs3_put_cache(FS3TrackIndex trk, FS3SectorIndex sct, void *buf) {
    insert_count++;
    croot = insert_cache(croot, trk, sct, buf);
    if (cache_size > cache_capacity) {
        croot = pop_lru(croot);
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_get_cache
// Description  : Get an element from the cache (
//
// Inputs       : trk - the track number of the sector to find
//                sct - the sector number of the sector to find
// Outputs      : returns NULL if not found or failed, pointer to buffer if found

void * fs3_get_cache(FS3TrackIndex trk, FS3SectorIndex sct)  {
    get_count++;
    struct Cache *cptr = croot;
    while (cptr) {
        if (less(cptr->track, cptr->sector, trk, sct)) {
            cptr = cptr->right;
        } else if (less(trk, sct, cptr->track, cptr->sector)) {
            cptr = cptr->left;
        } else {
            hit_count++;
            move_to_tail(cptr);
            return cptr->data;
        }
    }
    miss_count++;
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_log_cache_metrics
// Description  : Log the metrics for the cache 
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int fs3_log_cache_metrics(void) {
    logMessage(LOG_OUTPUT_LEVEL, "** FS3 cache Metrics **");
    logMessage(LOG_OUTPUT_LEVEL, "Cache inserts    [%9d]", insert_count);
    logMessage(LOG_OUTPUT_LEVEL, "Cache gets       [%9d]", get_count);
    logMessage(LOG_OUTPUT_LEVEL, "Cache hits       [%9d]", hit_count);
    logMessage(LOG_OUTPUT_LEVEL, "Cache misses     [%9d]", miss_count);
    logMessage(LOG_OUTPUT_LEVEL, "Cache hit ratio  [%%%5.2f]", 
               100.0 * hit_count / get_count);
    return(0);
}

