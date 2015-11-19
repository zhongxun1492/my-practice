/* =============================================================================
* Filename : server.h
* Summary  : definitions and declarations of socket interface 
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

#ifndef __SERVER_H__
#define __SERVER_H__

/*==============================================================================
* Header files
*=============================================================================*/
#include "data.h"

/*==============================================================================
* Macro Definition
*=============================================================================*/
#define SERVER_ADDR 	"192.168.0.100"		/* ip address */
#define SERVER_PORT 	8888				/* bind port */
#define PEND_CON_MAX	5					/* recv socket rqst num once */
#define OPT_NM_LEN		15					/* option name length */

/*==============================================================================
* Function declaration
*=============================================================================*/
extern int8 Socket_init (int32* fd);
extern int8 Socket_start(int32* fd);
extern int8 Socket_shutdown(int32* fd, cint8* nm);
extern int8 Socket_close	 (int32* fd, cint8* nm);
extern int8 Socket_set_nonblock(int32* fd);
extern void Socket_accept_cb(struct ev_loop* lp, ev_io* wt, int ev);

#endif

