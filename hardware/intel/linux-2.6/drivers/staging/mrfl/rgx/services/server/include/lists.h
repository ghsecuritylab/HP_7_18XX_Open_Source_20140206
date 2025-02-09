									    /*************************************************************************//*!
									       @File
									       @Title          Linked list shared functions templates.
									       @Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
									       @Description    Definition of the linked list function templates.
									       @License        Strictly Confidential.
    *//**************************************************************************/

#ifndef __LISTS_UTILS__
#define __LISTS_UTILS__

/* instruct QAC to ignore warnings about the following custom formatted macros */
/* PRQA S 0881,3410 ++ */
#include <stdarg.h>
#include "img_types.h"
#include "device.h"
#include "power.h"

/*
 - USAGE -

 The list functions work with any structure that provides the fields psNext and
 ppsThis. In order to make a function available for a given type, it is required
 to use the funcion template macro that creates the actual code.

 There are 4 main types of functions:
 - INSERT	: given a pointer to the head pointer of the list and a pointer to
 			  the node, inserts it as the new head.
 - REMOVE	: given a pointer to a node, removes it from its list.
 - FOR EACH	: apply a function over all the elements of a list.
 - ANY		: apply a function over the elements of a list, until one of them
 			  return a non null value, and then returns it.

 The two last functions can have a variable argument form, with allows to pass
 additional parameters to the callback function. In order to do this, the
 callback function must take two arguments, the first is the current node and
 the second is a list of variable arguments (va_list).

 The ANY functions have also another for wich specifies the return type of the
 callback function and the default value returned by the callback function.

*/

									    /*************************************************************************//*!
									       @Function       List_##TYPE##_ForEach
									       @Description    Apply a callback function to all the elements of a list.
									       @Input          psHead        The head of the list to be processed.
									       @Input          pfnCallBack   The function to be applied to each element of the list.
    *//**************************************************************************/
#define DECLARE_LIST_FOR_EACH(TYPE) \
IMG_VOID List_##TYPE##_ForEach(TYPE *psHead, IMG_VOID(*pfnCallBack)(TYPE* psNode))

#define IMPLEMENT_LIST_FOR_EACH(TYPE) \
IMG_VOID List_##TYPE##_ForEach(TYPE *psHead, IMG_VOID(*pfnCallBack)(TYPE* psNode))\
{\
	while(psHead)\
	{\
		pfnCallBack(psHead);\
		psHead = psHead->psNext;\
	}\
}

#define DECLARE_LIST_FOR_EACH_VA(TYPE) \
IMG_VOID List_##TYPE##_ForEach_va(TYPE *psHead, IMG_VOID(*pfnCallBack)(TYPE* psNode, va_list va), ...)

#define IMPLEMENT_LIST_FOR_EACH_VA(TYPE) \
IMG_VOID List_##TYPE##_ForEach_va(TYPE *psHead, IMG_VOID(*pfnCallBack)(TYPE* psNode, va_list va), ...) \
{\
	va_list ap;\
	while(psHead)\
	{\
		va_start(ap, pfnCallBack);\
		pfnCallBack(psHead, ap);\
		psHead = psHead->psNext;\
		va_end(ap);\
	}\
}

									    /*************************************************************************//*!
									       @Function       List_##TYPE##_Any
									       @Description    Applies a callback function to the elements of a list until the function
									       returns a non null value, then returns it.
									       @Input          psHead        The head of the list to be processed.
									       @Input          pfnCallBack   The function to be applied to each element of the list.
									       @Return         The first non null value returned by the callback function.
    *//**************************************************************************/
#define DECLARE_LIST_ANY(TYPE) \
IMG_VOID* List_##TYPE##_Any(TYPE *psHead, IMG_VOID* (*pfnCallBack)(TYPE* psNode))

#define IMPLEMENT_LIST_ANY(TYPE) \
IMG_VOID* List_##TYPE##_Any(TYPE *psHead, IMG_VOID* (*pfnCallBack)(TYPE* psNode))\
{ \
	IMG_VOID *pResult;\
	TYPE *psNextNode;\
	pResult = IMG_NULL;\
	psNextNode = psHead;\
	while(psHead && !pResult)\
	{\
		psNextNode = psNextNode->psNext;\
		pResult = pfnCallBack(psHead);\
		psHead = psNextNode;\
	}\
	return pResult;\
}

/*with variable arguments, that will be passed as a va_list to the callback function*/

#define DECLARE_LIST_ANY_VA(TYPE) \
IMG_VOID* List_##TYPE##_Any_va(TYPE *psHead, IMG_VOID*(*pfnCallBack)(TYPE* psNode, va_list va), ...)

#define IMPLEMENT_LIST_ANY_VA(TYPE) \
IMG_VOID* List_##TYPE##_Any_va(TYPE *psHead, IMG_VOID*(*pfnCallBack)(TYPE* psNode, va_list va), ...)\
{\
	va_list ap;\
	TYPE *psNextNode;\
	IMG_VOID* pResult = IMG_NULL;\
	while(psHead && !pResult)\
	{\
		psNextNode = psHead->psNext;\
		va_start(ap, pfnCallBack);\
		pResult = pfnCallBack(psHead, ap);\
		va_end(ap);\
		psHead = psNextNode;\
	}\
	return pResult;\
}

/*those ones are for extra type safety, so there's no need to use castings for the results*/

#define DECLARE_LIST_ANY_2(TYPE, RTYPE, CONTINUE) \
RTYPE List_##TYPE##_##RTYPE##_Any(TYPE *psHead, RTYPE (*pfnCallBack)(TYPE* psNode))

#define IMPLEMENT_LIST_ANY_2(TYPE, RTYPE, CONTINUE) \
RTYPE List_##TYPE##_##RTYPE##_Any(TYPE *psHead, RTYPE (*pfnCallBack)(TYPE* psNode))\
{ \
	RTYPE result;\
	TYPE *psNextNode;\
	result = CONTINUE;\
	psNextNode = psHead;\
	while(psHead && result == CONTINUE)\
	{\
		psNextNode = psNextNode->psNext;\
		result = pfnCallBack(psHead);\
		psHead = psNextNode;\
	}\
	return result;\
}

#define DECLARE_LIST_ANY_VA_2(TYPE, RTYPE, CONTINUE) \
RTYPE List_##TYPE##_##RTYPE##_Any_va(TYPE *psHead, RTYPE(*pfnCallBack)(TYPE* psNode, va_list va), ...)

#define IMPLEMENT_LIST_ANY_VA_2(TYPE, RTYPE, CONTINUE) \
RTYPE List_##TYPE##_##RTYPE##_Any_va(TYPE *psHead, RTYPE(*pfnCallBack)(TYPE* psNode, va_list va), ...)\
{\
	va_list ap;\
	TYPE *psNextNode;\
	RTYPE result = CONTINUE;\
	while(psHead && result == CONTINUE)\
	{\
		psNextNode = psHead->psNext;\
		va_start(ap, pfnCallBack);\
		result = pfnCallBack(psHead, ap);\
		va_end(ap);\
		psHead = psNextNode;\
	}\
	return result;\
}

									    /*************************************************************************//*!
									       @Function       List_##TYPE##_Remove
									       @Description    Removes a given node from the list.
									       @Input          psNode      The pointer to the node to be removed.
    *//**************************************************************************/
#define DECLARE_LIST_REMOVE(TYPE) \
IMG_VOID List_##TYPE##_Remove(TYPE *psNode)

#define IMPLEMENT_LIST_REMOVE(TYPE) \
IMG_VOID List_##TYPE##_Remove(TYPE *psNode)\
{\
	(*psNode->ppsThis)=psNode->psNext;\
	if(psNode->psNext)\
	{\
		psNode->psNext->ppsThis = psNode->ppsThis;\
	}\
}

									    /*************************************************************************//*!
									       @Function       List_##TYPE##_Insert
									       @Description    Inserts a given node at the beginnning of the list.
									       @Input          psHead   The pointer to the pointer to the head node.
									       @Input          psNode   The pointer to the node to be inserted.
    *//**************************************************************************/
#define DECLARE_LIST_INSERT(TYPE) \
IMG_VOID List_##TYPE##_Insert(TYPE **ppsHead, TYPE *psNewNode)

#define IMPLEMENT_LIST_INSERT(TYPE) \
IMG_VOID List_##TYPE##_Insert(TYPE **ppsHead, TYPE *psNewNode)\
{\
	psNewNode->ppsThis = ppsHead;\
	psNewNode->psNext = *ppsHead;\
	*ppsHead = psNewNode;\
	if(psNewNode->psNext)\
	{\
		psNewNode->psNext->ppsThis = &(psNewNode->psNext);\
	}\
}

									    /*************************************************************************//*!
									       @Function       List_##TYPE##_Reverse
									       @Description    Reverse a list in place
									       @Input          ppsHead    The pointer to the pointer to the head node.
    *//**************************************************************************/
#define DECLARE_LIST_REVERSE(TYPE) \
IMG_VOID List_##TYPE##_Reverse(TYPE **ppsHead)

#define IMPLEMENT_LIST_REVERSE(TYPE) \
IMG_VOID List_##TYPE##_Reverse(TYPE **ppsHead)\
{\
    TYPE *psTmpNode1; \
    TYPE *psTmpNode2; \
    TYPE *psCurNode; \
	psTmpNode1 = IMG_NULL; \
	psCurNode = *ppsHead; \
	while(psCurNode) { \
    	psTmpNode2 = psCurNode->psNext; \
        psCurNode->psNext = psTmpNode1; \
		psTmpNode1 = psCurNode; \
		psCurNode = psTmpNode2; \
		if(psCurNode) \
		{ \
			psTmpNode1->ppsThis = &(psCurNode->psNext); \
		} \
		else \
		{ \
			psTmpNode1->ppsThis = ppsHead;		\
		} \
	} \
	*ppsHead = psTmpNode1; \
}

#define IS_LAST_ELEMENT(x) ((x)->psNext == IMG_NULL)

DECLARE_LIST_ANY_2(PVRSRV_DEVICE_NODE, PVRSRV_ERROR, PVRSRV_OK);
DECLARE_LIST_ANY_VA(PVRSRV_DEVICE_NODE);
DECLARE_LIST_ANY_VA_2(PVRSRV_DEVICE_NODE, PVRSRV_ERROR, PVRSRV_OK);
DECLARE_LIST_FOR_EACH(PVRSRV_DEVICE_NODE);
DECLARE_LIST_FOR_EACH_VA(PVRSRV_DEVICE_NODE);
DECLARE_LIST_INSERT(PVRSRV_DEVICE_NODE);
DECLARE_LIST_REMOVE(PVRSRV_DEVICE_NODE);

DECLARE_LIST_ANY_VA(PVRSRV_POWER_DEV);
DECLARE_LIST_ANY_VA_2(PVRSRV_POWER_DEV, PVRSRV_ERROR, PVRSRV_OK);
DECLARE_LIST_INSERT(PVRSRV_POWER_DEV);
DECLARE_LIST_REMOVE(PVRSRV_POWER_DEV);

#undef DECLARE_LIST_ANY_2
#undef DECLARE_LIST_ANY_VA
#undef DECLARE_LIST_ANY_VA_2
#undef DECLARE_LIST_FOR_EACH
#undef DECLARE_LIST_FOR_EACH_VA
#undef DECLARE_LIST_INSERT
#undef DECLARE_LIST_REMOVE

IMG_VOID *MatchDeviceKM_AnyVaCb(PVRSRV_DEVICE_NODE * psDeviceNode, va_list va);
IMG_VOID *MatchPowerDeviceIndex_AnyVaCb(PVRSRV_POWER_DEV * psPowerDev,
					va_list va);

#endif

/* re-enable warnings */
/* PRQA S 0881,3410 -- */
