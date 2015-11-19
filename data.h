/* =============================================================================
* Filename : data.h
* Summary  : Struct definitions of connection and device
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

#ifndef __DATA_H__
#define __DATA_H__

/*==============================================================================
* Header files
*=============================================================================*/
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <ev.h>
#include "api.h"
#include "log.h"
#include "crc.h"

/*==============================================================================
* Macro Definition
*=============================================================================*/
#define IP_ADDR_LEN			(15)		/* ip address length 		*/
#define RECV_LEN_MAX		(2048)		/* recv buff max length 	*/
#define SEND_LEN_MAX		(2048)		/* send buff max length 	*/
#define CONN_NM_LEN 		(40)		/* connection name length 	*/
#define SEND_RETRY_MAX		(2500)		/* send retry max times		*/
#define MSG_HEAD_LEN		(2)			/* msg head length 			*/
#define MSG_TYPE_LEN		(1)			/* msg type length 			*/
#define MSG_MASK_LEN		(1)			/* msg mask length 			*/
#define RETRY_CNT_MAX		(5)			/* max send retry times 	*/
#define DEV_ID_LEN 			(10)		/* device id length 		*/
#define	SLV_NUM_MAX			(5)			/* slave db server max num 	*/
#define TIME_STAMP_LEN		(14)		/* timestamp length 		*/
#define ERROR_DATA_LOSS		(31)		/* data loss 				*/

/*==============================================================================
* Struct definitions
*=============================================================================*/
/* struct of time info */
typedef struct _time_info {
	uint8	sec;		/* second */
	uint8	min;		/* minute */
	uint8	hour;		/* hour   */
	uint8	date;		/* day	  */
	uint8	mon;		/* month  */
	uint16	year;		/* year   */
} TIME_INFO;

/* enum of timer id */
typedef enum _timer_id {
	TID_CONN_START,			/* re/connect    */
	TID_STOP_DELAY,			/* stop delay	 */
	TID_WAIT_DATA,			/* wait data	 */
	TID_WAIT_ANSWER,		/* wait answer 	 */
	TID_WORKER_START,		/* start workers */
	TID_MAX					/* num of timers */
} TIMER_ID;

/* struct of timer */
typedef struct _timer_s {
	TIMER_ID	t_id;		/* timer id 	*/
	float		t_val;		/* timer value 	*/
} TIMER_INFO;

/* actions of device controlling */
typedef enum _control_action {
	ACT_MAX				/* max num of actions */
} CONTROL_ACTION;

/* upgrading status */
typedef enum _upgrade_sts {
	UPG_STS_MAX			/* number of status		*/
} UPG_STS;

/* connection struct */
typedef struct _conn_info {
	int8 		is_alive;				/* is/not alive  */
	cint8		name[CONN_NM_LEN];		/* conn name 	 */
	cint8 		addr[IP_ADDR_LEN + 1];	/* ip address 	 */
	uint16		port;					/* bind port 	 */
	int32 		fd;						/* socket fd 	 */
	ev_io		io_wt;					/* io watcher 	 */
	ev_timer	t_wt;					/* timer watcher */
	uint8		times;					/* send times 	 */
	uint8		r_buf[RECV_LEN_MAX];	/* recv buffer 	 */
	uint8		s_buf[RECV_LEN_MAX];	/* send buffer   */
	struct _device_info	*dev;			/* corresponding device */
} CONN_INFO;

/* device info struct */
typedef struct _device_info {
	cint8				key_id[DEV_ID_LEN + 1]; /* device serial number */
	int16 				pt_ver;			 	 	/* protocol version 	*/
	struct _conn_info	d_conn;				 	/* device connection 	*/
	struct _conn_info 	s_conn[SLV_NUM_MAX]; 	/* servant connections  */
	uint8				slv_num;				/* db server max num 	*/
	int8				is_bind;			 	/* is/not bind   		*/
	uint32				tb_idx;				 	/* device index in tb 	*/
	uint8				upg_deg;			 	/* last upgrade degree  */
	UPG_STS				upg_sts;				/* upgrade status 		*/
} DEVICE_INFO;

/* device row struct in tb */
typedef struct _device_row {
	cint8		key_id[DEV_ID_LEN + 1];	/* device serial number */
	int32		ol_num;					/* device online num 	*/
	uint32		owner_idx;				/* last process index 	*/
	uint32		ctrl_act;				/* control action 		*/
	uint8		upg_deg;				/* last upgrade degree 	*/
} DEVICE_ROW;

/*==============================================================================
* Global variable declaration
*=============================================================================*/
extern TIMER_INFO 	timer_array[];	/* definitions of timer group */

/*==============================================================================
* Function declaration
*=============================================================================*/
extern int8 Msg_recv(int32* fd, uint8* buff, cint8* nm);
extern int8 Msg_send(int32* fd, uint8* buff, cint8* nm);
extern int8 Msg_check(uint8* buff, cint8* nm);

#endif

