////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_netowork.c
//  Description    : This is the network implementation for the FS3 system.

//
//  Author         : Patrick McDaniel
//  Last Modified  : Thu 16 Sep 2021 03:04:04 PM EDT
//

// Includes
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cmpsc311_log.h>

// Project Includes
#include <fs3_network.h>
#include <fs3_controller.h>
#include <fs3_driver.h>
#include <cmpsc311_util.h>

//
//  Global data
unsigned char     *fs3_network_address = NULL; // Address of FS3 server
unsigned short     fs3_network_port = 0;       // Port of FS3 serve

int socket_fd;
struct sockaddr_in caddr;

//
// Network functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : network_fs3_syscall
// Description  : Perform a system call over the network
//
// Inputs       : cmd - the command block to send
//                ret - the returned command block
//                buf - the buffer to place received data in
// Outputs      : 0 if successful, -1 if failure

int network_fs3_syscall(FS3CmdBlk cmd, FS3CmdBlk *ret, void *buf)
{
    int opcode;
    deconstruct_cmdBlock(cmd, &opcode, NULL, NULL, NULL);

    // connect if mount requested
    if (opcode == FS3_OP_MOUNT) {
        if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            return -1;
        }

        caddr.sin_family = AF_INET;
        caddr.sin_port = htons(fs3_network_port? fs3_network_port : FS3_DEFAULT_PORT);

        if (connect(socket_fd, (struct sockaddr*)&caddr, sizeof(caddr)) == -1) {
            close(socket_fd);
            return -1;
        }
    }

    // write cmd
    FS3CmdBlk network_cmd = htonll64(cmd);
    if (write(socket_fd, &network_cmd, sizeof(FS3CmdBlk)) != sizeof(FS3CmdBlk)) {
        close(socket_fd);
        return -1;
    }

    // write buffer
    if (opcode == FS3_OP_WRSECT) {
        if (write(socket_fd, buf, FS3_SECTOR_SIZE) != FS3_SECTOR_SIZE) {
            close(socket_fd);
            return -1;
        }
    }

    // read cmd
    if (read(socket_fd, &network_cmd, sizeof(FS3CmdBlk)) != sizeof(FS3CmdBlk)) {
        close(socket_fd);
        return -1;
    }
    *ret = ntohll64(network_cmd);

    // read buffer
    if (opcode == FS3_OP_RDSECT) {
        if (read(socket_fd, buf, FS3_SECTOR_SIZE) != FS3_SECTOR_SIZE) {
            close(socket_fd);
            return -1;
        }
    }

    // disconnect if unmount requested
    if (opcode == FS3_OP_UMOUNT) {
        close(socket_fd);
    }

    return 0;
}

