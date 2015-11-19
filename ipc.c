/* =============================================================================
* Filename : ipc.c
* Summary  : Handling of IPC mechanism.
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

/*==============================================================================
* Header files
*=============================================================================*/
#include "ipc.h"

/*==============================================================================
* Interface declarations
*=============================================================================*/
extern DEVICE_ROW*		device_tb;
extern struct ev_loop* 	host_loop;
extern ev_io 			channel_io_wt;	/* channel watcher */

extern void Device_delete(DEVICE_INFO* device);
extern DEVICE_INFO* Device_search(cint8* dev_id);

extern int8 Socket_close(int32* fd, cint8* nm);
extern int8 Socket_set_nonblock(int32* fd);

/*==============================================================================
* Global variable definition
*=============================================================================*/
PROCESS_INFO 	processes[WORKER_NUM_MAX]; /* worker processes array */	
SHARE_MEM		host_shm;	/* shared memory */
uint32			process_idx;/* current process index in array */
int32			host_mtx;	/* mutex of shared memory */

#if 0
#endif

/*==============================================================================
* Name	 :	int8 Channel_create(int32 fd[])
* Abstr	 :	Create socket pair for ipc.
* Params :	int32 fd[] : socket pair array
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Channel_create(int32 fd[])
{
	int8		err;
	
	err = EXEC_SUCS;

	/* create new socketpair */
	err = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
	if (!err) {
		log_debug(log_cat, "ch: create success [%d %d]", fd[0], fd[1]);
	} else {
		log_fatal(log_cat, "ch: create failed [%s]", strerror(errno));
	}

	/* set channel[0] fd nonblock */
	if (!err) {
		err = Socket_set_nonblock(&fd[0]);
	}

	/* set channel[1] fd nonblock */
	if (!err) {
		err = Socket_set_nonblock(&fd[1]);
	}

	/* error occur, close them */
	if (err) {
		Socket_close(&fd[0], "fd[0]");
		Socket_close(&fd[1], "fd[1]");
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Channel_write(int32* fd, CHANNEL_MSG* ch)
* Abstr	 :	Send command message through the socket pair.
* Params :	int32* 		fd	:	socket pair fd
*			CHANNEL_MSG* 	ch	:	command message
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Channel_write(int32* fd, CHANNEL_MSG* ch)
{
	int8			err;
	ssize_t			size;
	struct iovec	iov[1];
    struct msghdr	msg;
	
	union {
        struct cmsghdr  cm;
        char            space[CMSG_SPACE(sizeof(int32))];
	} cmsg;

	err  = EXEC_SUCS;
	size = 0;

	/* general command, no ancillary data */
	if (ch->fd <= 0) {
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		
	/* open channel, need ancillary data */
	} else {
		msg.msg_control = (caddr_t)&cmsg;
        msg.msg_controllen = sizeof(cmsg);

		memset(&cmsg, 0x00, sizeof(cmsg));
		cmsg.cm.cmsg_len   = CMSG_LEN(sizeof(int));
		cmsg.cm.cmsg_level = SOL_SOCKET;
		cmsg.cm.cmsg_type  = SCM_RIGHTS;
		memcpy(CMSG_DATA(&cmsg.cm), &ch->fd, sizeof(int32));
	}

	msg.msg_flags = 0;

	iov[0].iov_base = (cint8*)ch;
	iov[0].iov_len  = sizeof(CHANNEL_MSG);

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	/* send message */
	size = sendmsg(*fd, &msg, 0);
	if (size < 0) {
		log_fatal(log_cat, "[s=%u].ch[0]: [fd=%d] sendmsg failed [%s]", 
				  process_idx, *fd, strerror(errno));
		err = EXEC_FAIL;
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Channel_read(int32* fd, CHANNEL_MSG* ch)
* Abstr	 :	Recv command message from the socket pair.
* Params :	int32* 		fd	:	socket pair fd
*			CHANNEL_MSG* 	ch	:	command message
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Channel_read(int32* fd, CHANNEL_MSG* ch)
{
    ssize_t             size;
    int8 				err;
    struct iovec        iov[1];
    struct msghdr       msg;
	
    union {
        struct cmsghdr  cm;
        char            space[CMSG_SPACE(sizeof(int32))];
    } cmsg;

	err  = EXEC_SUCS;
	size = 0;

    iov[0].iov_base = (cint8*)ch;
    iov[0].iov_len  = sizeof(CHANNEL_MSG);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    msg.msg_control = (caddr_t)&cmsg;
    msg.msg_controllen = sizeof(cmsg);

    size = recvmsg(*fd, &msg, 0);

	switch (size) {
	case -1:	/* recvmsg failed */
		log_fatal(log_cat, "[s=%u].ch[1]: [fd=%d] recv failed [%s]", 
				  process_idx, *fd, strerror(errno));
		err = EXEC_FAIL;
		break;
	case 0:		/* oppsite close */
		log_fatal(log_cat, "[s=%u].ch[1]: [fd=%d] recv failed [closed]", 
				  process_idx, *fd);
		err = EXEC_FAIL;
		break;
	default:	/* recvmsg success */
		if ((size_t)size < sizeof(CHANNEL_MSG)) {
			log_fatal(log_cat, "[s=%u].ch[1]: [fd=%d] recv failed [loss]", 
					  process_idx, *fd);
			err = EXEC_FAIL;
		}
	}

	/* check the channel message */
	if (!err) {
		if (ch->cmd == CHANNEL_CMD_OPEN) {
			if (cmsg.cm.cmsg_len   <  (socklen_t)CMSG_LEN(sizeof(int32)) ||
				cmsg.cm.cmsg_level != SOL_SOCKET ||
				cmsg.cm.cmsg_type  != SCM_RIGHTS) {
				log_fatal(log_cat, "[s=%u].ch[1]: [fd=%d] check failed [invalid]", 
						  process_idx, *fd);
				err = EXEC_FAIL;
			} else {
				memcpy(&ch->fd, CMSG_DATA(&cmsg.cm), sizeof(int32));
			}
		}
	}

	if (!err) {
		if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) {
       		log_warn(log_cat, "[s=%u].ch[1]: [fd=%d] check warn [truncated]");
		}
	}

	return err;
}

/*==============================================================================
* Name	 :	void Channel_pass(CHANNEL_MSG* ch, uint32 last)
* Abstr	 :	Broadcast command message to other process.
* Params :	CHANNEL_MSG* 	ch	 :	command message
*			uint32 		last :	last process index / alive process num
* Return :	
* Modify :	
*=============================================================================*/
void Channel_pass(CHANNEL_MSG* ch, uint32 last)
{
    uint32 	i;

    for (i = 0; i < last; i++) {

		/* ignore self, exited, can't communicate */
        if ((process_idx == i)		 || 
        	(processes[i].pid <= 0)  || 
        	(processes[i].channel[0] <= 0)) {
            continue;
        }

		usleep(50000);

		/* send channel message */
        Channel_write(&(processes[i].channel[0]), ch);
    }
}

/*==============================================================================
* Name	 :	void Channel_recv_cb(struct ev_loop* lp, ev_io* wt, int ev)
* Abstr	 :	Handle of data receive on socket pair
* Params :	struct ev_loop* lp : inner event loop
*			ev_io* 			wt : socket pair io watcher
*			int 			ev : specific event
* Return :	
* Modify :	
*=============================================================================*/
void Channel_recv_cb(struct ev_loop* lp, ev_io* wt, int ev)
{
	int8			err;
	CHANNEL_MSG		ch;		/* channel communication unit */
	DEVICE_INFO*	old;	/* old device in list */

	err = EXEC_SUCS;
	old = NULL;
	memset(&ch, 0x00, sizeof(CHANNEL_MSG));

	/* recv channel message */
	err = Channel_read(&(processes[process_idx].channel[1]), &ch);
	if (!err) {
		switch (ch.cmd) {
			
		case CHANNEL_CMD_OPEN:/* open new channel */
			Socket_close(&(processes[ch.idx].channel[0]), "channel[0]");
			processes[ch.idx].pid = ch.pid;
			processes[ch.idx].channel[0] = ch.fd;
			log_debug(log_cat, "[s=%u].ch[1]: open channel [s=%u pid=%d fd=%d]", 
					  process_idx, ch.idx, ch.pid, ch.fd);
			break;
			
		case CHANNEL_CMD_DEV:/* close old connection */
			log_debug(log_cat, "[s=%u].ch[1]: device login [s=%u pid=%d]", 
					  process_idx, ch.idx, ch.pid);
			old = Device_search(&(device_tb[ch.idx].key_id[0]));
			if ((old != NULL) && (old->is_bind == TRUE)) {
				Device_delete(old);
			}
			break;
			
		default:/* unknown cmd */
			break;
		}
	} else {
		Socket_close(&(processes[process_idx].channel[1]), "channel[1]");
		ev_io_stop(host_loop, &channel_io_wt);
		ev_break(host_loop, EVBREAK_ALL);
	}
}

/*==============================================================================
* Name	 :	int8 Share_mem_alloc(SHARE_MEM* shm)
* Abstr	 :	Alloc shared memory
* Params :	SHARE_MEM* 	shm	:	pointer of shared memory and its size
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Share_mem_alloc(SHARE_MEM* shm)
{
	int8	err;

	err = EXEC_SUCS;
	
	shm->addr = (uint8*)mmap(NULL, shm->size, PROT_READ | PROT_WRITE, 
							   MAP_ANON | MAP_SHARED, -1, 0);
	if (shm->addr == MAP_FAILED) {
		log_fatal(log_cat, "shm : mmap failed [%s]", strerror(errno));
		shm->addr = NULL;
		err = EXEC_FAIL;
	} else {
		memset(shm->addr, 0x00, shm->size);
		log_debug(log_cat, "shm : mmap success [%dkb]", shm->size / 1024);
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Share_mem_free(SHARE_MEM* shm)
* Abstr	 :	Free shared memory
* Params :	SHARE_MEM* 	shm	:	pointer of shared memory and its size
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Share_mem_free(SHARE_MEM* shm)
{
	int8	err;

	err = EXEC_SUCS;

	if (shm->addr != NULL) {
		err = munmap((void*)shm->addr, shm->size);
		if (err) {
			log_fatal(log_cat, "shm : munmap failed [%s]", strerror(errno));
		} else {
			log_debug(log_cat, "shm : munmap success");
		}
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Share_mtx_create(int32* mtx)
* Abstr	 :	Create mutex of shared memory
* Params :	int32* 	mtx	:	mutex of shared memory
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Share_mtx_create(int32* mtx)
{
	int8	err;

	err  = EXEC_SUCS;
	*mtx = 0;

	*mtx = open("./shmtx", O_RDWR | O_CREAT, 0644);
	if (*mtx == -1) {
		log_fatal(log_cat, "shmtx: create failed [%s]", strerror(errno));
		err = EXEC_FAIL;
	} else {
		log_debug(log_cat, "shmtx: create success [mtx=%d]", *mtx);
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Share_mtx_destroy(int32* mtx)
* Abstr	 :	Destroy mutex of shared memory
* Params :	int32* 	mtx	:	mutex of shared memory
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Share_mtx_destroy(int32* mtx)
{
	int8		err;

	err = EXEC_SUCS;

	if (*mtx > 0) {
		err = close(*mtx);
		if (err) {
			log_fatal(log_cat, "shmtx: [mtx=%d] destroy failed [%s]", 
					  *mtx, strerror(errno));
			err = EXEC_FAIL;
		} else {
			log_debug(log_cat, "shmtx: [mtx=%d] destroy success", *mtx);
		}
	}

	*mtx = -1;

	return err;
}

/*==============================================================================
* Name	 :	int8 Share_mtx_lock(int32* mtx)
* Abstr	 :	Lock mutex of shared memory
* Params :	int32* 	mtx	:	mutex of shared memory
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Share_mtx_lock(int32* mtx)
{
	int8			err;
	struct flock  	fl;		/* info of mutex */
	
	memset(&fl, 0x00, sizeof(struct flock));

	fl.l_type   = F_WRLCK;
    fl.l_whence = SEEK_SET;

	err = fcntl(*mtx, F_SETLKW, &fl);
	if (err) {
		log_fatal(log_cat, "shmtx: [mtx=%d] lock failed [%s]", 
				  *mtx, strerror(errno));
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Share_mtx_unlock(int32* mtx)
* Abstr	 :	Unlock mutex of shared memory
* Params :	int32* 	mtx	:	mutex of shared memory
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Share_mtx_unlock(int32* mtx)
{
	int8			err;
	struct flock  	fl;		/* info of mutex */
	
	memset(&fl, 0x00, sizeof(struct flock));

	fl.l_type   = F_UNLCK;
    fl.l_whence = SEEK_SET;

	err = fcntl(*mtx, F_SETLK, &fl);
	if (err) {
		log_fatal(log_cat, "shmtx: [mtx=%d] unlock failed [%s]", 
				  *mtx, strerror(errno));
	}

	return err;
}

