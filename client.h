/* =============================================================================
* Filename : client.h
* Summary  : External interface of db server connections handling
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

#ifndef __CLIENT_H__
#define __CLIENT_H__

/*==============================================================================
* Header files
*=============================================================================*/
#include "db.h"
#include "data.h"

/*==============================================================================
* Function declaration
*=============================================================================*/
extern int8 Client_init_acquire(DEVICE_INFO* device);
extern void Client_start_handle(CONN_INFO* conn);
extern void Client_switch_to_stop(CONN_INFO* conn);

#endif



