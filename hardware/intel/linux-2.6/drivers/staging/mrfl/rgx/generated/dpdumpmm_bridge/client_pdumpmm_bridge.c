									    /*************************************************************************//*!
									       @Title          Direct client bridge for pdumpmm
									       @Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
									       @License        Strictly Confidential.
    *//**************************************************************************/

#include "client_pdumpmm_bridge.h"
#include "img_defs.h"
#include "pvr_debug.h"

/* Module specific includes */
#include "pdump.h"
#include "pdumpdefs.h"
#include "pvrsrv_memallocflags.h"
#include "devicemem_typedefs.h"

#include "devicemem_server.h"
#include "pmr.h"
#include "physmem.h"

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgePMRPDumpLoadMem(IMG_HANDLE hBridge,
							     IMG_HANDLE hPMR,
							     IMG_DEVMEM_OFFSET_T
							     uiOffset,
							     IMG_DEVMEM_SIZE_T
							     uiSize,
							     IMG_UINT32
							     ui32PDumpFlags)
{
	PVRSRV_ERROR eError;
	PMR *psPMRInt;

	PVR_UNREFERENCED_PARAMETER(hBridge);

	psPMRInt = (PMR *) hPMR;

	eError = PMRPDumpLoadMem(psPMRInt, uiOffset, uiSize, ui32PDumpFlags);

	return eError;
}

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgePMRPDumpLoadMemValue(IMG_HANDLE
								  hBridge,
								  IMG_HANDLE
								  hPMR,
								  IMG_DEVMEM_OFFSET_T
								  uiOffset,
								  IMG_UINT32
								  ui32Value,
								  IMG_UINT32
								  ui32PDumpFlags)
{
	PVRSRV_ERROR eError;
	PMR *psPMRInt;

	PVR_UNREFERENCED_PARAMETER(hBridge);

	psPMRInt = (PMR *) hPMR;

	eError =
	    PMRPDumpLoadMemValue(psPMRInt, uiOffset, ui32Value, ui32PDumpFlags);

	return eError;
}

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgePMRPDumpSaveToFile(IMG_HANDLE
								hBridge,
								IMG_HANDLE hPMR,
								IMG_DEVMEM_OFFSET_T
								uiOffset,
								IMG_DEVMEM_SIZE_T
								uiSize,
								IMG_UINT32
								ui32ArraySize,
								const IMG_CHAR *
								puiFileName)
{
	PVRSRV_ERROR eError;
	PMR *psPMRInt;

	PVR_UNREFERENCED_PARAMETER(hBridge);

	psPMRInt = (PMR *) hPMR;

	eError =
	    PMRPDumpSaveToFile(psPMRInt,
			       uiOffset, uiSize, ui32ArraySize, puiFileName);

	return eError;
}

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgePMRPDumpSymbolicAddr(IMG_HANDLE
								  hBridge,
								  IMG_HANDLE
								  hPMR,
								  IMG_DEVMEM_OFFSET_T
								  uiOffset,
								  IMG_UINT32
								  ui32MemspaceNameLen,
								  IMG_CHAR *
								  puiMemspaceName,
								  IMG_UINT32
								  ui32SymbolicAddrLen,
								  IMG_CHAR *
								  puiSymbolicAddr,
								  IMG_DEVMEM_OFFSET_T
								  *
								  puiNewOffset,
								  IMG_DEVMEM_OFFSET_T
								  *
								  puiNextSymName)
{
	PVRSRV_ERROR eError;
	PMR *psPMRInt;

	PVR_UNREFERENCED_PARAMETER(hBridge);

	psPMRInt = (PMR *) hPMR;

	eError =
	    PMR_PDumpSymbolicAddr(psPMRInt,
				  uiOffset,
				  ui32MemspaceNameLen,
				  puiMemspaceName,
				  ui32SymbolicAddrLen,
				  puiSymbolicAddr,
				  puiNewOffset, puiNextSymName);

	return eError;
}

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgePMRPDumpPol32(IMG_HANDLE hBridge,
							   IMG_HANDLE hPMR,
							   IMG_DEVMEM_OFFSET_T
							   uiOffset,
							   IMG_UINT32 ui32Value,
							   IMG_UINT32 ui32Mask,
							   PDUMP_POLL_OPERATOR
							   eOperator,
							   IMG_UINT32
							   ui32PDumpFlags)
{
	PVRSRV_ERROR eError;
	PMR *psPMRInt;

	PVR_UNREFERENCED_PARAMETER(hBridge);

	psPMRInt = (PMR *) hPMR;

	eError =
	    PMRPDumpPol32(psPMRInt,
			  uiOffset,
			  ui32Value, ui32Mask, eOperator, ui32PDumpFlags);

	return eError;
}

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgePMRPDumpCBP(IMG_HANDLE hBridge,
							 IMG_HANDLE hPMR,
							 IMG_DEVMEM_OFFSET_T
							 uiReadOffset,
							 IMG_DEVMEM_OFFSET_T
							 uiWriteOffset,
							 IMG_DEVMEM_SIZE_T
							 uiPacketSize,
							 IMG_DEVMEM_SIZE_T
							 uiBufferSize)
{
	PVRSRV_ERROR eError;
	PMR *psPMRInt;

	PVR_UNREFERENCED_PARAMETER(hBridge);

	psPMRInt = (PMR *) hPMR;

	eError =
	    PMRPDumpCBP(psPMRInt,
			uiReadOffset,
			uiWriteOffset, uiPacketSize, uiBufferSize);

	return eError;
}

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV
BridgeDevmemIntPDumpSaveToFileVirtual(IMG_HANDLE hBridge,
				      IMG_HANDLE hDevmemServerContext,
				      IMG_DEV_VIRTADDR sAddress,
				      IMG_DEVMEM_SIZE_T uiSize,
				      IMG_UINT32 ui32ArraySize,
				      const IMG_CHAR * puiFileName,
				      IMG_UINT32 ui32FileOffset,
				      IMG_UINT32 ui32PDumpFlags)
{
	PVRSRV_ERROR eError;
	DEVMEMINT_CTX *psDevmemServerContextInt;

	PVR_UNREFERENCED_PARAMETER(hBridge);

	psDevmemServerContextInt = (DEVMEMINT_CTX *) hDevmemServerContext;

	eError =
	    DevmemIntPDumpSaveToFileVirtual(psDevmemServerContextInt,
					    sAddress,
					    uiSize,
					    ui32ArraySize,
					    puiFileName,
					    ui32FileOffset, ui32PDumpFlags);

	return eError;
}
