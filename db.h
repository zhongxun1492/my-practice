/* =============================================================================
* Filename : db.h
* Summary  : Definitions of the DB module
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

#ifndef __DB_H__
#define __DB_H__

/*==============================================================================
* Header files
*=============================================================================*/
#include "api.h"
#include "log.h"
#include "zdb.h"

/*==============================================================================
* Macro Definition
*=============================================================================*/
#define DB_TYPE			"postgresql"	/* DB type 		*/
#define DB_HOST			"localhost"		/* DB host ip 	*/
#define DB_PORT			"5432"			/* DB host port */
#define DB_NAME			"xxxxxxxxxx"	/* DB name 		*/
#define DB_USER			"xxxxxxx"		/* DB user 		*/
#define DB_PASSWD		"xxxxxx"		/* DB passwd 	*/
#define DB_URL_LEN		100				/* DB url length 		*/
#define SQL_TEXT_LEN	100				/* SQL statement length */
#define	SP_TEXT_LEN		100				/* Stored Procedure text length */

/*==============================================================================
* Global variable declaration
*=============================================================================*/
extern URL_T 			 db_url;		/* db connection url */
extern ConnectionPool_T db_pool;		/* db connections pool */
extern Connection_T	 	db_conn;		/* db connection */

/*==============================================================================
* Function declaration
*=============================================================================*/
extern int8 DB_conn_start(URL_T* url_ptr, ConnectionPool_T* pool_ptr, Connection_T* conn_ptr);
extern void DB_conn_stop (URL_T* url_ptr, ConnectionPool_T* pool_ptr, Connection_T* conn_ptr);
extern int8 DB_func_exec (Connection_T* conn_ptr, cint8* func, cint8* arg, cint8* nm);

#endif

