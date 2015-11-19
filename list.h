/* =============================================================================
* Filename : list.h
* Summary  : Interface of link list
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

#ifndef __LIST_H__
#define __LIST_H__

/*==============================================================================
* Header files
*=============================================================================*/
#include "api.h"
#include "log.h"
#include "memory.h"

/*==============================================================================
* Struct definition
*=============================================================================*/
/* single node in the linked list */
typedef struct _list_node {
	struct _list_node *		last;		/* pointer of last node */
	struct _list_node *		next;		/* pointer of next node */
	void *					data;		/* pointer of real data */
} LIST_NODE;

/* struct of a linked list */
typedef struct _link_list {
	struct _list_node *		head; 		/* point to the head node */
	struct _list_node *		tail; 		/* point to the tail node */
	uint32 					cur_num; 	/* current num of list	  */
//	uint16 					max_num; 	/* maximum num of list	  */
} LINK_LIST;

/*==============================================================================
* Function declaration
*=============================================================================*/
extern LINK_LIST* List_new();
extern void List_free(LINK_LIST* list);
extern int8 List_isempty(LINK_LIST* list);
extern uint32 List_num(LINK_LIST* list);
extern void* List_node_pop(LINK_LIST* list);
extern void  List_node_add(LINK_LIST* list, void* data);
extern void  List_node_del(LINK_LIST* list, void* data);

#endif

