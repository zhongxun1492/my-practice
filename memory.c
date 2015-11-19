/* =============================================================================
* Filename : memory.c
* Summary  : handle of memory
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
#include "memory.h"

/*==============================================================================
* Name	 :	void* safe_malloc(size_t size)
* Abstr	 :	malloc and initialize 
* Params :	size_t 	size : size of memory need
* Return :	void 	* 	 : pointer of the new memory
* Modify :
*=============================================================================*/
void* safe_malloc(size_t size)
{
	void* p;

	p = NULL;

	p = malloc(size);
	if (p == NULL) {
		log_fatal(log_cat, "malloc fault !!!");
		abort();
	} else {
		memset(p, 0x00, size);
	}

	return p;
}

/*==============================================================================
* Name	 :	void safe_free(void* p)
* Abstr	 :	Free memory
* Params :	void* p : pointer of memory want to free
* Return :	
* Modify :
*=============================================================================*/
void safe_free(void* p)
{
	if (p) {
		free(p);
		p = NULL;
	}
}


