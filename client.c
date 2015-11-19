/* =============================================================================
* Filename : client.c
* Summary  : Handling of sockets which are connected with the db servers. 
*			 Most of the time, sockets are waiting for data from db servers. 
*			 These connections will be closed and restart after a time while 
*			 exceptions occur. Authentication message is sent first always once 
*			 connections is established.	 
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
#include "client.h"

/*==============================================================================
* Interface declarations
*=============================================================================*/
extern struct ev_loop* 	host_loop;	/* inner event loop */
extern ev_async			async_wt;	/* asynchronization watcher */

extern int8 Socket_init (int32* fd);
extern int8 Socket_close(int32* fd, cint8* nm);
extern int8 Socket_shutdown(int32* fd, cint8* nm);

static void Client_switch_to_start(CONN_INFO* conn);
static void Client_start_prepare (CONN_INFO* conn); 
static void Client_switch_to_test(CONN_INFO* conn);
static void Client_switch_to_recv(CONN_INFO* conn);
static void Client_start_cb(struct ev_loop* lp, ev_timer* wt, int ev);
static void Client_stop_cb(struct ev_loop* lp, ev_timer* wt, int ev);
static void Client_test_cb(struct ev_loop* lp, ev_io* wt, int ev);
static void Client_recv_cb(struct ev_loop* lp, ev_io* wt, int ev);

#if 0
#endif

/*==============================================================================
* Name	 :	int8 Client_init_acquire(DEVICE_INFO* device)
* Abstr	 :	Acquire ip address and port of the target db servers according to 
*			the protocol version of each device
* Params :	DEVICE_INFO* device : device information
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Client_init_acquire(DEVICE_INFO* device)
{
	int8			err;
	cint8 			sql[SQL_TEXT_LEN];	/* sql text 		*/
	CONN_INFO*		conn;				/* temp connection 	*/
	ResultSet_T 	result;				/* query result 	*/

	err  = EXEC_SUCS;
	conn = NULL;
	result = NULL;
	memset(&sql[0], 0x00, SQL_TEXT_LEN);

	TRY {
		snprintf(&sql[0], SQL_TEXT_LEN, 
				 "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 
				 device->pt_ver);

		result = Connection_executeQuery(db_conn, &sql[0]);

		while ((ResultSet_next(result)) && (device->slv_num < SLV_NUM_MAX)) {
			conn = NULL;
			conn = &(device->s_conn[device->slv_num]);
			/* initialize device pointer */
			conn->dev = device;
			/* ip address */
			strncpy(&(conn->addr[0]), ResultSet_getString(result, 1), IP_ADDR_LEN);
			/* listen port */
			conn->port = (uint16)ResultSet_getInt(result, 2);
			/* connection name */
			snprintf(&(conn->name[0]), CONN_NM_LEN, "%s [%s:%d]", 
					 &(device->key_id[0]), &(conn->addr[0]), conn->port);
			/* prepare send data */
			memcpy (&conn->s_buf[0], &(device->d_conn.r_buf[0]), RECV_LEN_MAX);

			log_debug(log_cat, "%s: summon servant success", &conn->name[0]);

			device->slv_num++;
		}

		log_debug(log_cat, "%s: [servant num=%d]", &(device->key_id[0]), device->slv_num);
		
		(device->slv_num == 0) ? (err = EXEC_FAIL) : (err = EXEC_SUCS);

	} CATCH(SQLException) {
		log_fatal(log_cat, "%s: SQLException -> %s", &(device->key_id[0]), 
				  Exception_frame.message);
		err = EXEC_FAIL;
	} END_TRY;

	return err;
}

/*==============================================================================
* Name	 :	void Client_start_prepare(CONN_INFO* conn)
* Abstr	 :	Prepare authentication message for next connecting before close 
*			current connection		
* Params :	CONN_INFO* conn : connection with db server
* Return :	
* Modify :	
*=============================================================================*/
static void Client_start_prepare(CONN_INFO* conn)
{
	uint8*			ptr;	/* buffer pointer */
	uint16			code;	/* check code */
	uint16			len;	/* msg length */
	DEVICE_INFO* 	device;	/* corresponding device */
	
	code = 0;
	len  = 0;
	ptr  = &conn->s_buf[0];
	device = conn->dev;
	
	/* msg head */
	len = MSG_TYPE_LEN + DEV_ID_LEN + sizeof(int16) + CRC_CODE_LEN;
	*((uint16*)ptr) = len;
	ptr += MSG_HEAD_LEN;

	/* msg type */
	*ptr = 0x0f;
	ptr += 1;

	/* device id */
	memcpy(ptr, &(device->key_id[0]), DEV_ID_LEN);
	ptr += DEV_ID_LEN;

	/* can ver */
	*((int16*)ptr) = device->pt_ver;
	ptr += sizeof(int16);

	/* crc check code */
	code = CRCCode_get(&conn->s_buf[0], len);
	*((uint16*)ptr) = code;
}

/*==============================================================================
* Name	 :	void Client_start_handle(CONN_INFO* conn)
* Abstr	 :	Start the connection with db server	
* Params :	CONN_INFO* conn : connection with db server
* Return :	
* Modify :	
*=============================================================================*/
void Client_start_handle(CONN_INFO* conn)
{
	int8				err;
	struct sockaddr_in 	addr;		/* servant address info */
	
	err = EXEC_SUCS;
	memset(&addr, 0x00, sizeof(struct sockaddr_in));

	/* initialize the socket fd */
	err = Socket_init(&conn->fd);

	if (!err) {
		/* bind the address and port */
		addr.sin_family = AF_INET;
		addr.sin_port   = htons(conn->port);
		addr.sin_addr.s_addr = inet_addr(&conn->addr[0]);

		/* start connection */
		err = connect(conn->fd, (struct sockaddr *)(&addr), sizeof(struct sockaddr));
		if ((err) && (errno != EINPROGRESS)) {
			log_fatal(log_cat, "%s: connect failed [s_fd=%d] [%s]", 
					  &conn->name[0], conn->fd, strerror(errno));
			err = EXEC_FAIL;
		} else {
			err = EXEC_SUCS;
		}
	}

	/* check the result of connect */
	if (!err) {
		Client_switch_to_test(conn);
	} else {
		Client_switch_to_stop(conn);
	}
}

/*==============================================================================
* Name	 :	void Client_switch_to_start(CONN_INFO* conn)
* Abstr	 :	Start the connection restart timer
* Params :	CONN_INFO* conn : connection with db server
* Return :	
* Modify :	
*=============================================================================*/
static void Client_switch_to_start(CONN_INFO* conn)
{
	/* start the connection after 120s */
	ev_timer_init(&conn->t_wt, Client_start_cb, timer_array[TID_CONN_START].t_val, 0.);
	ev_timer_start(host_loop, &conn->t_wt); 

	/* notice the host loop */
	ev_async_send(host_loop, &async_wt);
}

/*==============================================================================
* Name	 :	void Client_switch_to_test(CONN_INFO* conn)
* Abstr	 :	Start the socket test watcher 
* Params :	CONN_INFO* conn : connection with db server
* Return :	
* Modify :	
*=============================================================================*/
static void Client_switch_to_test(CONN_INFO* conn)
{	
	/* io wathcer switch to testing status */
	ev_io_init(&conn->io_wt, Client_test_cb, conn->fd, EV_READ | EV_WRITE);
	ev_io_start(host_loop, &conn->io_wt);

	/* notice the host loop */
	ev_async_send(host_loop, &async_wt);
}

/*==============================================================================
* Name	 :	void Client_switch_to_recv(CONN_INFO* conn)
* Abstr	 :	Start the io watcher to waiting for data
* Params :	CONN_INFO* conn : connection with db server
* Return :	
* Modify :	
*=============================================================================*/
static void Client_switch_to_recv(CONN_INFO* conn)
{
	/* io wathcer switch to waiting for data */
	ev_io_init(&conn->io_wt, Client_recv_cb, conn->fd, EV_READ);
	ev_io_start(host_loop, &conn->io_wt); 

	/* notice the host loop */
	ev_async_send(host_loop, &async_wt);
}

/*==============================================================================
* Name	 :	void Client_switch_to_stop(CONN_INFO* conn)
* Abstr	 :	Stop the socket io watcher and close it after a moment
* Params :	CONN_INFO* conn : connection with db server
* Return :	
* Modify :	
*=============================================================================*/
void Client_switch_to_stop(CONN_INFO* conn)
{
	/* reset connection alive flag */
	conn->is_alive = FALSE;

	log_fatal(log_cat, "%s: catch exception", &conn->name[0]);

	/* stop connection io watcher */
	ev_io_stop(host_loop, &conn->io_wt);

	/* do stopping handle after 10s */
	ev_timer_init(&conn->t_wt, Client_stop_cb, timer_array[TID_STOP_DELAY].t_val, 0.);
	ev_timer_start(host_loop, &conn->t_wt); 

	/* notice the host loop */
	ev_async_send(host_loop, &async_wt);
}

/*==============================================================================
* Name	 :	void Client_start_cb(struct ev_loop* lp, ev_timer* wt, int ev)
* Abstr	 :	Time to start the connection
* Params :	struct ev_loop* lp: inner event loop
*			ev_timer* 		wt: connection start timer watcher
*			int 			ev: specific event
* Return :	
* Modify :	
*=============================================================================*/
static void Client_start_cb(struct ev_loop* lp, ev_timer* wt, int ev)
{
	CONN_INFO*			conn;	/* connection with servant server */
		
	conn = container_of(wt, CONN_INFO, t_wt);

	Client_start_handle(conn);
}

/*==============================================================================
* Name	 :	void Client_test_cb(struct ev_loop* lp, ev_io* wt, int ev)
* Abstr	 :	Check the connection is established successful or not
* Params :	struct ev_loop* lp: inner event loop
*			ev_io* 			wt: socket io watcher
*			int 			ev: specific event
* Return :	
* Modify :	
*=============================================================================*/
static void Client_test_cb(struct ev_loop* lp, ev_io* wt, int ev)
{	
	int8				err;
	uint16			len;	/* msg length */
	CONN_INFO*			conn;	/* connection with servant server */

	err = EXEC_SUCS;
	len	= 0;
	conn = container_of(wt, CONN_INFO, io_wt);

	/* stop the socket io watcher */
	ev_io_stop(host_loop, wt); 
	ev_async_send(host_loop, &async_wt);

	/* check the result of connect */
	switch (ev) {
	case EV_WRITE:	/* connect success */
		log_debug(log_cat, "%s: connect success [s_fd=%d]", &conn->name[0], conn->fd);
		conn->is_alive = TRUE;
		/* send login msg */
		err = Msg_send(&(conn->fd), &(conn->s_buf[0]), &(conn->name[0]));
		if (!err) {
			Client_switch_to_recv(conn);
		} else {
			Client_switch_to_stop(conn);
		}
		break;
	case EV_READ:
	case (EV_READ | EV_WRITE):
	default:		/* connect failed */
		log_fatal(log_cat, "%s: connect failed [s_fd=%d] [%s]", 
				  &conn->name[0], conn->fd, strerror(errno));
		Client_switch_to_stop(conn);
		break;
	}
}

/*==============================================================================
* Name	 :	void Client_recv_cb(struct ev_loop* lp, ev_io* wt, int ev)
* Abstr	 :	Receive data handle
* Params :	struct ev_loop* lp: inner event loop
*			ev_io* 			wt: socket io watcher
*			int 			ev: specific event
* Return :	
* Modify :	
*=============================================================================*/
static void Client_recv_cb(struct ev_loop* lp, ev_io* wt, int ev)
{
	int8				err;
	CONN_INFO*			conn;	/* connection with servant server */

	err  = EXEC_SUCS;
	conn = container_of(wt, CONN_INFO, io_wt);

	if (!err) {
		err = Msg_recv(&conn->fd, &conn->r_buf[0], &conn->name[0]);
	}

	if (err) {
		Client_switch_to_stop(conn);
	}
}

/*==============================================================================
* Name	 :	void Client_stop_cb(struct ev_loop* lp, ev_timer* wt, int ev)
* Abstr	 :	Time to close the connection and start the re-connect timer
* Params :	struct ev_loop* lp: inner event loop
*			ev_timer* 		wt: timer watcher
*			int 			ev: specific event
* Return :	
* Modify :	
*=============================================================================*/
static void Client_stop_cb(struct ev_loop* lp, ev_timer* wt, int ev)
{	
	CONN_INFO*			conn;	/* connection with servant server */
	
	conn = container_of(wt, CONN_INFO, t_wt);

	/* free socket resource */
	Socket_shutdown(&conn->fd, &conn->name[0]);
	Socket_close   (&conn->fd, &conn->name[0]);

	/* prepare reconnect msg */
	memset(&conn->s_buf[0], 0x00, SEND_LEN_MAX);
	Client_start_prepare(conn);

	/* re-start the connection with servant server */
	Client_switch_to_start(conn);
}



