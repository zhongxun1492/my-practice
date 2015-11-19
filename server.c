/* =============================================================================
* Filename : server.c
* Summary  : Handling of socket
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
#include "server.h"

/*==============================================================================
* Interface declarations
*=============================================================================*/
extern void Device_create(int32* dev_s_fd);

static int8 Socket_set_attr(int32* fd, cint8* opt_nm, int32 opt_no, 
 							  int32* opt_val);

#if 0
#endif

/*==============================================================================
* Name	 :	int8 Socket_init(int32* fd)
* Abstr	 :	Get a new socket fd and set attributes of it
* Params :	int32* 	fd : socket fd pointer
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Socket_init(int32* fd)
{
	int8		err;
	int32 		opt_val;
	cint8		opt_nm[OPT_NM_LEN];

	opt_val = 0;
	err 	= EXEC_SUCS;
	memset(&opt_nm[0], 0x00, sizeof(cint8) * OPT_NM_LEN);

	/* get a new socket fd */
	*fd = socket(AF_INET, SOCK_STREAM, 0);
	if (*fd <= 0) {
		log_fatal(log_cat, "s_fd: open failed [%s]", strerror(errno));
		*fd = -1;
		err = EXEC_FAIL;
	} else {
		log_debug(log_cat, "s_fd: open success [s_fd=%d]", *fd);
	}

	/* set the socket address reuse */
	if (!err) {
		opt_val= 1;
		memset  (&opt_nm[0], 0x00, sizeof(cint8) * OPT_NM_LEN);
		snprintf(&opt_nm[0], OPT_NM_LEN, "%s", "addr reuse");
		err = Socket_set_attr(fd, &opt_nm[0], SO_REUSEADDR, &opt_val);
	}

	/* set the socket recv buff size to 2k */
	if (!err) {
		opt_val= RECV_LEN_MAX / 2;
		memset  (&opt_nm[0], 0x00, sizeof(cint8) * OPT_NM_LEN);
		snprintf(&opt_nm[0], OPT_NM_LEN, "%s", "recv buff size");
		err = Socket_set_attr(fd, &opt_nm[0], SO_RCVBUF, &opt_val);
	}

	/* set the socket send buff size to 16k */
	if (!err) {
		opt_val= SEND_LEN_MAX / 2;
		memset  (&opt_nm[0], 0x00, sizeof(cint8) * OPT_NM_LEN);
		snprintf(&opt_nm[0], OPT_NM_LEN, "%s", "send buff size");
		err = Socket_set_attr(fd, &opt_nm[0], SO_SNDBUF, &opt_val);
	}

	/* set the socket to non block */
	if (!err) {
		err = Socket_set_nonblock(fd);
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Socket_start(int32* fd)
* Abstr	 :	Bind the address information and start listening on the port
* Params :	int32* 	fd : socket fd pointer
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Socket_start(int32* fd)
{
	int8				err;
	struct sockaddr_in 	addr;

	err = EXEC_SUCS;
	memset(&addr, 0x00, sizeof(struct sockaddr_in));

	/* set the address and port */
	addr.sin_family = AF_INET;
	addr.sin_port 	= htons(SERVER_PORT);
	addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);

	/* bind the socket */
	err = bind(*fd, (struct sockaddr *)(&addr), sizeof(struct sockaddr_in));
	if (err) {
		log_fatal(log_cat, "s_fd: %d bind failed [%s]", *fd, strerror(errno));
		err = EXEC_FAIL;
	} else {
		log_debug(log_cat, "s_fd: %d bind success", *fd);
	}

	/* listen to the port */
	if (!err) {
		err = listen(*fd, PEND_CON_MAX);
		if (err) {
			log_fatal(log_cat, "s_fd: %d listen failed [%s]", *fd, strerror(errno));
			err = EXEC_FAIL;
		} else {
			log_debug(log_cat, "s_fd: %d listen success", *fd);
		}
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Socket_shutdown(int32* fd, cint8* nm)
* Abstr	 :	Shutdown the connection
* Params :	int32* 	fd 	: socket fd pointer
*			cint8* 	nm 	: socket name
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Socket_shutdown(int32* fd, cint8* nm)
{
	int8		err;

	err = EXEC_SUCS;

	if (*fd > 0) {
		err = shutdown(*fd, SHUT_RDWR);
		if (err) {
			log_fatal(log_cat, "s_fd: %d [%s] shutdown failed [%s]", 
					  *fd, &nm[0], strerror(errno));
			err = EXEC_FAIL;
		} else {
			log_debug(log_cat, "s_fd: %d [%s] shutdown success", *fd, &nm[0]);
		}
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Socket_close(int32* fd, cint8* nm)
* Abstr	 :	Close the connection
* Params :	int32* 	fd 	: socket fd pointer
*			cint8* 	nm 	: socket name
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Socket_close(int32* fd, cint8* nm)
{
	int8		err;

	err = EXEC_SUCS;

	if (*fd > 0) {
		err = close(*fd);
		if (err) {
			log_fatal(log_cat, "s_fd: %d [%s] close failed [%s]", 
					  *fd, &nm[0], strerror(errno));
			err = EXEC_FAIL;
		} else {
			log_debug(log_cat, "s_fd: %d [%s] close success", *fd, &nm[0]);
		}
	}

	*fd = -1;

	return err;
}

/*==============================================================================
* Name	 :	static int8 Socket_set_attr(int32* fd, cint8* opt_nm, 
*							  			  int32  opt_no, int32* opt_val)
* Abstr	 :	Set attributes of the socket
* Params :	int32* 	fd 		: socket fd
*			cint8* 	opt_nm 	: option name
*			int32  	opt_no 	: option no
*			int32* 	opt_val	: option value
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
static int8 Socket_set_attr(int32* fd, cint8* opt_nm, 
							  int32  opt_no, int32* opt_val)
{
	int8		err;

	err = EXEC_SUCS;

	/* socket attribute set */
	err = setsockopt(*fd, SOL_SOCKET, opt_no, (void*)opt_val, sizeof(int32));
	if (err) {
		log_fatal(log_cat, "s_fd: %d set %s failed [%s]", 
				  *fd, opt_nm, strerror(errno));
		err = EXEC_FAIL;
	} else {
		log_debug(log_cat, "s_fd: %d set %s success", *fd, opt_nm);
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Socket_set_nonblock(int32* fd)
* Abstr	 :	Set nonblock of this socket
* Params :	int32* 	fd 	: socket fd
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Socket_set_nonblock(int32* fd)
{
	int8		err;
	int32		attr;		/* attribute flag */

	attr = 0;
	err  = EXEC_SUCS;

	/* get the primary attribute of the fd */
	attr = fcntl(*fd, F_GETFL, 0);
	if (attr < 0) {
		log_fatal(log_cat, "s_fd: %d get primary attr failed [%s]", 
				  *fd, strerror(errno));
		err = EXEC_FAIL;
	}

	/* set fd to nonblock */
	if (!err) {
		err = fcntl(*fd, F_SETFL, attr | O_NONBLOCK);
		if (err < 0) {
			log_fatal(log_cat, "s_fd: %d set nonblock failed [%s]", 
					  *fd, strerror(errno));
			err = EXEC_FAIL;
		} 
	}

	if (!err) {
		log_debug(log_cat, "s_fd: %d set nonblock success", *fd);
	}
	
	return err;
}

/*==============================================================================
* Name	 :	void Socket_accept_cb(struct ev_loop* lp, ev_io* wt, int ev)
* Abstr	 :	Accept new connection
* Params :	struct ev_loop* lp : inner event loop
*			ev_io* 			wt : listen socket fd io watcher
*			int 			ev : specific event
* Return :	
* Modify :	
*=============================================================================*/
void Socket_accept_cb(struct ev_loop* lp, ev_io* wt, int ev)
{
	int32				new_s_fd;		/* new socket fd  */
	socklen_t 			addr_len;		/* address length */
	struct sockaddr_in 	addr;			/* temp address   */
	
	new_s_fd = -1;
	addr_len = sizeof(struct sockaddr_in);
	memset(&addr, 0x00, sizeof(struct sockaddr_in));

	/* accept connection */
	new_s_fd = accept(wt->fd, (struct sockaddr*)(&addr), &addr_len);
	if (new_s_fd <= 0) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
			;
		} else {
			log_fatal(log_cat, "acpt: accept failed [%s]", strerror(errno));
			ev_break(lp, EVBREAK_ALL);
		}
	} else {
		if (Socket_set_nonblock(&new_s_fd)) {
			Socket_close(&new_s_fd, "new_s_fd");
		} else {
			log_debug(log_cat, "acpt: accept success [s_fd=%d]", new_s_fd);
			Device_create(&new_s_fd);
		}
	}
}



