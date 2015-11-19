/* =============================================================================
* Filename : master.c
* Summary  : Start master process to doing main work. Contain with starting 
*			 worker processes and monitoring the exit status of them. The child 
*			 process is the one who actually do data handling.
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
#include <sys/types.h>
#include <sys/wait.h>
#include "db.h"
#include "ipc.h"
#include "list.h"
#include "server.h"
#include "upgrade.h"

/*==============================================================================
* Interface declarations
*=============================================================================*/
static void Worker_start_cb(struct ev_loop* lp, ev_timer *wt, int ev);
static void Worker_start(uint32 index);
static void Worker_init();
static void Worker_main();
static void Worker_run ();
static void Worker_exit_cb(struct ev_loop* lp, ev_child* wt, int ev);
static void Loop_wakeup_cb(struct ev_loop* lp, ev_async* wt, int ev);
static void Master_stop_cb(struct ev_loop* lp, ev_timer *wt, int ev);

/*==============================================================================
* Global variable definitions
*=============================================================================*/
/* shared global variable */
struct ev_loop*	host_loop;		/* main work loop */
pid_t			current_pid;	/* current process pid */
int32			host_fd;		/* socket fd used to listened */
uint32			process_last;	/* last worker process index */

/* master process private variable */
ev_timer		host_t_wt;		/* timer to start worker process */
ev_child		exit_c_wt;		/* worker process exit watcher */
pid_t			master_pid;		/* master process pid */

/* workers process private variable */
ev_io			host_io_wt;		/* socket fd watcher */
ev_io			channel_io_wt;	/* channel watcher */
ev_async		async_wt;		/* async watcher in worker process */



int main(int argc, char* argv[])
{
	int8		err;
	uint32	i;

	i = 0;
	err = EXEC_SUCS;
	host_fd   = 0;
	host_loop = NULL;
	master_pid  = 0;
	current_pid = 0;
	process_idx  = 0;
	process_last = 0;
	memset(&host_shm, 	  0x00, sizeof(SHARE_MEM));
	memset(&processes[0], 0x00, sizeof(PROCESS_INFO) * WORKER_NUM_MAX);

	/* shielding signal SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	/* get the master pid */
	master_pid  = getpid();
	current_pid = master_pid;

	/* start the log module */
	err = Log_start();

	/* malloc shared memory */
	if (!err) {
		host_shm.size = sizeof(int16)  + UPG_VER_LEN + 1 + UPG_PATH_LEN + 1 +
						sizeof(uint16) + sizeof(uint8) + sizeof(uint32) + 
						UPG_DATA_SIZE 	 + sizeof(uint32) + 
						sizeof(DEVICE_ROW) * DEVICE_NUM_MAX;
		
		err = Share_mem_alloc(&host_shm);
	}

	/* create shared memory mutex */
	if (!err) {
		err = Share_mtx_create(&host_mtx);
	}
	
	/* initialize the host listen socket */
	if (!err) {
		err = Socket_init(&host_fd);
	}

	/* listen on the host socket */
	if (!err) {
		err = Socket_start(&host_fd);
	}

	/* create a new outer loop */
	if (!err) {
		host_loop = ev_default_loop(0);
		if (host_loop == NULL) {
			log_fatal(log_cat, "loop: outer create failed");
			err = EXEC_FAIL;
		} else {
			log_debug(log_cat, "loop: outer create success");
		}
	}

	/* initialize watchers used in master process */
	if (!err) {
		ev_timer_init(&host_t_wt, Worker_start_cb, timer_array[TID_WORKER_START].t_val, 0);
		ev_timer_start(host_loop, &host_t_wt);
		ev_child_init(&exit_c_wt, Worker_exit_cb, 0, 0);
	   	ev_child_start(host_loop, &exit_c_wt);
	}

	/* start the outer loop */
	if (!err) {
		err = ev_run(host_loop, 0);
		log_debug(log_cat, "loop: outer exit [return=%d]", err);
	}

	/* free handle, both master and worker process can arrive here */

	/* stop watchers used in master process */
	if (current_pid == master_pid) {
		ev_timer_stop(host_loop, &host_t_wt);
		ev_child_stop(host_loop, &exit_c_wt);
		Share_mem_free(&host_shm);
	/* stop watchers used in worker process */
	} else {
		ev_io_stop(host_loop, &host_io_wt);
		ev_io_stop(host_loop, &channel_io_wt);
		ev_async_stop(host_loop, &async_wt);
	}

	/* close channel */
	for (i = 0; i < WORKER_NUM_MAX; i++) {
		processes[i].pid = 0;
		Socket_close(&(processes[i].channel[0]), "channel[0]");
		Socket_close(&(processes[i].channel[1]), "channel[1]");
	}

	/* close listen socket */
	Socket_close(&host_fd, "host_fd");

	/* destory the shared memory mutex */
	Share_mtx_destroy(&host_mtx);
	
	/* free log module */
	if (log_cat != NULL) {
		log_debug(log_cat, "zlog: ready to exit");
	}

	if (log_run) {
		printf("zlog: free success\n");
		Log_free();
	}

	return err;
}

/*==============================================================================
* Name	 :	void Worker_start_cb(struct ev_loop* lp, ev_timer *wt, int ev)
* Abstr	 :	Time to start worker processes
* Params :	struct ev_loop* lp : outer event loop
*			ev_timer* 		wt : timer watcher
*			int 			ev : specific event
* Return :	
* Modify :	
*=============================================================================*/
static void Worker_start_cb(struct ev_loop* lp, ev_timer *wt, int ev)
{
	while (process_last < WORKER_NUM_MAX) {
		
		/* start worker process in specific position */
		Worker_start(process_last);
		
		/* worker process exit the loop */
		if (current_pid != master_pid) {
			break;
		}
	}

	/*
	ev_timer_init (&host_t_wt, Master_stop_cb, 30., 0);
	ev_timer_start(host_loop, &host_t_wt);
	*/
}

/*==============================================================================
* Name	 :	void Master_stop_cb(struct ev_loop* lp, ev_timer *wt, int ev)
* Abstr	 :	Time to stop master processes
* Params :	struct ev_loop* lp : outer event loop
*			ev_timer* 		wt : timer watcher
*			int 			ev : specific event
* Return :	
* Modify :	
*=============================================================================*/
static void Master_stop_cb(struct ev_loop* lp, ev_timer *wt, int ev)
{
	ev_break(lp, EVBREAK_ALL);
}

/*==============================================================================
* Name	 :	void Worker_start(uint32 index)
* Abstr	 :	Start worker processes
* Params :	uint32 index : current process index in array
* Return :	
* Modify :	
*=============================================================================*/
static void Worker_start(uint32 index)
{
	int8			err;
	pid_t			pid;
	CHANNEL_MSG		ch;	/* channel message */
	
	err = EXEC_SUCS;
	pid = 0;
	memset(&ch, 0x00, sizeof(CHANNEL_MSG));
	
	/* create channel for IPC */
	err = Channel_create(processes[index].channel);

	/* set the slot for worker process */
	if (!err) {
		process_idx = index;
	}
	
	/* start worker process */
	if (!err) {

		/* fork worker process */
		pid = fork();

		/* fork failed */
		if (pid < 0) {
			log_debug(log_cat, "papa: fork failed");
			
		/* child process */
		} else if (pid == 0) {

			/* libev requirement */
			ev_loop_fork(host_loop);

			/* Clean resource used in master process */
			Worker_init();

			/* worker process main function */
			Worker_main();
			
		/* master process */
		} else {

			/* libev requirement */
			ev_loop_fork(host_loop);

			/* get worker pid */
			processes[index].pid = pid;

			/* process counter add 1 */
			if (index == process_last) {
				process_last++;
			}

			/* set command of open channel */
			ch.idx = process_idx;
			ch.pid = processes[process_idx].pid;
			ch.fd  = processes[process_idx].channel[0];
			ch.cmd = CHANNEL_CMD_OPEN;

			/* notice other worker processes */
	        Channel_pass(&ch, process_last);
		}
	}
	
	sleep(2);
}

/*==============================================================================
* Name	 :	void Worker_init()
* Abstr	 :	Initialize the worker process
* Params :	
* Return :	
* Modify :	
*=============================================================================*/
static void Worker_init()
{
	uint32 	i;
	uint8*	addr;

	i 	 = 0;
	addr = NULL;

	/* shielding signal SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	/* load shared memory */
	addr = host_shm.addr;

	device_last = (uint32*)addr;
	addr += sizeof(uint32);

	device_tb = (DEVICE_ROW*)addr;

	/* get current worker pid */
	current_pid = getpid();
	log_debug(log_cat, "worker:	create success [p_idx=%u] [pid=%d]", process_idx, current_pid);

	/* stop watchers which are used in parent process */
	ev_timer_stop(host_loop, &host_t_wt);
	ev_child_stop(host_loop, &exit_c_wt);

	/* close channel */
	for (i = 0; i < process_last; i++) {
		if ((process_idx == i) ||
			(processes[i].pid <= 0) ||
			(processes[i].channel[1] <= 0)) {
			continue;
		}
		Socket_close(&(processes[i].channel[1]), "channel[1]");
	}
	
	Socket_close(&(processes[process_idx].channel[0]), "channel[0]");

	/* free the outer ev loop */
	ev_loop_destroy(host_loop);
}

/*==============================================================================
* Name	 :	void Worker_main()
* Abstr	 :	Worker process main function
* Params :	
* Return :	
* Modify :	
*=============================================================================*/
static void Worker_main()
{
	int8		err;

	err = EXEC_SUCS;
	
	/* start db connection */
	err = DB_conn_start(&db_url, &db_pool, &db_conn);

	/* create a new inner loop */
	if (!err) {
		host_loop = ev_default_loop(EVBACKEND_EPOLL | EVFLAG_NOENV);
		if (host_loop == NULL) {
			log_fatal(log_cat, "loop: inner create failed");
			err = EXEC_FAIL;
		} else {
			log_debug(log_cat, "loop: inner create success");
		}
	}

	/* start the service */
	if (!err) {
		Worker_run();
	}

	/* free db resources */
	DB_conn_stop(&db_url, &db_pool, &db_conn);

	/* suspend 1s */
	sleep(3);
}

/*==============================================================================
* Name	 :	void Worker_main()
* Abstr	 :	Run worker processes main handling loop
* Params :	
* Return :	
* Modify :	
*=============================================================================*/
static void Worker_run()
{	
	int8			err;
	DEVICE_INFO*	device;		/* temp device */

	err = EXEC_SUCS;
	device = NULL;
	
	/* creat new device list */
	device_list = List_new();

	/* start channel watcher */
	ev_io_init(&channel_io_wt, Channel_recv_cb, processes[process_idx].channel[1], EV_READ);
	ev_io_start(host_loop, &channel_io_wt);

	/* start host socket fd watcher */
	ev_io_init(&host_io_wt, Socket_accept_cb, host_fd, EV_READ);
	ev_io_start(host_loop, &host_io_wt);

	/* start async watcher */
	ev_async_init(&async_wt, Loop_wakeup_cb);
	ev_async_start(host_loop, &async_wt);

	/* start the inner loop */
	err = ev_run(host_loop, 0);
	log_debug(log_cat, "loop: inner exit [return=%d]", err);

	/* free devices */
	while ((device = (DEVICE_INFO*)List_node_pop(device_list))) {
		Device_shutup(device);
		log_debug(log_cat, "%s: delete success", &(device->key_id[0]));
		safe_free((void*)device);
		device = NULL;
	}

	/* free list */
	List_free(device_list);
}

/*==============================================================================
* Name	 :	void Worker_exit_cb(struct ev_loop* lp, ev_child* wt, int ev)
* Abstr	 :	Handling of worker process exit
* Params :	struct ev_loop* lp : outer ev loop
*			ev_child* 		wt : child process watcher
*			int 			ev : specific event
* Return :	
* Modify :	
*=============================================================================*/
static void Worker_exit_cb(struct ev_loop* lp, ev_child* wt, int ev)
{
	uint32	i;
	uint32	idx;	/* position of the exited process */

	i   = 0;
	idx = 0;

	/* search the position of the exited process */
	for (i = 0; i < process_last; i++) {
		if (processes[i].pid == wt->rpid) {
			idx = i;
			break;
		}
	}

	/* unknown process */
	if ((idx == 0) && (i == process_last)) {
		log_debug(log_cat, "exit: [pid=%d] [no find]", wt->rpid);
	
	/* find the exited process */
	} else {
		log_debug(log_cat, "exit: [pid=%d] [idx=%u]", wt->rpid, idx);

		processes[idx].pid = 0;
		Socket_close(&(processes[idx].channel[0]), "channel[0]");
		Socket_close(&(processes[idx].channel[1]), "channel[1]");

		Worker_start(idx);
	}
}

/*==============================================================================
* Name	 :	void Loop_wakeup_cb(struct ev_loop* lp, ev_async* wt, int ev)
* Abstr	 :	Wakeup the event loop
* Params :	struct ev_loop* lp : outer ev loop
*			ev_async* 		wt : async watcher
*			int 			ev : specific event
* Return :	
* Modify :	
*=============================================================================*/
static void Loop_wakeup_cb(struct ev_loop* lp, ev_async* wt, int ev)
{
      // just used for the side effects 
}



