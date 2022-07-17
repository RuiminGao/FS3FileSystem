////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the FS3 storage system.
//
//   Author        : Ruimin Gao
//   Last Modified : July 16, 2022
//

// Includes
#include <stdlib.h>
#include <string.h>
#include <cmpsc311_log.h>

// Project Includes
#include <fs3_driver.h>
#include <fs3_driver_pi.h>
#include <fs3_cache.h>

//
// Defines
#define SECTOR_INDEX_NUMBER(x) ((int)(x/FS3_SECTOR_SIZE))

//
// Static Global Variables
int mounted = 0;
int next_fd = 0;
int next_track = 0;
int next_sector = 0;
int on_track = FS3_MAX_TRACKS;
struct File *fhead = NULL;
struct File *ftail = NULL;

//
// Implementation

int syscall(int opcode, int sector, int track, int ret, char *buf) {
	FS3CmdBlk cmd = construct_cmdBlock(opcode, sector, track, ret);
	FS3CmdBlk retBlk;
	network_fs3_syscall(cmd, &ret, buf);
	deconstruct_cmdBlock(retBlk, NULL, NULL, NULL, &ret);
	return ret == 0;
}

FS3CmdBlk construct_cmdBlock(int opcode, int sector, int track, int ret) {
	return (FS3CmdBlk) opcode << 60 | (FS3CmdBlk) sector << 44 |
		   (FS3CmdBlk) track << 12 	| (FS3CmdBlk) ret << 11;
}

void deconstruct_cmdBlock(FS3CmdBlk cmdBlock, int *opcode, int *sector, int *track, int *ret) {
	if (opcode) *opcode = cmdBlock >> 60 & 0xF;
	if (sector) *sector = cmdBlock >> 44 & 0xFF;
	if (track) *track = cmdBlock >> 12 & 0xFFFF;
	if (ret) *ret = cmdBlock >> 11 & 1;
}

struct File * get_file_by_path(char *path) {
	struct File *fptr = fhead;
	while (fptr) {
		if (!strcmp(fptr->path, path)) break;
		fptr = fptr->next;
	}
	return fptr;
}

struct File * get_file_by_fd(int16_t fd) {
	struct File *fptr = fhead;
	while (fptr) {
		if (fptr->fd == fd) break;
		fptr = fptr->next;
	}
	return fptr;
}

struct File * create_file(char *path) {
	struct File *fptr = (struct File *) malloc(sizeof(struct File));
	fptr->path = (char *) malloc(strlen(path) + 1);
	strcpy(fptr->path, path);
	fptr->fd = next_fd++;
	fptr->is_open = 1;
	fptr->size = 0;
	fptr->loc = 0;
	fptr->bhead = create_blk();
	fptr->next = NULL;

	if (!fhead) {
		fhead = fptr;
	} else {
		ftail->next = fptr;
	}
	return ftail = fptr;
}

struct Block *create_blk() {
	struct Block *bptr = (struct Block *) malloc(sizeof(struct Block));
	bptr->track = next_track;
	bptr->sector = next_sector;
	bptr->next = NULL;

	if (++next_sector == FS3_TRACK_SIZE) {
		next_track++;
		next_sector = 0;
	}

	return bptr;
}

struct Block * next_blk(struct Block *bptr) {
	if (!bptr->next) {
		bptr->next = create_blk();
	}
	return bptr->next;
}

void delete_files() {
	while (fhead) {
		while (fhead->bhead) {
			struct Block *bptr = fhead->bhead->next;
			free(fhead->bhead);
			fhead->bhead = bptr;
		}
		struct File *next = fhead->next;
		free(fhead->path);
		free(fhead);
		fhead = next;
	}
}

void fix_track(int track) {
	if (track != on_track) {
		char *buf = (char *) malloc(FS3_SECTOR_SIZE);
		syscall(FS3_OP_TSEEK, 0, track, 0, buf);
		free(buf);
		on_track = track;
	}
}

void read_from_sector(int track, int sector, char *buf) {
	char *read_cache = fs3_get_cache(track, sector);
	if (read_cache) {
		memcpy(buf, read_cache, FS3_SECTOR_SIZE);
	} else {
		fix_track(track);
		syscall(FS3_OP_RDSECT, sector, 0, 0, buf);
	}
}

void write_to_sector(int track, int sector, char *buf) {
	fix_track(track);
	syscall(FS3_OP_WRSECT, sector, 0, 0, buf);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_mount_disk
// Description  : FS3 interface, mount/initialize filesystem
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_mount_disk(void) {
	syscall(FS3_OP_MOUNT, 0, 0, 0, NULL);
	mounted = 1;
	next_fd = 0;
	next_track = 0;
	next_sector = 0;
	on_track = FS3_MAX_TRACKS;
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_unmount_disk
// Description  : FS3 interface, unmount the disk, close all files
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_unmount_disk(void) {
	if (!mounted) return -1;
	syscall(FS3_OP_UMOUNT, 0, 0, 0, NULL);
	delete_files();
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle if successful, -1 if failure

int16_t fs3_open(char *path) {
	struct File * fptr = get_file_by_path(path);
	if (fptr) {
		fptr->is_open = 1;
		return fptr->fd;
	} else {
		return create_file(path)->fd;
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
// Outputs      : 0 if successful, -1 if failure

int16_t fs3_close(int16_t fd) {
	struct File * fptr = get_file_by_fd(fd);
	if (!fptr || !fptr->is_open) return -1;
	fptr->is_open = 0;
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_read
// Description  : Reads "count" bytes from the file handle "fh" into the 
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure

int32_t fs3_read(int16_t fd, void *buf, int32_t count) {
	struct File * fptr = get_file_by_fd(fd);
	if (!fptr || !fptr->is_open) return -1;
	if (count == 0 || fptr->loc == fptr->size) return 0;
	if (count > fptr->size - fptr->loc) {
		count = fptr->size - fptr->loc;
	}

	// find current block
	struct Block *bptr = fptr->bhead;
	for (int i = 0; i < fptr->loc / FS3_SECTOR_SIZE; ++i) {
		bptr = bptr->next;
	}

	int read = 0;
	char read_buf[FS3_SECTOR_SIZE] = {0};
	if (fptr->loc % FS3_SECTOR_SIZE != 0) {
		// initially partial read
		read_from_sector(bptr->track, bptr->sector, read_buf);
		fs3_put_cache(bptr->track, bptr->sector, read_buf);
		read = count < FS3_SECTOR_SIZE - fptr->loc % FS3_SECTOR_SIZE? 
			   count : FS3_SECTOR_SIZE - fptr->loc % FS3_SECTOR_SIZE;
		memcpy(buf, read_buf + fptr->loc % FS3_SECTOR_SIZE, read);
		bptr = bptr->next;
	}

	for (; read + FS3_SECTOR_SIZE <= count; read += FS3_SECTOR_SIZE) {
		read_from_sector(bptr->track, bptr->sector, read_buf);
		fs3_put_cache(bptr->track, bptr->sector, read_buf);
		memcpy(buf + read, read_buf, FS3_SECTOR_SIZE);
		bptr = bptr->next;
	}

	if (read != count) {
		read_from_sector(bptr->track, bptr->sector, read_buf);
		fs3_put_cache(bptr->track, bptr->sector, read_buf);
		memcpy(buf + read, read_buf, count - read);
	}

	fptr->loc += count;
	return count;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_write
// Description  : Writes "count" bytes to the file handle "fh" from the 
//                buffer  "buf"
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written if successful, -1 if failure

int32_t fs3_write(int16_t fd, void *buf, int32_t count) {
	struct File * fptr = get_file_by_fd(fd);
	if (!fptr || !fptr->is_open) return -1;

	// find current block
	struct Block *bptr = fptr->bhead;
	for (int i = 0; i < fptr->loc / FS3_SECTOR_SIZE; ++i) {
		bptr = next_blk(bptr);
	}

	int written = 0;
	char write_buf[FS3_SECTOR_SIZE] = {0};
	if (fptr->loc % FS3_SECTOR_SIZE != 0) {
		// initially partial read
		read_from_sector(bptr->track, bptr->sector, write_buf);
		written = count < FS3_SECTOR_SIZE - fptr->loc % FS3_SECTOR_SIZE? 
				  count : FS3_SECTOR_SIZE - fptr->loc % FS3_SECTOR_SIZE;
		memcpy(write_buf + fptr->loc % FS3_SECTOR_SIZE, buf, written);
		fs3_put_cache(bptr->track, bptr->sector, write_buf);
		write_to_sector(bptr->track, bptr->sector, write_buf);
		bptr = next_blk(bptr);
	}

	for (; written + FS3_SECTOR_SIZE <= count; written += FS3_SECTOR_SIZE) {
		memcpy(write_buf, buf + written, FS3_SECTOR_SIZE);
		fs3_put_cache(bptr->track, bptr->sector, write_buf);
		write_to_sector(bptr->track, bptr->sector, write_buf);
		bptr = next_blk(bptr);
	}

	if (written != count) {
		if (fptr->loc + count < fptr->size) {
			read_from_sector(bptr->track, bptr->sector, write_buf);
		}
		memcpy(write_buf, buf + written, count - written);
		fs3_put_cache(bptr->track, bptr->sector, write_buf);
		write_to_sector(bptr->track, bptr->sector, write_buf);
	}

	fptr->loc += count;
	if (fptr->size < fptr->loc) {
		fptr->size = fptr->loc;
	}
	return count;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_seek
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - offset of file in relation to beginning of file
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_seek(int16_t fd, uint32_t loc) {
	struct File * fptr = get_file_by_fd(fd);
	if (!fptr || !fptr->is_open || loc < 0 || loc > fptr->size) return -1;
	fptr->loc = loc;
	return 0;
}
