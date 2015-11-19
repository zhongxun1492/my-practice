/* =============================================================================
* Filename : data.c
* Summary  : Data handling. Contain with data receive, data send and data check
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
#include "data.h"

/*==============================================================================
* Global constant definitions
*=============================================================================*/
/* definition of timer group */
TIMER_INFO timer_array[TID_MAX] = 
{
	{TID_CONN_START, 	60.0 },
	{TID_STOP_DELAY, 	60.0 },
	{TID_WAIT_DATA,  	60.0 },
	{TID_WAIT_ANSWER,  	60.0 },
	{TID_WORKER_START, 	60.0 }
};

/*==============================================================================
* Interface declarations
*=============================================================================*/
static int8 Data_recv(int32* fd, uint8* buff, uint16 len, cint8* nm);
static int8 Data_send(int32* fd, uint8* buff, int32  len, cint8* nm);


#if 0
#endif

/*==============================================================================
* Name	 :	int8 Data_send(int32* fd, uint8* buff, int32 len, cint8* nm)
* Abstr	 :	Data sending
* Params :	int32* 	fd 	 : socket fd
*			uint8* 	buff : data buffer
*			int32 	len	 : data length
*			cint8* 	nm	 : connection name
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Data_send(int32* fd, uint8* buff, int32 len, cint8* nm)
{
	int8		err;
	int32		size;			/* number of bytes at a time */
	uint32		start;			/* start position of next sending */
	uint32		retry;			/* retry times */
	int32 		s_len;			/* send length */
	
	err   = EXEC_SUCS;
	size  = 0;
	start = 0;
	retry = 0;
	s_len = len;

	while (s_len > 0) {
		size = 0;
		size = send(*fd, &buff[start], s_len, 0);
		if (size < 0) {	/* send failed */
			if (
				((errno == EAGAIN) 		|| 
				 (errno == EWOULDBLOCK) || 
				 (errno == EINTR)) 
				&& 
				(retry < SEND_RETRY_MAX)) {	/* system resource temporarily 
									not available, wait 200us and retry */
				usleep(200);
				retry++;
				continue;
			} else {						/* fatal error or wait over 0.5s */
				retry = 0;
				err = EXEC_FAIL;
				break;
			}
		} else {		/* send success */
			retry  = 0;
			s_len -= size;
			start += size;
		}
	}

	if (!err) {
		log_debug(log_cat, "%s: send success [type=%02x] [len=%d]", 
				  &nm[0], buff[MSG_HEAD_LEN], len);
	} else {
		log_fatal(log_cat, "%s: send failed  [type=%02x] [len=%d] [%s]", 
				  &nm[0], buff[MSG_HEAD_LEN], len, strerror(errno));
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Msg_send(int32* fd, uint8* buff,  cint8* nm)
* Abstr	 :	Send message
* Params :	int32* 	fd   : socket fd
*			uint8* 	buff : data buffer
*			cint8* 	nm	 : connection name
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Msg_send(int32* fd, uint8* buff,  cint8* nm)
{
	int8		err;
	uint16	len;

	err = EXEC_SUCS;
	len = 0;

	len = *((uint16*)(&buff[0]));
	err = Data_send(fd, &buff[0], MSG_HEAD_LEN + len, &nm[0]);

	return err;
}

/*==============================================================================
* Name	 :	int8 Msg_recv(int32* fd, uint8* buff, cint8* nm)
* Abstr	 :	Data receiving
* Params :	int32* 	fd   : socket fd
*			uint8* 	buff : data buffer
*			cint8* 	nm	 : connection name
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Msg_recv(int32* fd, uint8* buff, cint8* nm)
{
	int8		err;
	uint16		len;				/* message head */

	err = EXEC_SUCS;
	len = 0;

	/* initialize recv buffer */
	memset(&buff[0], 0x00, RECV_LEN_MAX);

	/* recv msg head */
	err = Data_recv(fd, &buff[0], MSG_HEAD_LEN, &nm[0]);

	/* recv msg body */
	if (!err) {
		memcpy(&len, &buff[0], MSG_HEAD_LEN);
		err = Data_recv(fd, &buff[MSG_HEAD_LEN], len, &nm[0]);
	}

	if (!err) {
		log_debug(log_cat, "%s: recv success [len=%u] [type=%02x]", 
				  &nm[0], len, buff[MSG_HEAD_LEN]);
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Data_recv(int32* fd, uint8* buff, uint16 len, cint8* nm)
* Abstr	 :	specific handling of receive message
* Params :	int32* 	fd   : socket fd
*			uint8* 	buff : data buffer
*			uint16 	len	 : data length
*			cint8* 	nm	 : connection name
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
static int8 Data_recv(int32* fd, uint8* buff, uint16 len,  cint8* nm)
{
	int8		err;	
	int32		size;	/* length of recv actually */

	err  = EXEC_SUCS;
	size = 0;

	/* begin recv data */
	size = recv(*fd, &buff[0], len, 0);
	
	switch (size) {
	case -1: /* recv error */
		log_fatal(log_cat, "%s: msg recv failed [%s]", &nm[0], strerror(errno));
		err = EXEC_FAIL;
		break;
	case 0:	/* socket closed */
		log_warn (log_cat, "%s: msg recv failed [opposite closed]", &nm[0]);
		err = EXEC_FAIL;
		break;
	default:/* no exception */
		if (size < len) {
			log_fatal(log_cat, "%s: msg recv failed [loss %u<%u]", &nm[0], size, len);
			err = ERROR_DATA_LOSS;
		}
		break;
	}

	return err;
}

/*==============================================================================
* Name	 :	int8 Msg_check(uint8* buff, cint8* nm)
* Abstr	 :	Handling of data check
* Params :	uint8* 	buff : data buffer
*			cint8* 	nm	 : connection name
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :	
*=============================================================================*/
int8 Msg_check(uint8* buff, cint8* nm)
{
	int8	err;
	uint16	len;		/* length of msg body  */
	uint16 	recv;		/* receive check code  */
	uint16 	gnrt;		/* generate check code */

	err  = EXEC_SUCS;
	len  = 0;
	recv = 0;
	gnrt = 0;

	/* body length */
	memcpy(&len, &buff[0], MSG_HEAD_LEN);

	/* get recv check code, 
	   start postition is [MSG_HEAD_LEN + body_len - CRC_CODE_LEN] */
	memcpy(&recv, &buff[len], CRC_CODE_LEN);

	/* get generate check code */
	gnrt = CRCCode_get(&buff[0], len);

	/* compare two check code */
	if (recv != gnrt) {
		log_fatal(log_cat, "%s: data check failed [recv=%04x gnrt=%04x]", &nm[0], recv, gnrt);
		err = EXEC_FAIL;
	}

	return err;
}

