/******************************************************************************
 * Name         : devicemem_mmap.c
 * Title        : Device Memory Management
 * Author(s)    : Imagination Technologies
 * Created      : 
 *
 * Copyright    : 2010 by Imagination Technologies Limited.
 *                All rights reserved. No part of this software, either
 *                material or conceptual may be copied or distributed,
 *                transmitted, transcribed, stored in a retrieval system or
 *                translated into any human or computer language in any form
 *                by any means, electronic, mechanical, manual or otherwise,
 *                or disclosed to third parties without the express written
 *                permission of Imagination Technologies Limited,
 *                Home Park Estate, Kings Langley, Hertfordshire,
 *                WD4 8LZ, U.K.
 *
 * Description  : OS abstraction for the mmap2 interface for mapping PMRs into
 *                User Mode memory
 *
 * Platform     : ALL
 *
 *****************************************************************************/

/* our exported API */
#include "devicemem_mmap.h"

/* include/ */
#include "img_types.h"
#include "pvr_debug.h"
#include "pvrsrv_error.h"

/* services/include/ */

/* services/include/srvhelper/ */
#include "ra.h"

/* autogenerated bridge */
#include "client_mm_bridge.h"

/* FIXME: Direct access to PMR bypassing bridge... */
#include "pmr.h"

IMG_INTERNAL PVRSRV_ERROR
OSMMapPMR(IMG_HANDLE hBridge,
	  IMG_HANDLE hPMR,
	  IMG_DEVMEM_SIZE_T uiPMRSize,
	  IMG_HANDLE * phOSMMapPrivDataOut,
	  IMG_VOID ** ppvMappingAddressOut, IMG_SIZE_T * puiMappingLengthOut)
{
	PVRSRV_ERROR eError;
	PMR *psPMR;
	IMG_VOID *pvKernelAddress;
	IMG_SIZE_T uiLength;
	IMG_HANDLE hPriv;

	PVR_UNREFERENCED_PARAMETER(hBridge);

	/*
	   Normally this function would mmap a PMR into the memory space of
	   user process, but in this case we're taking a PMR and mapping it
	   into kernel virtual space.  We keep the same function name for
	   symmetry as this allows the higher layers of the software stack
	   to not care whether they are user mode or kernel
	 */

	psPMR = hPMR;

	/*
	   TODO:

	   Since we know we're in kernel, we can sidestep the bridge.
	   We should "direct" bridge this function?  We certainly don't want
	   to put it in the "normal" bridge, at least not without making it
	   secure somehow.
	   We can't afford for user mode processes to learn about kernel
	   virtual addresses (although, one may argue it won't cause any
	   _real_ harm)
	 */
	eError = PMRAcquireKernelMappingData(psPMR,
					     0,
					     0,
					     &pvKernelAddress,
					     &uiLength, &hPriv);
	if (eError != PVRSRV_OK) {
		goto e0;
	}

	*phOSMMapPrivDataOut = hPriv;
	*ppvMappingAddressOut = pvKernelAddress;
	*puiMappingLengthOut = uiLength;

	PVR_ASSERT(*puiMappingLengthOut == uiPMRSize);

	return PVRSRV_OK;

	/*
	   error exit paths follow
	 */

 e0:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

IMG_INTERNAL IMG_VOID
OSMUnmapPMR(IMG_HANDLE hBridge,
	    IMG_HANDLE hPMR,
	    IMG_HANDLE hOSMMapPrivData,
	    IMG_VOID * pvMappingAddress, IMG_SIZE_T uiMappingLength)
{
	PMR *psPMR;

	PVR_UNREFERENCED_PARAMETER(hBridge);
	PVR_UNREFERENCED_PARAMETER(pvMappingAddress);
	PVR_UNREFERENCED_PARAMETER(uiMappingLength);

	psPMR = hPMR;
	PMRReleaseKernelMappingData(psPMR, hOSMMapPrivData);
}
