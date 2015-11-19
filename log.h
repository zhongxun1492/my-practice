/* =============================================================================
* Filename : log.h
* Summary  : Redefinitions of the interface in zlog
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

#ifndef __LOG_H__
#define __LOG_H__

/*==============================================================================
* Header files
*=============================================================================*/
#include "api.h"
#include "zlog.h"

/*==============================================================================
* Macro Definition
*=============================================================================*/
#define LOG_CAT			"my_cat"		/* log category */
#define LOG_CONFIG		"zlog.conf"		/* config file  */

/* redefinitions of interface */
#define log_debug(cat, ...) 	zlog_debug(cat,  ##__VA_ARGS__)
#define log_info(cat, ...) 		zlog_info(cat,   ##__VA_ARGS__)
#define log_notice(cat, ...) 	zlog_notice(cat, ##__VA_ARGS__)
#define log_warn(cat, ...) 		zlog_warn(cat,   ##__VA_ARGS__)
#define log_error(cat, ...) 	zlog_error(cat,  ##__VA_ARGS__)
#define log_fatal(cat, ...) 	zlog_fatal(cat,  ##__VA_ARGS__)
#define Log_free() 				zlog_fini()

/*==============================================================================
* Global variable declaration
*=============================================================================*/
extern	zlog_category_t *	log_cat;	/* variable of log category */
extern	int8				log_run;	/* status of log module 	*/

/*==============================================================================
* Function declaration
*=============================================================================*/
extern int8 Log_start();

#endif

