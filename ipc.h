/* =============================================================================
* Filename : ipc.h
* Summary  : Struct definitions and external interfaces of processes and 
*			 communication between them. Mainly include channel mechanism, 
*			 synchronization mechanism and shared memory.
* Compiler : gcc
*
* Version  : 
* Update   : 
* Date     : 
* Author   : 
* Org      : 
*
* History  :
*
* ============================================================================*/

#ifndef __IPC_H__
#define __IPC_H__

/*==============================================================================
* Header files
*=============================================================================*/
#include <sys/mman.h>
#include <unistd.h>
#include "data.h"

/*==============================================================================
* Macro Definition
*=============================================================================*/
#define WORKER_NUM_MAX		10		/* worker process max num 	*/
#define DEVICE_NUM_MAX		5000	/* device max num 			*/

#define CHANNEL_CMD_OPEN  	1		/* open a new channel */
#define CHANNEL_CMD_DEV		2		/* device login */

/*==============================================================================
* Struct definitions
*=============================================================================*/
/* process struct */
typedef struct _process_info {
	pid_t		pid;		/* process id		*/
	int32 	channel[2]; /* channel for IPC	*/
} PROCESS_INFO;

/* shared memory struct */
typedef struct _share_mem {
	uint8*	addr;		/* shared memory start address  */
	uint32	size;		/* shared memory size 			*/
} SHARE_MEM;

/* channel communicate unit */
typedef struct _channel_msg {
	 uint32		cmd;		/* specific command 	 */
	 pid_t		pid;		/* target process id 	 */
	 uint32		idx;		/* target index in array */
	 int32		fd; 		/* target socket fd 	 */
} CHANNEL_MSG;

/*==============================================================================
* Global variable declaration
*=============================================================================*/
extern PROCESS_INFO	processes[]; 	/* worker processes array */ 
extern uint32		process_idx; 	/* current process index in array */
extern SHARE_MEM	host_shm;		/* shared memory */
extern int32 		host_mtx;		/* mutex of shared memory */

/*==============================================================================
* Function declaration
*=============================================================================*/
extern int8 Channel_create(int32 fd[]);
extern int8 Channel_write(int32* fd, CHANNEL_MSG* ch);
extern int8 Channel_read(int32* fd, CHANNEL_MSG* ch);
extern void Channel_pass(CHANNEL_MSG* ch, uint32 last);
extern void Channel_recv_cb(struct ev_loop* lp, ev_io* wt, int ev);
extern int8 Share_mem_alloc(SHARE_MEM* shm);
extern int8 Share_mem_free(SHARE_MEM* shm);
extern int8 Share_mtx_create(int32* mtx);
extern int8 Share_mtx_destroy(int32* mtx);
extern int8 Share_mtx_lock(int32* mtx);
extern int8 Share_mtx_unlock(int32* mtx);

#endif


