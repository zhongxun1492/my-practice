/* =============================================================================
* Filename : api.h
* Summary  : some definitions of common interface 
* Compiler : gcc
*
* Version  : 
* Update   : 
* Date     : 
* Author   : 
*
* History  :
*
* ============================================================================*/

#ifndef __API_H__
#define __API_H__

/*==============================================================================
* Header files
*=============================================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

/*==============================================================================
* Macro Definition
*=============================================================================*/
#define ON				(1)			/* flag is true  */
#define OFF				(0)			/* flag is false */
#define EXEC_SUCS		(0)			/* function execute success */
#define EXEC_FAIL		(-1)		/* function execute failed 	*/
#define FALSE			(0)			/* boolean : false 	*/
#define TRUE			(1)			/* boolean : true 	*/

/* find the whole struct by one pointer of the members 	*/
#define container_of(ptr, type, member) ({             		\
		const typeof(((type *)0)->member) *__mptr = (ptr);	\
		(type *)((char *)__mptr - offsetof(type, member));})
						
/* get the offset of address for a member in the struct */	
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE*)0)->MEMBER)

#define container_of(ptr,type,member) ({ \
	const typeof(((type*)0)->member) *__mptr = (ptr); \
	(type*)((char*)__mptr - offsetof(type, member));})


/*==============================================================================
* Struct definition
*=============================================================================*/
/* definition of data type  */
typedef signed 		char    int8;
typedef unsigned 	char  	uint8;
typedef  			char	cint8;
typedef signed 		short   int16;
typedef unsigned 	short 	uint16;
typedef signed 		int		int32;
typedef unsigned 	int   	uint32;

#endif


