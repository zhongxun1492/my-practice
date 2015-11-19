/* =============================================================================
* Filename : device.h
* Summary  : External interface of device connections handling
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

#ifndef __DEVICE_H__
#define __DEVICE_H__

/*==============================================================================
* Header files
*=============================================================================*/
#include "db.h"
#include "data.h"
#include "list.h"
#include "ipc.h"

/*==============================================================================
* Macro Definition
*=============================================================================*/
#define MPU_UID_LEN		24		/* mpu uid length */
#define LOAD_SOFT_LIMIT	200		/* workers load soft limit 	*/
#define LOAD_HARD_LIMIT	250		/* workers load hard limit 	*/
#define ACCOFF_MSG_LEN 	6		/* acc off message length 	*/

/*==============================================================================
* Struct definitions
*=============================================================================*/
/* enum of msg id */
typedef enum _msg_id {









	
} MSG_ID;

/*==============================================================================
* Global variable declaration
*=============================================================================*/
extern LINK_LIST* 		device_list;		/* device list 	 */
extern DEVICE_ROW*		device_tb;			/* device table  */
extern uint32*			device_last;		/* device num 	 */

/*==============================================================================
* Function declaration
*=============================================================================*/
extern void Device_create(int32* fd);
extern void Device_shutup(DEVICE_INFO* device);
extern void Device_delete(DEVICE_INFO* device);
extern int8 Device_send(CONN_INFO* conn);
extern DEVICE_INFO* Device_search(cint8* dev_id);

#endif

