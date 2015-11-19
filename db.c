/* =============================================================================
* Filename : db.c
* Summary  : Start a db connection pool
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
#include "db.h"

/*==============================================================================
* Global variable definition
*=============================================================================*/
URL_T 			 db_url;		/* db connection url */
ConnectionPool_T db_pool;		/* db connections pool */
Connection_T	 db_conn;		/* db connection */

/*==============================================================================
* Name	 :	int8 DB_conn_start(URL_T* url_ptr, ConnectionPool_T* pool_ptr, 
*								 Connection_T* conn_ptr)
* Abstr	 :	Start the DB connection pool and get a db connection
* Params :	URL_T* 				url_ptr	 : db url pointer
*			ConnectionPool_T* 	pool_ptr : db pool pointer
*			Connection_T* 		conn_ptr : db connection pointer
* Return :	int8 err
*			EXEC_SUCS : starting success
*			EXEC_FAIL : starting failed
* Modify :
*=============================================================================*/
int8 DB_conn_start(URL_T* url_ptr, ConnectionPool_T* pool_ptr, Connection_T* conn_ptr)
{
	int8		err;
	cint8		url_t[DB_URL_LEN];	/* db conneciton url */

	err = EXEC_SUCS;
	*url_ptr  = NULL;
	*pool_ptr = NULL;
	*conn_ptr = NULL;
	memset(&url_t[0], 0x00, DB_URL_LEN);

	snprintf(&url_t[0], DB_URL_LEN, "%s://%s:%s/%s?user=%s&password=%s", 
			 DB_TYPE, DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWD);

	/* create a new db connection url */
	*url_ptr = URL_new(&url_t[0]);
	if (*url_ptr == NULL) {
		log_fatal(log_cat, "url: create failed");
		err = EXEC_FAIL;
	}
	
	/* start the db connection pool */
	if (!err) {
		*pool_ptr = ConnectionPool_new(*url_ptr);
		ConnectionPool_setInitialConnections(*pool_ptr, 1);
		ConnectionPool_setMaxConnections(*pool_ptr, 1);
		TRY {
			ConnectionPool_start(*pool_ptr);
		} CATCH (SQLException) {
			log_fatal(log_cat, "SQLException -> %s", Exception_frame.message);
			err = EXEC_FAIL;
		} END_TRY;
	}

	/* get a new db connection */
	if (!err) {
		*conn_ptr = ConnectionPool_getConnection(*pool_ptr);
		if (*conn_ptr == NULL) {
			log_fatal(log_cat, "db: get connection failed");
			err = EXEC_FAIL;
		}
	}

	if (!err) {
		log_debug(log_cat, "db: get connection success");
	}

	return err;
}

/*==============================================================================
* Name	 :	void DB_conn_stop(URL_T* url_ptr, ConnectionPool_T* pool_ptr, 
*							  Connection_T* conn_ptr)
* Abstr	 :	Stop the db connection and free resource
* Params :	URL_T* 				url_ptr	 : db url pointer
*			ConnectionPool_T* 	pool_ptr : db pool pointer
*			Connection_T* 		conn_ptr : db connection pointer
* Return :	
* Modify :
*=============================================================================*/
void DB_conn_stop(URL_T* url_ptr, ConnectionPool_T* pool_ptr, Connection_T* conn_ptr)
{
	if ((conn_ptr != NULL) && (*conn_ptr != NULL)) {
		Connection_close(*conn_ptr);
		log_debug(log_cat, "db conn: close success");
	}

	if ((pool_ptr != NULL) && (*pool_ptr != NULL)) {
		ConnectionPool_free(pool_ptr);
		log_debug(log_cat, "db pool: free success");
	}

	if ((url_ptr != NULL) && (*url_ptr != NULL)) {
		URL_free(url_ptr);
		log_debug(log_cat, "db  url: free success");
	}
}

/*==============================================================================
* Name	 :	int8 DB_func_exec(Connection_T* conn_ptr, cint8* func, 
*								cint8* arg, cint8* nm)
* Abstr	 :	Execute the db functions
* Params :	Connection_T* 	conn_ptr : db connection pointer
*			cint8* 		func 	 : function name
*			cint8* 		arg  	 : function argument
*			cint8* 		nm	 	 : caller name
* Return :	int8	  :
*			EXEC_SUCS : execute success
*			EXEC_FAIL : execute failed
* Modify :
*=============================================================================*/
int8 DB_func_exec(Connection_T* conn_ptr, cint8* func, cint8* arg, cint8* nm)
{
	int8		err;
	cint8		sp[SP_TEXT_LEN];	/* store process text */

	err = EXEC_SUCS;
	memset(&sp[0], 0x00, SP_TEXT_LEN);
	
	TRY {
		snprintf(&sp[0], SP_TEXT_LEN, "select * from %s(%s)", &func[0], &arg[0]);
		Connection_executeQuery(*conn_ptr, &sp[0]);
	} CATCH(SQLException) {
		log_fatal(log_cat, "%s: SQLException -> %s", &nm[0], Exception_frame.message);
		err = EXEC_FAIL;
	} END_TRY;

	if (!err) {
		log_debug(log_cat, "%s: sp exec success [%s] [arg=%s]", &nm[0], &func[0], &arg[0]);
	} else {
		log_fatal(log_cat, "%s: sp exec failed  [%s] [arg=%s]", &nm[0], &func[0], &arg[0]);
	}

	return err;
}



