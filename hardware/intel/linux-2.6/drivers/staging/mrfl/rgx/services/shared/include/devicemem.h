									     /**************************************************************************//*!
									        @File           devicemem.h
									        @Title          Device Memory Management core internal
									        @Author         Copyright (C) Imagination Technologies Limited.
									        All rights reserved. Strictly Confidential.
									        @Description    Services internal interface to core device memory management
									        functions that are shared between client and server code.
    *//***************************************************************************/

#ifndef SRVCLIENT_DEVICEMEM_H
#define SRVCLIENT_DEVICEMEM_H

/********************************************************************************
 *                                                                              *
 *   +------------+   +------------+    +--------------+      +--------------+  *
 *   | a   sub-   |   | a   sub-   |    |  an          |      | allocation   |  *
 *   | allocation |   | allocation |    |  allocation  |      | also mapped  |  *
 *   |            |   |            |    |  in proc 1   |      | into proc 2  |  *
 *   +------------+   +------------+    +--------------+      +--------------+  *
 *             |         |                     |                     |          *
 *          +--------------+            +--------------+      +--------------+  *
 *          | page   gran- |            | page   gran- |      | page   gran- |  *
 *          | ular mapping |            | ular mapping |      | ular mapping |  *
 *          +--------------+            +--------------+      +--------------+  *
 *                 |                                 |          |               *
 *                 |                                 |          |               *
 *                 |                                 |          |               *
 *          +--------------+                       +--------------+             *
 *          |              |                       |              |             *
 *          | A  "P.M.R."  |                       | A  "P.M.R."  |             *
 *          |              |                       |              |             *
 *          +--------------+                       +--------------+             *
 *                                                                              *
 ********************************************************************************/

/*
    All device memory allocations are ultimately a view upon (not
    necessarily the whole of) a "PMR".

    A PMR is a "Physical Memory Resource", which may be a
    "pre-faulted" lump of physical memory, or it may be a
    representation of some physical memory that will be instantiated
    at some future time.

    PMRs always represent multiple of some power-of-2 "contiguity"
    promised by the PMR, which will allow them to be mapped in whole
    pages into the device MMU.  As memory allocations may be smaller
    than a page, these mappings may be suballocated and thus shared
    between multiple allocations in one process.  A PMR may also be
    mapped simultaneously into multiple device memory contexts
    (cross-process scenario), however, for security reasons, it is not
    legal to share a PMR "both ways" at once, that is, mapped into
    multiple processes and divided up amongst several suballocations.

    This PMR terminology is introduced here for background
    information, but is generally of little concern to the caller of
    this API.  This API handles suballocations and mappings, and the
    caller thus dealy primarily with MEMORY DESCRIPTORS representing
    an allocation or suballocation, HEAPS representing ranges of
    virtual addresses in a CONTEXT.
*/

/*
   |<---------------------------context------------------------------>|
   |<-------heap------->|   |<-------heap------->|<-------heap------->|
   |<-alloc->|          |   |<-alloc->|<-alloc->||   |<-alloc->|      |
*/

#include "img_types.h"
#include "devicemem_typedefs.h"
#include "pdumpdefs.h"
#include "pvrsrv_error.h"
#include "pvrsrv_memallocflags.h"

#include "pdump.h"

typedef IMG_UINT32 DEVMEM_HEAPCFGID;
#define DEVMEM_HEAPCFG_FORCLIENTS 0
#define DEVMEM_HEAPCFG_META 1
/* FIXME: For now, PMMIF uses the same heap config as normal clients */
#define DEVMEM_HEAPCFG_PMMIF 0

/*
  In order to call the server side functions, we need a bridge handle.
  We abstract that here, as we may wish to change its form.
 */

typedef IMG_HANDLE DEVMEM_BRIDGE_HANDLE;

/*
 * DevmemCreateContext()
 *
 * Create a device memory context
 *
 * This must be called before any heap is created in this context
 *
 * Caller to provide bridge handle which will be squirreled away
 * internally and used for all future operations on items from this
 * memory context.  Caller also to provide devicenode handle, as this
 * is used for MMU configuration and also to determine the heap
 * configuration for the auto-instantiated heaps.
 *
 * Note that when compiled in services/server, the hBridge is not used
 * and is thrown away by the "fake" direct bridge.  (This may change.
 * It is recommended that IMG_NULL be passed for the handle for now)
 *
 * hDeviceNode and uiHeapBlueprintID shall together dictate which
 * heap-config to use.
 *
 * This will cause the server side counterpart to be created also
 * (with appropriate resman stuff where applicable)
 *
 * If you call DevmemCreateContext() (and the call succeeds) you
 * are promising that you will later call Devmem_ContextDestroy(),
 * except for abnormal process termination in which case it is
 * expected that ResMan will do this on your behalf.
 *
 * Caller to provide storage for the pointer to the NEWDEVMEM_CONTEXT
 * object thusly created.
 */
extern PVRSRV_ERROR
DevmemCreateContext(DEVMEM_BRIDGE_HANDLE hBridge,
		    IMG_HANDLE hDeviceNode,
		    DEVMEM_HEAPCFGID uiHeapBlueprintID,
		    DEVMEM_CONTEXT ** ppsCtxPtr);

/*
 * DevmemAcquireDevPrivData()
 * 
 * Acquire the device private data for this memory context
 */
PVRSRV_ERROR
DevmemAcquireDevPrivData(DEVMEM_CONTEXT * psCtx, IMG_HANDLE * hPrivData);

/*
 * DevmemReleaseDevPrivData()
 * 
 * Release the device private data for this memory context
 */
PVRSRV_ERROR DevmemReleaseDevPrivData(DEVMEM_CONTEXT * psCtx);

/*
 * DevmemDestroyContext()
 *
 * Undoes that done by DevmemCreateContext()
 */
extern PVRSRV_ERROR DevmemDestroyContext(DEVMEM_CONTEXT * psCtx);

/*
 * DevmemCreateHeap()
 *
 * Create a heap in the given context.
 *
 * N.B.  Not intended to be called directly, though it can be.
 * Normally, heaps are instantiated at context creation time according
 * to the specified blueprint.  See DevmemCreateContext() for details.
 *
 * This will cause MMU code to set up data structures for the heap,
 * but may not cause page tables to be modified until allocations are
 * made from the heap.
 *
 * The "Quantum" is both the device MMU page size to be configured for
 * this heap, and the unit multiples of which "quantized" allocations
 * are made (allocations smaller than this, known as "suballocations"
 * will be made from a "sub alloc RA" and will "import" chunks
 * according to this quantum)
 *
 * TODO: decide whether there's a need to distinguish between the
 * quantum and the device mmu page size?  I think not.
 *
 * Where imported PMRs (or, for example, PMRs created by device class
 * buffers) are mapped into this heap, it is important that the
 * physical contiguity guarantee offered by the PMR is greater than or
 * equal to the quantum size specified here, otherwise the attempt to
 * map it will fail.  "Normal" allocations via Devmem_Allocate
 * shall automatically meet this requirement, as each "import" will
 * trigger the creation of a PMR with the desired contiguity.  The
 * supported quantum sizes in that case shall be dictated by the OS
 * specific implementation of PhysmemNewOSRamBackedPMR() (see)
 */
extern PVRSRV_ERROR DevmemCreateHeap(DEVMEM_CONTEXT * psCtxPtr,
				     /* base and length of heap */
				     IMG_DEV_VIRTADDR sBaseAddress,
				     IMG_DEVMEM_SIZE_T uiLength,
				     /* log2 of allocation quantum, i.e. "page" size.
				        All allocations (that go to server side) are
				        multiples of this.  We use a client-side RA to
				        make sub-allocations from this */
				     IMG_UINT32 ui32Log2Quantum,
				     /* Name of heap for debug */
				     /* N.B.  Okay to exist on caller's stack - this
				        func takes a copy if it needs it. */
				     const IMG_CHAR * pszName,
				     DEVMEM_HEAP ** ppsHeapPtr);
/*
 * DevmemDestroyHeap()
 *
 * Reverses DevmemCreateHeap()
 *
 * N.B. All allocations must have been freed and all mappings must
 * have been unmapped before invoking this call
 */
extern PVRSRV_ERROR DevmemDestroyHeap(DEVMEM_HEAP * psHeap);

/*
 * DevmemExportalignAdjustSizeAndAlign()
 * Compute the Size and Align passed to avoid suballocations (used when allocation with PVRSRV_MEMALLOCFLAG_EXPORTALIGN)
 */
IMG_INTERNAL IMG_VOID
DevmemExportalignAdjustSizeAndAlign(DEVMEM_HEAP * psHeap,
				    IMG_DEVMEM_SIZE_T * puiSize,
				    IMG_DEVMEM_ALIGN_T * puiAlign);

/*
 * DevmemAllocate()
 *
 * Makes an allocation (possibly a "suballocation", as described
 * below) of device virtual memory from this heap.
 *
 * The size and alignment of the allocation will be honoured by the RA
 * that allocates the "suballocation".  The resulting allocation will
 * be mapped into GPU virtual memory and the physical memory to back
 * it will exist, by the time this call successfully completes.
 * 
 * TODO: The mapping of the PMR to the GPU is optional and controlled
 * by the flags passed in. This leads to the interesting question as
 * to if the allocation requests a GPU mapping should we map it in here
 * or do that as a 2nd to DevmemAcquireDevVirtAddr request same way we
 * do for CPU mapping.
 *
 * The size must be a positive integer multiple of the alignment.
 * (i.e. the aligment specifies the alignment of both the start and
 * the end of the resulting allocation.)
 *
 * Allocations made via this API are routed though a "suballocation
 * RA" which is responsible for ensuring that small allocations can be
 * made without wasting physical memory in the server.  Furthermore,
 * such suballocations can be made entirely client side without
 * needing to go to the server unless the allocation spills into a new
 * page.
 *
 * Such suballocations cause many allocations to share the same "PMR".
 * This happens only when the flags match exactly.
 *
 * Note that we don't currently have a flag to determine that an
 * allocation is to be marked as "exportable" - TODO: should we have
 * such a flag?  In general it is assumed that the caller may make
 * small allocations (suballocations) _or_ export allocations to other
 * processes but not both.
 */

PVRSRV_ERROR DevmemAllocate(DEVMEM_HEAP * psHeap,
			    IMG_DEVMEM_SIZE_T uiSize,
			    IMG_DEVMEM_ALIGN_T uiAlign,
			    DEVMEM_FLAGS_T uiFlags,
			    DEVMEM_MEMDESC ** ppsMemDescPtr);

PVRSRV_ERROR
DevmemAllocateExportable(IMG_HANDLE hBridge,
			 IMG_HANDLE hDeviceNode,
			 IMG_DEVMEM_SIZE_T uiSize,
			 IMG_DEVMEM_ALIGN_T uiAlign,
			 DEVMEM_FLAGS_T uiFlags,
			 DEVMEM_MEMDESC ** ppsMemDescPtr);

/*
 * DevmemFree()
 *
 * Reverses that done by DevmemAllocate() N.B.  The underlying
 * mapping and server side allocation _may_ not be torn down, for
 * example, if the allocation has been exported, or if multiple
 * allocations were suballocated from the same mapping, but this is
 * properly refcounted, so the caller does not have to care.
 */

extern IMG_VOID DevmemFree(DEVMEM_MEMDESC * psMemDesc);

/*
	DevmemMapToDevice:

	Map an allocation to the device is was allocated from.
	This function _must_ be called before any call to 
	DevmemAcquireDevVirtAddr is made as it binds the allocation
	to the heap.
	DevmemReleaseDevVirtAddr is used to release the reference
	to the device mapping this function created, but it doesn't
	mean that the memory will actually be unmapped from the
	device as other references to the mapping abtained via
	DevmemAcquireDevVirtAddr could still be active.
*/
PVRSRV_ERROR DevmemMapToDevice(DEVMEM_MEMDESC * psMemDesc,
			       DEVMEM_HEAP * psHeap,
			       IMG_DEV_VIRTADDR * psDevVirtAddr);

/*
	DevmemAcquireDevVirtAddr

	Acquire the MemDesc's device virtual address.
	This function _must_ be called after DevmemMapToDevice
	and is expected to be used be functions which didn't allocate
	the MemDesc but need to know it's address
 */
PVRSRV_ERROR DevmemAcquireDevVirtAddr(DEVMEM_MEMDESC * psMemDesc,
				      IMG_DEV_VIRTADDR * psDevVirtAddrRet);
/*
 * DevmemReleaseDevVirtAddr()
 *
 * give up the licence to use the device virtual address that was
 * acquired by "Acquire" or "MapToDevice"
 */
extern IMG_VOID DevmemReleaseDevVirtAddr(DEVMEM_MEMDESC * psMemDesc);

/*
 * DevmemAcquireCpuVirtAddr()
 *
 * Acquires a license to use the cpu virtual address of this mapping.
 * Note that the memory may not have been mapped into cpu virtual
 * memory prior to this call.  On first "acquire" the memory will be
 * mapped in (if it wasn't statically mapped in) and on last put it
 * _may_ become unmapped.  Later calling "Acquire" again, _may_ cause
 * the memory to be mapped at a different address.
 */
PVRSRV_ERROR DevmemAcquireCpuVirtAddr(DEVMEM_MEMDESC * psMemDesc,
				      IMG_VOID ** ppvCpuVirtAddr);
/*
 * DevmemReleaseDevVirtAddr()
 *
 * give up the licence to use the cpu virtual address that was granted
 * with the "Get" call.
 */
extern IMG_VOID DevmemReleaseCpuVirtAddr(DEVMEM_MEMDESC * psMemDesc);

/*
 * DevmemExport()
 *
 * Given a memory allocation allocated with Devmem_Allocate() (N.B.
 * mustn't be a suballocated one, for security reasons... TODO:
 * enforce by flag, perhaps?) create a "cookie" that can be passed
 * intact by the caller's own choice of secure IPC to another process
 * and used as the argument to "map" to map this memory into a heap in
 * the target processes.  N.B.  This can also be used to map into
 * multiple heaps in one process, though that's not the intention.
 *
 * Note, the caller must later call Unexport before freeing the
 * memory.
 */
PVRSRV_ERROR DevmemExport(DEVMEM_MEMDESC * psMemDesc,
			  DEVMEM_EXPORTCOOKIE * psExportCookie);

IMG_VOID DevmemUnexport(DEVMEM_MEMDESC * psMemDesc,
			DEVMEM_EXPORTCOOKIE * psExportCookie);

PVRSRV_ERROR
DevmemImport(IMG_HANDLE hBridge,
	     DEVMEM_EXPORTCOOKIE * psCookie,
	     DEVMEM_FLAGS_T uiFlags, DEVMEM_MEMDESC ** ppsMemDescPtr);

/*
 * DevmemIsValidExportCookie()
 * Check whether the Export Cookie contains a valid export */
IMG_BOOL DevmemIsValidExportCookie(DEVMEM_EXPORTCOOKIE * psExportCookie);

/*
 * DevmemMakeServerExportClientExport()
 * 
 * This is a "special case" function for making a server export cookie
 * which went through the direct bridge into an export cookie that can
 * be passed through the client bridge.
 */
PVRSRV_ERROR
DevmemMakeServerExportClientExport(DEVMEM_BRIDGE_HANDLE hBridge,
				   DEVMEM_SERVER_EXPORTCOOKIE
				   hServerExportCookie,
				   DEVMEM_EXPORTCOOKIE * psExportCookie);

/*
 * DevmemUnmakeServerExportClientExport()
 * 
 * Free any resource associated with the Make operation
 */
PVRSRV_ERROR
DevmemUnmakeServerExportClientExport(DEVMEM_BRIDGE_HANDLE hBridge,
				     DEVMEM_EXPORTCOOKIE * psExportCookie);

/*
 *
 * The following set of functions is specific to the heap "blueprint"
 * stuff, for automatic creation of heaps when a context is created
 *
 */

/* Devmem_HeapConfigCount: returns the number of heap configs that
   this device has.  Note that there is no acquire/release semantics
   required, as this data is guaranteed to be constant for the
   lifetime of the device node */
extern PVRSRV_ERROR
DevmemHeapConfigCount(DEVMEM_BRIDGE_HANDLE hBridge,
		      IMG_HANDLE hDeviceNode,
		      IMG_UINT32 * puiNumHeapConfigsOut);

/* Devmem_HeapCount: returns the number of heaps that a given heap
   config on this device has.  Note that there is no acquire/release
   semantics required, as this data is guaranteed to be constant for
   the lifetime of the device node */
extern PVRSRV_ERROR
DevmemHeapCount(DEVMEM_BRIDGE_HANDLE hBridge,
		IMG_HANDLE hDeviceNode,
		IMG_UINT32 uiHeapConfigIndex, IMG_UINT32 * puiNumHeapsOut);
/* Devmem_HeapConfigName: return the name of the given heap config.
   The caller is to provide the storage for the returned string and
   indicate the number of bytes (including null terminator) for such
   string in the BufSz arg.  Note that there is no acquire/release
   semantics required, as this data is guaranteed to be constant for
   the lifetime of the device node.
 */
extern PVRSRV_ERROR
DevmemHeapConfigName(DEVMEM_BRIDGE_HANDLE hBridge,
		     IMG_HANDLE hDeviceNode,
		     IMG_UINT32 uiHeapConfigIndex,
		     IMG_CHAR * pszConfigNameOut, IMG_UINT32 uiConfigNameBufSz);

/* Devmem_HeapDetails: fetches all the metadata that is recorded in
   this heap "blueprint".  Namely: heap name (caller to provide
   storage, and indicate buffer size (including null terminator) in
   BufSz arg), device virtual address and length, log2 of data page
   size (will be one of 12, 14, 16, 18, 20, 21, at time of writing).
   Note that there is no acquire/release semantics required, as this
   data is guaranteed to be constant for the lifetime of the device
   node. */
extern PVRSRV_ERROR
DevmemHeapDetails(DEVMEM_BRIDGE_HANDLE hBridge,
		  IMG_HANDLE hDeviceNode,
		  IMG_UINT32 uiHeapConfigIndex,
		  IMG_UINT32 uiHeapIndex,
		  IMG_CHAR * pszHeapNameOut,
		  IMG_UINT32 uiHeapNameBufSz,
		  IMG_DEV_VIRTADDR * psDevVAddrBaseOut,
		  IMG_DEVMEM_SIZE_T * puiHeapLengthOut,
		  IMG_UINT32 * puiLog2DataPageSize);

/*
 * Devmem_FindHeapByName()
 *
 * returns the heap handle for the named _automagic_ heap in this
 * context.  "automagic" heaps are those that are born with the
 * context from a blueprint
 */
extern PVRSRV_ERROR
DevmemFindHeapByName(const DEVMEM_CONTEXT * psCtx,
		     const IMG_CHAR * pszHeapName, DEVMEM_HEAP ** ppsHeapRet);

/*
 * DevmemGetHeapBaseDevVAddr()
 *
 * returns the device virtual address of the base of the heap.
 */

PVRSRV_ERROR
DevmemGetHeapBaseDevVAddr(DEVMEM_HEAP * psHeap, IMG_DEV_VIRTADDR * pDevVAddr);

extern PVRSRV_ERROR
DevmemGetImportUID(DEVMEM_MEMDESC * psMemDesc, IMG_UINT64 * pui64UID);

#endif				/* #ifndef SRVCLIENT_DEVICEMEM_CLIENT_H */
