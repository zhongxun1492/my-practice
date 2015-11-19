/* =============================================================================
* Filename : device.c
* Summary  : Handling of sockets which are connected with the device. 	 
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
#include "device.h"
#include <sys/time.h>

/*==============================================================================
* Interface declarations
*=============================================================================*/
extern struct ev_loop* 	host_loop;
extern ev_async			async_wt;
extern pid_t			current_pid;
extern ev_io			host_io_wt;	
extern int32			host_fd;

static void Device_switch_to_recv(CONN_INFO* conn);
static void Device_switch_to_stop(CONN_INFO* conn);
static void Device_recv_cb(struct ev_loop* lp, ev_io* wt, int ev);
static void Device_wait_cb(struct ev_loop* lp, ev_timer* wt, int ev);
static int8 Device_handle(CONN_INFO* conn);
static int8 Device_login (DEVICE_INFO* device);
static void Device_breath(DEVICE_INFO* device);
static void Device_logout(DEVICE_INFO* device);
static int8 Device_answer(CONN_INFO* conn);
static int8 Device_control(DEVICE_ROW* row, CONN_INFO* conn);
static int8 Device_init_acquire(DEVICE_INFO* device);
static int8 Device_past_clear(DEVICE_INFO* device);
static int8 Device_can_acquire(CONN_INFO* conn, int16 ver);
static void Device_login_trans(CONN_INFO* conn);
static int8 Device_online_stats(cint8* id, int8 offset);
static struct tm* Time_get(time_t offset_sec, TIME_INFO* target_time);

/*==============================================================================
* Global variable definition
*=============================================================================*/
LINK_LIST* 		device_list;		/* device list 	 */
DEVICE_ROW*		device_tb;			/* device table  */
uint32*			device_last;		/* device num 	 */

/* dummy acc off message */
static const uint8 accof_msg[ACCOFF_MSG_LEN] = 
{0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#if 0
#endif

/*==============================================================================
* Name	 :	void Device_create(int32* fd)
* Abstr	 :	Create a new device memory and initialize it. 
*			Change the socket status to waiting for data. 
*			Add the device into list.
* Params :	int32* 	fd: new socket fd
* Return :	
* Modify :	
*=============================================================================*/
void Device_create(int32* fd)
{
	DEVICE_INFO* 	device;		/* new device ptr */
	
	device = NULL;

	/* give the new device memory */
	device = (DEVICE_INFO*)safe_malloc(sizeof(DEVICE_INFO));

	/* initialize the new device */
	device->d_conn.fd  = *fd;
	device->d_conn.dev = device;

	/* let the socket switch to recv status */
	Device_switch_to_recv(&(device->d_conn));

	/* add to the device list */
	List_node_add(device_list, device);

	/* limited load */
	if ((ev_is_active(&host_io_wt)) && 
		(device_list->cur_num > LOAD_HARD_LIMIT)) {
		ev_io_stop(host_loop, &host_io_wt);
	}

	log_debug(log_cat, "device: creat success [fd=%d] [load=%u]", *fd, device_list->cur_num);
}

/*==============================================================================
* Name	 :	void Device_shutup(DEVICE_INFO* device)
* Abstr	 :	Stop the io watcher of connections with devices and db servers and 
*			then close the connections.
* Params :	DEVICE_INFO* device: device info
* Return :	
* Modify :	
*=============================================================================*/
void Device_shutup(DEVICE_INFO* device)
{
	uint8		i;		/* servant connections counter */
	CONN_INFO*	conn;	/* temp connection */

	i  = 0;
	conn = NULL;

	/* stop the master connection */
	conn = &(device->d_conn);
	ev_io_stop	 (host_loop, &(conn->io_wt));
	ev_timer_stop(host_loop, &(conn->t_wt ));
	Socket_shutdown(&(conn->fd), &(conn->name[0]));
	Socket_close   (&(conn->fd), &(conn->name[0]));

	/* stop the servant connecitons */
	for (i = 0; i < device->slv_num; i++) {
		conn = NULL;
		conn = &(device->s_conn[i]);
		ev_io_stop	 (host_loop, &(conn->io_wt));
		ev_timer_stop(host_loop, &(conn->t_wt ));
		Socket_shutdown(&(conn->fd), &(conn->name[0]));
		Socket_close   (&(conn->fd), &(conn->name[0]));
	}

	/* online num -1 */
	if (device->is_bind) {
		device_tb[device->tb_idx].ol_num--;
	}
	
	Device_online_stats(&(device->key_id[0]), -1);
}

/*==============================================================================
* Name	 :	void Device_delete(DEVICE_INFO* device)
* Abstr	 :	Delete the device from list.
* Params :	DEVICE_INFO* device: device info
* Return :	
* Modify :	
*=============================================================================*/
void Device_delete(DEVICE_INFO* device)
{
	Device_shutup(device);
	
	List_node_del(device_list, device);
	
	log_debug(log_cat, "%s: delete success", &(device->key_id[0]));
	
	safe_free((void*)device);
}

/*==============================================================================
* Name	 :	void Device_switch_to_recv(CONN_INFO* conn)
* Abstr	 :	Start io watcher and data waiting timer watcher		
* Params :	CONN_INFO* 	conn : connection info
* Return :	
* Modify :	
*=============================================================================*/
static void Device_switch_to_recv(CONN_INFO* conn)
{
	/* io wathcer switch to waiting for data */
	ev_io_init(&(conn->io_wt), Device_recv_cb, conn->fd, EV_READ);
	ev_io_start(host_loop, &(conn->io_wt));

	/* start wait data timer */
	ev_timer_init(&(conn->t_wt), Device_wait_cb, 0., timer_array[TID_WAIT_DATA].t_val);
	ev_timer_again(host_loop, &(conn->t_wt));

	/* notice the host loop */
	ev_async_send(host_loop, &async_wt);
}

/*==============================================================================
* Name	 :	void Device_switch_to_stop(CONN_INFO* conn)
* Abstr	 :	Stop the socket io watcher and then start the stopping timer watcher
* Params :	CONN_INFO* 	conn : connection info
* Return :	
* Modify :	
*=============================================================================*/
static void Device_switch_to_stop(CONN_INFO* conn)
{
	log_fatal(log_cat, "%s: catch exception", &(conn->name[0]));

	/* reset connection alive flag */
	conn->is_alive = FALSE;

	/* stop connection io watcher */
	ev_io_stop(host_loop, &(conn->io_wt));

	/* do stopping handle after 10s  */
	conn->t_wt.repeat = timer_array[TID_STOP_DELAY].t_val;
	ev_timer_again(host_loop, &(conn->t_wt));

	/* notice the host loop */
	ev_async_send(host_loop, &async_wt);
}

/*==============================================================================
* Name	 :	void Device_recv_cb(struct ev_loop* lp, ev_io* wt, int ev)
* Abstr	 :	Handle of data receive
* Params :	struct ev_loop* lp : inner event loop
*			ev_io* 			wt : socket io watcher
*			int 			ev : specific event
* Return :	
* Modify :	
*=============================================================================*/
static void Device_recv_cb(struct ev_loop* lp, ev_io* wt, int ev)
{
	int8			err;
	CONN_INFO*		conn;	/* connection with device */

	err  = EXEC_SUCS;
	conn = container_of(wt, CONN_INFO, io_wt);

	/* refresh the waiting timer */
	conn->t_wt.repeat = timer_array[TID_WAIT_DATA].t_val;
	ev_timer_again(host_loop, &(conn->t_wt));

	/* recv message */
	err = Msg_recv(&(conn->fd), &(conn->r_buf[0]), &(conn->name[0]));
	if (err == ERROR_DATA_LOSS) {
		return;
	}

	/* message check */
	if (!err) {
		err = Msg_check(&(conn->r_buf[0]), &(conn->name[0]));
		if (err) {
			return;
		}
	}

	/* message handle */
	if (!err) {
		err = Device_handle(conn);
	}

	/* error handle */
	if (err) {
		Device_switch_to_stop(conn);
	}
}

/*==============================================================================
* Name	 :	void Device_wait_cb(struct ev_loop* lp, ev_timer* wt, int ev)
* Abstr	 :	Handle of timeout
* Params :	struct ev_loop* lp : inner event loop
*			ev_timer* 		wt : timer watcher
*			int 			ev : specific event
* Return :	
* Modify :	
*=============================================================================*/
static void Device_wait_cb(struct ev_loop* lp, ev_timer* wt, int ev)
{	
	int8				err;
	CONN_INFO*			conn;	/* connection with device */
	DEVICE_INFO*		device;	/* corresponding device */
	
	err = EXEC_SUCS;
	conn = container_of(wt, CONN_INFO, t_wt);
	device = conn->dev;

	log_warn(log_cat, "%s: timeout [t_val=%.1f] [times=%d]", 
			 &(conn->name[0]), conn->t_wt.repeat, conn->times);

	/* long time no data arrive */
	if (abs(wt->repeat - timer_array[TID_WAIT_DATA].t_val) < 0.01) {
		
		/* send dummy acc off */
		log_debug(log_cat, "%s", &(device->key_id[0]));
		
		memset(&(conn->r_buf[0]), 0x00, 		   RECV_LEN_MAX);
		memcpy(&(conn->r_buf[0]), &(accof_msg[0]), ACCOFF_MSG_LEN);
		
		Device_logout(device);
		
	/* no answer arrive in expected time */
	} else if (abs(wt->repeat - timer_array[TID_WAIT_ANSWER].t_val) < 0.01) {
	
		if (conn->times < RETRY_CNT_MAX) {
			conn->times++;
			err = Msg_send(&(conn->fd), &(conn->s_buf[0]), &(conn->name[0]));
		} else {
			err = EXEC_FAIL;
		}

		if (err) {
			Device_switch_to_stop(conn);
		}
		
	/* time to stop the device */
	} else if (abs(wt->repeat - timer_array[TID_STOP_DELAY].t_val) < 0.01) {

		/* catch exception during upgrading */
		if (device->upg_sts != STS_DEFAULT) {
			Upgrade_update_table(&(device->key_id[0]), FALSE, 
								 &(device->upg_deg), &(device->upg_sts));
		}
	
		/* delete the device */
		Device_delete(device);

		/* low load, reopen the SteinsGate */
		if ((!ev_is_active(&host_io_wt)) && 
			(device_list->cur_num < LOAD_SOFT_LIMIT)) {
			ev_io_init(&host_io_wt, Socket_accept_cb, host_fd, EV_READ);
			ev_io_start(host_loop, &host_io_wt);
		}
		
	/* unknown time value */
	} else {
		;/* do nothing */
	}
}

/*==============================================================================
* Name	 :	int8 Device_handle(CONN_INFO* conn)
* Abstr	 :	Handle of the specific message from device
* Params :	CONN_INFO* conn : connection with device
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
static int8 Device_handle(CONN_INFO* conn)
{
	int8			err;
	DEVICE_INFO* 	device;
	
	err = EXEC_SUCS;
	device = conn->dev;

	switch (conn->r_buf[MSG_HEAD_LEN]) {
		
	/* device login */
	case :
		if (strlen(&device->key_id[0]) > 0) {	/* has login */
			log_warn(log_cat, "%s: already login", &(device->key_id[0]));
		} else {							/* first login */
			err = Device_login(device);
		}
		break;
		
	/* device logout */
	case :
		Device_logout(device);
		break;	
		
	/* answer of can id msg */
	case :	
		err = Device_answer(conn);
		break;
		
	/* regular msg */
	case :
		Device_breath(device);
		err = Device_control(&device_tb[device->tb_idx], conn);
		break;

	/* upgrade request */
	case :
		err = Upgrade_rqst(&(conn->r_buf[0]));
		break;

	/* upgrade command answer */
	case :
		err = Upgrade_cmd_ack(conn);
		break;
		
	/* upgrade single package answer */
	case :
		err = Upgrade_sgl_ack(conn);
		break;

	/* upgrade package check ok */
	case :
		Upgrade_chk_ack(conn);
		break;

	/* upgrade success */
	case :
		Upgrade_sucs_ack(conn);
		break;

	default:
		err = EXEC_FAIL;
		break;
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Device_login(DEVICE_INFO* device)
* Abstr	 :	Handle of device login
* Params :	DEVICE_INFO* device : device info
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
static int8 Device_login(DEVICE_INFO* device)
{
	int8		err;
	uint8		i;		/* servant server counter */
	uint8		mode;	/* login mode 0:acc on 1:reconnection */
	CONN_INFO*	conn;	/* master connection with device */
	
	err = EXEC_SUCS;
	i = 0;
	conn = &(device->d_conn);
	mode = conn->r_buf[MSG_HEAD_LEN + MSG_TYPE_LEN] & 0x01;

	/* get the device info */
	err = Device_init_acquire(device);

	/* delete old device of the same key_id */
	if (!err) {
		err = Device_past_clear(device);
	}

	/* add online num in db */
	if (!err) {
		err = Device_online_stats(&(device->key_id[0]), 1);
	}

	/* get can info and prepare msg */
	if (!err) {
		err = Device_can_acquire(conn, device->pt_ver);
	}

	/* prepare login msg */
	if (!err) {
		Device_login_trans(conn);
	}

	/* get servant db server address info */
	if (!err) {
		err = Client_init_acquire(device);
	}

	/* start connections to the servant db server */
	if (!err) {
		for (i = 0; i < device->slv_num; i++) {
			Client_start_handle(&device->s_conn[i]);
		}
	}

	/* analyze login mode */
	if (!err) {
		/* device is acc on, send can config info */
		if (mode == 0) {
			log_debug(log_cat, "%s: [mode=  ]", &(device->key_id[0]));
			err = Device_send(conn);
		/* device is reconnect */
		} else {
			log_debug(log_cat, "%s: [mode=  ]", &(device->key_id[0]));
			conn->is_alive = TRUE;
		}
	}

	return err;
}

/*==============================================================================
* Name	 :	void Device_breath(DEVICE_INFO* device)
* Abstr	 :	Handle of the heartbeat or normal message from device
* Params :	DEVICE_INFO* device : device info
* Return :	
* Modify :	
*=============================================================================*/
static void Device_breath(DEVICE_INFO* device)
{
	int8			err;
	uint8			i;			/* servant server counter 	*/
	CONN_INFO* 		tmp_conn;	/* temp servant connection 	*/
	CONN_INFO*		dev_conn;	/* device connection 		*/

	i 	= 0;
	err = EXEC_SUCS;
	tmp_conn = NULL;
	dev_conn = &(device->d_conn);

	for (i = 0; i < device->slv_num; i++) {
		
		tmp_conn = NULL;
		tmp_conn = &(device->s_conn[i]);
		
		if (tmp_conn->is_alive) {
			
			memset(&(tmp_conn->s_buf[0]), 0x00, SEND_LEN_MAX);
			memcpy(&(tmp_conn->s_buf[0]), &(dev_conn->r_buf[0]), SEND_LEN_MAX);
			
			err = Msg_send(&(tmp_conn->fd), &(tmp_conn->s_buf[0]), 
						   &(tmp_conn->name[0]));
			if (err) {
				Client_switch_to_stop(tmp_conn);
			}
			
		}
		
	}
}

/*==============================================================================
* Name	 :	void Device_logout(DEVICE_INFO* device)
* Abstr	 :	Handle of device logout
* Params :	DEVICE_INFO* device : device info
* Return :	
* Modify :	
*=============================================================================*/
static void Device_logout(DEVICE_INFO* device)
{
	/* transmit the logout msg */
	Device_breath(device);

	/* switch to stop handle */
	Device_switch_to_stop(&(device->d_conn));
}

/*==============================================================================
* Name	 :	int8 Device_answer(CONN_INFO* conn)
* Abstr	 :	Handle of answer message from device
* Params :	CONN_INFO* conn : connection with device
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
static int8 Device_answer(CONN_INFO* conn)
{
	int8			err;
	
	err = EXEC_SUCS;

	/* analyze the answer signal */
	switch (conn->r_buf[MSG_HEAD_LEN + MSG_TYPE_LEN]) {
	
	case :	/* delivery success */
		log_debug(log_cat, "%s: delivery success", &(conn->name[0]));
		conn->times = 0;
		conn->is_alive = TRUE;
		break;
	
	case :	/* delivery failed */
		log_fatal(log_cat, "%s: delivery failed [times=%d]", 
				  &(conn->name[0]), conn->times);
		if (conn->times < RETRY_CNT_MAX) {
			conn->times++;
			err = Device_send(conn);
		} else {
			err = EXEC_FAIL;
		}
		break;
	
	default:	/* can info delivery situation unknown */
		err = EXEC_FAIL;
		break;
	}
	
	return err;
}

/*==============================================================================
* Name	 :	int8 Device_control(DEVICE_ROW* row, CONN_INFO* conn)
* Abstr	 :	Check the control request of device
* Params :	DEVICE_ROW* row	 : corresponding row in tb of this device
*			CONN_INFO* 	conn : connection with this device
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
static int8 Device_control(DEVICE_ROW* row, CONN_INFO* conn)
{
	int8			err;
	DEVICE_INFO*	device;

	err = EXEC_SUCS;
	device = conn->dev;

	/* device has exited upgrading mode */
	if (device->upg_sts != ) {
		Upgrade_update_table(&(device->key_id[0]), FALSE, &(device->upg_deg), 
							 &(device->upg_sts));
	}

	/* control action is upgrading */
	if (row->ctrl_act == ) {

		device->upg_deg = (device->upg_deg > row->upg_deg) 
						  ? device->upg_deg 
						  : row->upg_deg;

		row->upg_deg  = 0;
		row->ctrl_act = 0;
		
		Upgrade_cmd_prepare(conn);
		
		err = Device_send(conn);
	}

	return err;
}

/*==============================================================================
* Name	 :	int8	Device_send(CONN_INFO* conn)
* Abstr	 :	Send message to the device
* Params :	CONN_INFO* 	conn : connection with this device
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Device_send(CONN_INFO* conn)
{
	int8	err;

	err = EXEC_SUCS;

	/* send message to the device */
	err = Msg_send(&(conn->fd), &(conn->s_buf[0]), &(conn->name[0]));
	
	/* send success, begin to waiting for answer */
	if (!err) {
		conn->t_wt.repeat = timer_array[TID_WAIT_ANSWER].t_val;
		ev_timer_again(host_loop, &(conn->t_wt));
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Device_init_acquire(DEVICE_INFO* device)
* Abstr	 :	Acquire information from db to initialize the device
* Params :	DEVICE_INFO* device : device info
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
static int8 Device_init_acquire(DEVICE_INFO* device)
{
	int8			err;
	cint8			sql[SQL_TEXT_LEN];		/* sql text 		 */
	cint8			mpu[MPU_UID_LEN + 1];	/* mpu uid of device */
	ResultSet_T 	result;					/* query result 	 */

	err    = EXEC_SUCS;
	result = NULL;
	memset(&sql[0], 0x00, SQL_TEXT_LEN);
	memset(&mpu[0], 0x00, MPU_UID_LEN + 1);

	/* get mpu uid */
	memcpy(&mpu[0], 
		   &(device->d_conn.r_buf[MSG_HEAD_LEN + MSG_TYPE_LEN + MSG_MASK_LEN]), 
		   MPU_UID_LEN);

	TRY {
		snprintf(&sql[0], SQL_TEXT_LEN, 
				 "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 
				 &mpu[0]);
		
		result = Connection_executeQuery(db_conn, &sql[0]);
		
		if (ResultSet_next(result)) {
			strncpy(&(device->key_id[0]), ResultSet_getString(result, 1), DEV_ID_LEN);
			strncpy(&(device->d_conn.name[0]), &(device->key_id[0]), DEV_ID_LEN);
			
			device->pt_ver = (int16)ResultSet_getInt(result, 2);
			
			log_debug(log_cat, "%s: log in [mpu=%s] [ver=%d]", &(device->key_id[0]), &mpu[0], device->pt_ver);
		} else {
			log_fatal(log_cat, "%s: no find [mpu=%s]", &(device->key_id[0]), &mpu[0]);
			err = EXEC_FAIL;
		}
	} CATCH(SQLException) {
		log_fatal(log_cat, "%s: SQLException -> %s", &(device->key_id[0]), 
				  Exception_frame.message);
		err = EXEC_FAIL;
	} END_TRY;

	return err;
}

/*==============================================================================
* Name	 :	int8 Device_past_clear(DEVICE_INFO* device)
* Abstr	 :	Delete old connection of the same device serial number and 
*			update shared memory.
* Params :	DEVICE_INFO* device : device info
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
static int8 Device_past_clear(DEVICE_INFO* device)
{
	int8			err;
	uint32			i;
	uint32			idx;		/* device index in tb */
	int8 			find;		/* find flag */
	CHANNEL_MSG		ch;			/* channel message */
	DEVICE_INFO*	old;		/* last device in list */

	err = EXEC_SUCS;
	i 	= 0;
	idx = 0;
	old = NULL;
	find = FALSE;
	memset(&ch, 0x00, sizeof(CHANNEL_MSG));

	/* lock the shared memory */
	err = Share_mtx_lock(&host_mtx);
	if (err) {
		ev_break(host_loop, EVBREAK_ALL);
	}

	if (!err) {
		
		/* search the device in shm table */
		for (i = 0; i < (*device_last); i++) {
			if (0 == memcmp(&(device_tb[i].key_id[0]), &(device->key_id[0]), DEV_ID_LEN)) {
				idx  = i;
				find = TRUE;
				break;
			}
		}

		/* find the device in shm table */
		if (find) {

			/* in same process */
			if (device_tb[idx].owner_idx == process_idx) {
				old = Device_search(&(device_tb[idx].key_id[0]));
				if ((old != NULL) && (old->is_bind == TRUE)) {
					Device_delete(old);
				}
			/* in different process */
			} else {
				ch.cmd = CHANNEL_CMD_DEV;
				ch.fd  = 0;
				ch.pid = current_pid;
				ch.idx = idx;
				if (processes[device_tb[idx].owner_idx].channel[0] > 0) {
					Channel_write(&(processes[device_tb[idx].owner_idx].channel[0]), &ch);
				}
			}

			/* update tb row info */
			device_tb[idx].owner_idx = process_idx;
			device_tb[idx].ol_num++;
			device->tb_idx  = idx;
			device->is_bind = TRUE;
			
		/* doesn't find the device */
		} else {
			memcpy(&(device_tb[*device_last].key_id[0]), &(device->key_id[0]), DEV_ID_LEN);
			device_tb[*device_last].owner_idx = process_idx;
			device_tb[*device_last].ctrl_act  = ACT_DEFAULT;
			device_tb[*device_last].ol_num++;
			device->tb_idx  = *device_last;
			device->is_bind = TRUE;
			(*device_last)++;
		}
	}

	/* unlock the shared memory */
	if (!err) {
		err = Share_mtx_unlock(&host_mtx);
		if (err) {
			ev_break(host_loop, EVBREAK_ALL);
		}
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Device_can_acquire(CONN_INFO* conn, int16 ver)
* Abstr	 :	Acquire config information from db to prepare can id message
* Params :	CONN_INFO* 	conn: connection with device
*			int16 	ver	: protocol version of this device
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
static int8 Device_can_acquire(CONN_INFO* conn, int16 ver)
{









	
}

/*==============================================================================
* Name	 :	void Device_login_trans(CONN_INFO* conn)
* Abstr	 :	Translate the login message from device
* Params :	CONN_INFO* 	conn: connection with device
* Return :	
* Modify :	
*=============================================================================*/
static void Device_login_trans(CONN_INFO* conn)
{
	uint8*			ptr;	/* buffer pointer */
	uint16			code;	/* check code */
	uint16			len;	/* msg length */
	DEVICE_INFO* 	device;	/* corresponding device */








	
}

/*==============================================================================
* Name	 :	int8 Device_online_stats(cint8* d_id, int8 offset)
* Abstr	 :	Statistics of device online number 
* Params :	cint8* 	d_id	: device id
*			int8 		offset	: online num offset
* Return :	
* Modify :	
*=============================================================================*/
static int8 Device_online_stats(cint8* id, int8 offset)
{
	int8		err;
	cint8		arg[SP_TEXT_LEN];
	
	err = EXEC_SUCS;
	memset(&arg[0], 0x00, SP_TEXT_LEN);

	snprintf(&arg[0], SP_TEXT_LEN, "'%s', %d", &id[0], offset);

	err = DB_func_exec(&db_conn, "xxxxxxxxxxxxxxxx", &arg[0], &id[0]);
	
	return err;
}

/*==============================================================================
* Name	 :	DEVICE_INFO* Device_search(cint8*	dev_id)
* Abstr	 :	Look for the device in list according to device serial number
* Params :	cint8*	dev_id : device serial number 
* Return :	DEVICE_INFO * device	
*			!= NULL : find the device
*			== NULL : no this device
* Modify :
*=============================================================================*/
DEVICE_INFO* Device_search(cint8*	dev_id)
{
	DEVICE_INFO* 	device;
	LIST_NODE*		node;

	device = NULL;
	node   = NULL;
	
	node = device_list->head;

	while (node) {
		device = (DEVICE_INFO*)(node->data);
		if (0 == memcmp(&(device->key_id[0]), &dev_id[0], DEV_ID_LEN)) {
			break;
		} else {
			node = node->next;
			device = NULL;
		}
	}
	
	return device;
}

/*==============================================================================
* Name	 :	struct tm * Time_get(time_t offset_sec, TIME_INFO* target_time)
* Abstr	 :	Get time
* Params :	time_t 		offset_sec	: time offset
*			TIME_INFO* 	target_time	: target time
* Return :	struct tm *	 :	pointer of request time
* Modify :	
*=============================================================================*/
static struct tm * Time_get(time_t offset_sec, TIME_INFO* target_time)
{
	time_t			now_sec;
	struct tm * 	rqst_time;

	now_sec   = 0;
	rqst_time = NULL;

	time(&now_sec);
	now_sec += offset_sec;
	rqst_time = localtime(&now_sec);

	if (target_time != NULL) {
		target_time->year = rqst_time->tm_year + 1900;
 		target_time->mon  = rqst_time->tm_mon  + 1;
		target_time->date = rqst_time->tm_mday;
		target_time->hour = rqst_time->tm_hour;
		target_time->min  = rqst_time->tm_min;
		target_time->sec  = rqst_time->tm_sec;
	}
	
	return rqst_time;
}

