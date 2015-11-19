/* =============================================================================
* Filename : list.c
* Summary  : Processing of link list
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
#include "list.h"

/*==============================================================================
* Interface declarations
*=============================================================================*/
static void List_clear(LINK_LIST* list);

#if 0
#endif

/*==============================================================================
* Name	 :	LINK_LIST* List_new()
* Abstr	 :	Create a new link list		
* Params :	
* Return :	LINK_LIST* : a new link list
* Modify :	
*=============================================================================*/
LINK_LIST* List_new()
{
	LINK_LIST*	new_list;

	new_list = NULL;

	new_list = (LINK_LIST*)safe_malloc(sizeof(LINK_LIST));
	new_list->head = NULL;
	new_list->tail = NULL;
	new_list->cur_num = 0;

	log_debug(log_cat, "list: new success");

	return new_list;
}

/*==============================================================================
* Name	 :	void List_clear(LINK_LIST* list)
* Abstr	 :	Clear the link list
* Params :	LINK_LIST* 	list : the link list need to clear
* Return :	
* Modify :	
*=============================================================================*/
static void List_clear(LINK_LIST* list)
{
	LIST_NODE* 	sig_node;

	sig_node = NULL;
	
	while ((sig_node = list->head)) {
		
		list->head = sig_node->next;
		
		if (list->head != NULL) {
			list->head->last = NULL;
		} else {
			list->tail = NULL;
		}
		
		sig_node->last = sig_node->next = NULL;
		
		safe_free((void*)sig_node);
		
		list->cur_num--;
	}

	log_debug(log_cat, "list: clear success");
}

/*==============================================================================
* Name	 :	void List_free(LINK_LIST* list)
* Abstr	 :	Free memory of the link list
* Params :	LINK_LIST* 	list : the link list need to free
* Return :	
* Modify :	
*=============================================================================*/
void List_free(LINK_LIST* list)
{
	/* clear the list */
	List_clear(list);

	/* free handle */
	safe_free((void*)list);

	log_debug(log_cat, "list: free success");
}

/*==============================================================================
* Name	 :	int8 List_isempty(LINK_LIST* list)
* Abstr	 :	Judge the link list is empty or not
* Params :	LINK_LIST* 	list : the link list
* Return :	int8 	:
*			TRUE	: is empty
*			FALSE	: not empty
* Modify :	
*=============================================================================*/
int8 List_isempty(LINK_LIST* list)
{
	return (list->cur_num ? FALSE : TRUE);
}

/*==============================================================================
* Name	 :	uint32 List_num(LINK_LIST* list)
* Abstr	 :	Return the num of nodes in current link list
* Params :	LINK_LIST* 	list : the link list
* Return :	uint32 : num of nodes
* Modify :	
*=============================================================================*/
uint32 List_num(LINK_LIST* list)
{
	return list->cur_num;
}

/*==============================================================================
* Name	 :	void List_node_add(LINK_LIST* list, void* data)
* Abstr	 :	Add a new node to the link list and bind the data field
* Params :	LINK_LIST* 	list : the link list
*			void* 		data : data field
* Return :	
* Modify :	
*=============================================================================*/
void List_node_add(LINK_LIST* list, void* data)
{
	LIST_NODE* new_node;
	
	new_node = NULL;

	new_node = (LIST_NODE*)safe_malloc(sizeof(LIST_NODE));
	new_node->last = NULL;
	new_node->next = NULL;
	new_node->data = data;

	/* list is empty, first node */
	if (list->head == NULL) {
		list->head = list->tail = new_node;
	/* list is not empty, general node */
	} else {
		list->tail->next = new_node;
		new_node->last = list->tail;
		list->tail = new_node;
	}

	list->cur_num++;

	//log_debug(log_cat, "list: add new node success");
}

/*==============================================================================
* Name	 :	void* List_node_pop(LINK_LIST* list)
* Abstr	 :	Pop the first node from the link list and free its memory
* Params :	LINK_LIST* 	list : the link list
* Return :	void *	: data field of the pop node
* Modify :	
*=============================================================================*/
void* List_node_pop(LINK_LIST* list)
{
	LIST_NODE* 	pop_node;	/* pop node */
	void*		data;		/* data of the pop node */

	data 	 = NULL;
	pop_node = NULL;
	
	if (list->cur_num <= 0) {
		log_warn(log_cat, "list: is empty");
	} else {
		pop_node = list->head;
		list->head = pop_node->next;
		
		/* pop node is the last */
		if (list->head == NULL) {
			list->tail = NULL;
		/* pop node is not last */
		} else {
			list->head->last = NULL;
		}
		/* free handle */
		list->cur_num--;
		data = pop_node->data;
		safe_free((void*)pop_node);
		//log_debug(log_cat, "list: pop head node success");
	}

	return data;
}

/*==============================================================================
* Name	 :	void List_node_del(LINK_LIST* list, void* data)
* Abstr	 :	Delete a node in the link list
* Params :	LINK_LIST* 	list : the link list
*			void *		data : data field of the delete node
* Return :	
* Modify :	
*=============================================================================*/
void List_node_del(LINK_LIST* list, void* data)
{
	LIST_NODE* 	search_node;	/* search node */
	
	search_node = list->head;

	while (search_node) {
		/* current node is target */
		if (search_node->data == data) {
			/* only one node */
			if (list->cur_num <= 1) {	
				list->head = list->tail = NULL;
			/* more than one node */
			} else {
				/* target is head */
				if (search_node == list->head) {
					list->head = search_node->next;
					list->head->last = NULL;
				/* target is tail */
				} else if (search_node == list->tail) {
					list->tail = search_node->last;
					list->tail->next = NULL;
				/* target is middle */
				} else {
					search_node->last->next = search_node->next;
					search_node->next->last = search_node->last;
				}
			}
			/* free handle */
			search_node->last = search_node->next = NULL;
			safe_free((void*)search_node);
			list->cur_num--;
			//log_debug(log_cat, "list: delete node success");
			break;
		/* current node is not target */
		} else {
			search_node = search_node->next;
		}
	}
	
}



