/* =============================================================================
* Filename : log.c
* Summary  : Start the log module
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
#include "log.h"

/*==============================================================================
* Global variable definition
*=============================================================================*/
zlog_category_t *	log_cat;			/* log category */
int8				log_run;			/* status of log module */

/*==============================================================================
* Name	 :	int8 	Log_start()
* Abstr	 :	Start the log module
* Params :	none
* Return :	int8 err :
*			EXEC_SUCS  : starting success
*			EXEC_FAIL  : starting failed
* Modify :
*=============================================================================*/
int8 Log_start()
{
	int8		err;

	err 	= EXEC_SUCS;
	log_cat = NULL;
	log_run = OFF;
	
	/* load the config file */
	err = zlog_init(LOG_CONFIG);
	if (err) {
		printf("zlog: config file load failed!\n");
		err = EXEC_FAIL;
	} else {
		log_run = ON;
	}

	/* get the log category */
	if (!err) {
		log_cat = zlog_get_category(LOG_CAT);
		if (log_cat == NULL) {
			printf("category get failed!\n");
			err = EXEC_FAIL;
		} else {
			log_debug(log_cat, "zlog: start success");
		}
	}
	
	return err;
}

