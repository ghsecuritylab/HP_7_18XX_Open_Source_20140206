									    /*************************************************************************//*!
									       @File
									       @Title          Physcial heap management header
									       @Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
									       @Description    Defines the interface for the physical heap management
									       @License        Strictly Confidential.
    *//**************************************************************************/

#include "img_types.h"
#include "pvrsrv_error.h"

#ifndef _PHYSHEAP_H_
#define _PHYSHEAP_H_

typedef struct _PHYS_HEAP_ PHYS_HEAP;

typedef IMG_VOID(*CpuPAddrToDevPAddr) (IMG_HANDLE hPrivData,
				       IMG_DEV_PHYADDR * psDevPAddr,
				       IMG_CPU_PHYADDR * psCpuPAddr);

typedef IMG_VOID(*DevPAddrToCpuPAddr) (IMG_HANDLE hPrivData,
				       IMG_CPU_PHYADDR * psCpuPAddr,
				       IMG_DEV_PHYADDR * psDevPAddr);

typedef struct _PHYS_HEAP_FUNCTIONS_ {
	/*! Translate CPU physical address to device physical address */
	CpuPAddrToDevPAddr pfnCpuPAddrToDevPAddr;
	/*! Translate device physical address to CPU physical address */
	DevPAddrToCpuPAddr pfnDevPAddrToCpuPAddr;
} PHYS_HEAP_FUNCTIONS;

typedef enum _PHYS_HEAP_TYPE_ {
	PHYS_HEAP_TYPE_UNKNOWN = 0,
	PHYS_HEAP_TYPE_UMA,
	PHYS_HEAP_TYPE_LMA,
} PHYS_HEAP_TYPE;

typedef struct _PHYS_HEAP_CONFIG_ {
	IMG_UINT32 ui32PhysHeapID;
	PHYS_HEAP_TYPE eType;
	/*
	   Note:
	   sStartAddr and uiSize are only required for LMA heaps
	 */
	IMG_CPU_PHYADDR sStartAddr;
	IMG_UINT64 uiSize;
	IMG_CHAR *pszPDumpMemspaceName;
	PHYS_HEAP_FUNCTIONS *psMemFuncs;
	IMG_HANDLE hPrivData;
} PHYS_HEAP_CONFIG;

PVRSRV_ERROR PhysHeapRegister(PHYS_HEAP_CONFIG * psConfig,
			      PHYS_HEAP ** ppsPhysHeap);

IMG_VOID PhysHeapUnregister(PHYS_HEAP * psPhysHeap);

PVRSRV_ERROR PhysHeapAcquire(IMG_UINT32 ui32PhysHeapID,
			     PHYS_HEAP ** ppsPhysHeap);

IMG_VOID PhysHeapRelease(PHYS_HEAP * psPhysHeap);

PHYS_HEAP_TYPE PhysHeapGetType(PHYS_HEAP * psPhysHeap);

PVRSRV_ERROR PhysHeapGetAddress(PHYS_HEAP * psPhysHeap,
				IMG_CPU_PHYADDR * psCpuPAddr);

PHYS_HEAP_TYPE PhysHeapGetSize(PHYS_HEAP * psPhysHeap, IMG_UINT64 * puiSize);

IMG_VOID PhysHeapCpuPAddrToDevPAddr(PHYS_HEAP * psPhysHeap,
				    IMG_DEV_PHYADDR * psDevPAddr,
				    IMG_CPU_PHYADDR * psCpuPAddr);
IMG_VOID PhysHeapDevPAddrToCpuPAddr(PHYS_HEAP * psPhysHeap,
				    IMG_CPU_PHYADDR * psCpuPAddr,
				    IMG_DEV_PHYADDR * psDevPAddr);

IMG_CHAR *PhysHeapPDumpMemspaceName(PHYS_HEAP * psPhysHeap);

PVRSRV_ERROR PhysHeapInit(IMG_VOID);
PVRSRV_ERROR PhysHeapDeinit(IMG_VOID);

#endif				/* _PHYSHEAP_H_ */
