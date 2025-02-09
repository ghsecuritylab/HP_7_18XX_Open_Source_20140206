									    /*************************************************************************//*!
									       @File
									       @Title          Device specific initialisation routines
									       @Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
									       @Description    Device specific MMU initialisation
									       @License        Strictly Confidential.
    *//**************************************************************************/
#include "rgxmmuinit.h"
#include "rgxmmudefs_km.h"

#include "device.h"
#include "img_types.h"
#include "mmu_common.h"
#include "pdump_mmu.h"

#include "pvr_debug.h"
#include "pvrsrv_error.h"
#include "rgx_memallocflags.h"

/*
 * Bits of PT, PD and PC not involving addresses 
 */

#define RGX_MMUCTRL_PTE_PROTMASK	(RGX_MMUCTRL_PT_DATA_PM_META_PROTECT_EN | \
									 RGX_MMUCTRL_PT_DATA_ENTRY_PENDING_EN | \
									 RGX_MMUCTRL_PT_DATA_PM_SRC_EN | \
									 RGX_MMUCTRL_PT_DATA_SLC_BYPASS_CTRL_EN | \
									 RGX_MMUCTRL_PT_DATA_CC_EN | \
									 RGX_MMUCTRL_PT_DATA_READ_ONLY_EN | \
									 RGX_MMUCTRL_PT_DATA_VALID_EN)

#define RGX_MMUCTRL_PDE_PROTMASK	(RGX_MMUCTRL_PD_DATA_ENTRY_PENDING_EN | \
									 ~RGX_MMUCTRL_PD_DATA_PAGE_SIZE_CLRMSK | \
									 RGX_MMUCTRL_PD_DATA_VALID_EN)

#define RGX_MMUCTRL_PCE_PROTMASK	(RGX_MMUCTRL_PC_DATA_ENTRY_PENDING_EN | \
									 RGX_MMUCTRL_PC_DATA_VALID_EN)

static MMU_PxE_CONFIG sRGXMMUPCEConfig;
static MMU_DEVVADDR_CONFIG sRGXMMUTopLevelDevVAddrConfig;

typedef struct _RGX_PAGESIZECONFIG_ {
	const MMU_PxE_CONFIG *psPDEConfig;
	const MMU_PxE_CONFIG *psPTEConfig;
	const MMU_DEVVADDR_CONFIG *psDevVAddrConfig;
	IMG_UINT32 uiRefCount;
	IMG_UINT32 uiMaxRefCount;
} RGX_PAGESIZECONFIG;

/*
 *
 *  Configuration for heaps with 4kB Data-Page size
 *
 */

static MMU_PxE_CONFIG sRGXMMUPDEConfig_4KBDP;
static MMU_PxE_CONFIG sRGXMMUPTEConfig_4KBDP;
static MMU_DEVVADDR_CONFIG sRGXMMUDevVAddrConfig_4KBDP;
static RGX_PAGESIZECONFIG gsPageSizeConfig4KB;

/*
 *
 *  Configuration for heaps with 16kB Data-Page size
 *
 */

static MMU_PxE_CONFIG sRGXMMUPDEConfig_16KBDP;
static MMU_PxE_CONFIG sRGXMMUPTEConfig_16KBDP;
static MMU_DEVVADDR_CONFIG sRGXMMUDevVAddrConfig_16KBDP;
static RGX_PAGESIZECONFIG gsPageSizeConfig16KB;

/*
 *
 *  Configuration for heaps with 64kB Data-Page size
 *
 */

static MMU_PxE_CONFIG sRGXMMUPDEConfig_64KBDP;
static MMU_PxE_CONFIG sRGXMMUPTEConfig_64KBDP;
static MMU_DEVVADDR_CONFIG sRGXMMUDevVAddrConfig_64KBDP;
static RGX_PAGESIZECONFIG gsPageSizeConfig64KB;

/*
 *
 *  Configuration for heaps with 256kB Data-Page size
 *
 */

static MMU_PxE_CONFIG sRGXMMUPDEConfig_256KBDP;
static MMU_PxE_CONFIG sRGXMMUPTEConfig_256KBDP;
static MMU_DEVVADDR_CONFIG sRGXMMUDevVAddrConfig_256KBDP;
static RGX_PAGESIZECONFIG gsPageSizeConfig256KB;

/*
 *
 *  Configuration for heaps with 1MB Data-Page size
 *
 */

static MMU_PxE_CONFIG sRGXMMUPDEConfig_1MBDP;
static MMU_PxE_CONFIG sRGXMMUPTEConfig_1MBDP;
static MMU_DEVVADDR_CONFIG sRGXMMUDevVAddrConfig_1MBDP;
static RGX_PAGESIZECONFIG gsPageSizeConfig1MB;

/*
 *
 *  Configuration for heaps with 2MB Data-Page size
 *
 */

static MMU_PxE_CONFIG sRGXMMUPDEConfig_2MBDP;
static MMU_PxE_CONFIG sRGXMMUPTEConfig_2MBDP;
static MMU_DEVVADDR_CONFIG sRGXMMUDevVAddrConfig_2MBDP;
static RGX_PAGESIZECONFIG gsPageSizeConfig2MB;

/* Forward declaration of protection bits derivation functions, for
   the following structure */
static IMG_UINT64 RGXDerivePCEProt8(IMG_UINT32 uiProtFlags);
static IMG_UINT32 RGXDerivePCEProt4(IMG_UINT32 uiProtFlags);
static IMG_UINT64 RGXDerivePDEProt8(IMG_UINT32 uiProtFlags);
static IMG_UINT32 RGXDerivePDEProt4(IMG_UINT32 uiProtFlags);
static IMG_UINT64 RGXDerivePTEProt8(IMG_UINT32 uiProtFlags);
static IMG_UINT32 RGXDerivePTEProt4(IMG_UINT32 uiProtFlags);

static PVRSRV_ERROR RGXGetPageSizeConfigCB(IMG_UINT32 uiLog2DataPageSize,
					   const MMU_PxE_CONFIG **
					   ppsMMUPDEConfig,
					   const MMU_PxE_CONFIG **
					   ppsMMUPTEConfig,
					   const MMU_DEVVADDR_CONFIG **
					   ppsMMUDevVAddrConfig,
					   IMG_HANDLE * phPriv);

static PVRSRV_ERROR RGXPutPageSizeConfigCB(IMG_HANDLE hPriv);

static MMU_DEVICEATTRIBS sRGXMMUDeviceAttributes;

PVRSRV_ERROR RGXMMUInit_Register(PVRSRV_DEVICE_NODE * psDeviceNode)
{
	/*
	 * Setup sRGXMMUDeviceAttributes
	 */
	sRGXMMUDeviceAttributes.eTopLevel = MMU_LEVEL_3;
	sRGXMMUDeviceAttributes.ui32BaseAlign = 12;	/* FIXME: Is there a define for this? */
	sRGXMMUDeviceAttributes.psBaseConfig = &sRGXMMUPCEConfig;
	sRGXMMUDeviceAttributes.psTopLevelDevVAddrConfig =
	    &sRGXMMUTopLevelDevVAddrConfig;

	/* Functions for deriving page table/dir/cat protection bits */
	sRGXMMUDeviceAttributes.pfnDerivePCEProt8 = RGXDerivePCEProt8;
	sRGXMMUDeviceAttributes.pfnDerivePCEProt4 = RGXDerivePCEProt4;
	sRGXMMUDeviceAttributes.pfnDerivePDEProt8 = RGXDerivePDEProt8;
	sRGXMMUDeviceAttributes.pfnDerivePDEProt4 = RGXDerivePDEProt4;
	sRGXMMUDeviceAttributes.pfnDerivePTEProt8 = RGXDerivePTEProt8;
	sRGXMMUDeviceAttributes.pfnDerivePTEProt4 = RGXDerivePTEProt4;

	/* Functions for establishing configurations for PDE/PTE/DEVVADDR
	   on per-heap basis */
	sRGXMMUDeviceAttributes.pfnGetPageSizeConfiguration =
	    RGXGetPageSizeConfigCB;
	sRGXMMUDeviceAttributes.pfnPutPageSizeConfiguration =
	    RGXPutPageSizeConfigCB;

	/*
	 * Setup sRGXMMUPCEConfig
	 */
	sRGXMMUPCEConfig.uiBytesPerEntry = 4;
	sRGXMMUPCEConfig.uiAddrMask = 0xfffffff0;

	sRGXMMUPCEConfig.uiAddrShift = 4;
	sRGXMMUPCEConfig.uiLog2Align = 12;

	sRGXMMUPCEConfig.uiProtMask = RGX_MMUCTRL_PCE_PROTMASK;
	sRGXMMUPCEConfig.uiProtShift = 0;

	/*
	 *  Setup sRGXMMUTopLevelDevVAddrConfig
	 */
	sRGXMMUTopLevelDevVAddrConfig.uiPCIndexMask =
	    ~RGX_MMUCTRL_VADDR_PC_INDEX_CLRMSK;
	sRGXMMUTopLevelDevVAddrConfig.uiPCIndexShift =
	    RGX_MMUCTRL_VADDR_PC_INDEX_SHIFT;

	sRGXMMUTopLevelDevVAddrConfig.uiPDIndexMask =
	    ~RGX_MMUCTRL_VADDR_PD_INDEX_CLRMSK;
	sRGXMMUTopLevelDevVAddrConfig.uiPDIndexShift =
	    RGX_MMUCTRL_VADDR_PD_INDEX_SHIFT;

/*
 *
 *  Configuration for heaps with 4kB Data-Page size
 *
 */

	/*
	 * Setup sRGXMMUPDEConfig_4KBDP
	 */
	sRGXMMUPDEConfig_4KBDP.uiBytesPerEntry = 8;

	sRGXMMUPDEConfig_4KBDP.uiAddrMask = IMG_UINT64_C(0xfffffffff0);
	sRGXMMUPDEConfig_4KBDP.uiAddrShift = 12;
	sRGXMMUPDEConfig_4KBDP.uiLog2Align = 12;

	sRGXMMUPDEConfig_4KBDP.uiVarCtrlMask = IMG_UINT64_C(0x000000000e);
	sRGXMMUPDEConfig_4KBDP.uiVarCtrlShift = 1;

	sRGXMMUPDEConfig_4KBDP.uiProtMask = RGX_MMUCTRL_PDE_PROTMASK;
	sRGXMMUPDEConfig_4KBDP.uiProtShift = 0;

	/*
	 * Setup sRGXMMUPTEConfig_4KBDP
	 */
	sRGXMMUPTEConfig_4KBDP.uiBytesPerEntry = 8;

	sRGXMMUPTEConfig_4KBDP.uiAddrMask = IMG_UINT64_C(0xfffffff000);
	sRGXMMUPTEConfig_4KBDP.uiAddrShift = 12;
	sRGXMMUPTEConfig_4KBDP.uiLog2Align = 12;

	sRGXMMUPTEConfig_4KBDP.uiProtMask = RGX_MMUCTRL_PTE_PROTMASK;
	sRGXMMUPTEConfig_4KBDP.uiProtShift = 0;

	/*
	 * Setup sRGXMMUDevVAddrConfig_4KBDP
	 */
	sRGXMMUDevVAddrConfig_4KBDP.uiPCIndexMask =
	    ~RGX_MMUCTRL_VADDR_PC_INDEX_CLRMSK;
	sRGXMMUDevVAddrConfig_4KBDP.uiPCIndexShift =
	    RGX_MMUCTRL_VADDR_PC_INDEX_SHIFT;

	sRGXMMUDevVAddrConfig_4KBDP.uiPDIndexMask =
	    ~RGX_MMUCTRL_VADDR_PD_INDEX_CLRMSK;
	sRGXMMUDevVAddrConfig_4KBDP.uiPDIndexShift =
	    RGX_MMUCTRL_VADDR_PD_INDEX_SHIFT;

	sRGXMMUDevVAddrConfig_4KBDP.uiPTIndexMask =
	    ~RGX_MMUCTRL_VADDR_PT_INDEX_CLRMSK;
	sRGXMMUDevVAddrConfig_4KBDP.uiPTIndexShift =
	    RGX_MMUCTRL_VADDR_PT_INDEX_SHIFT;

	sRGXMMUDevVAddrConfig_4KBDP.uiPageOffsetMask =
	    IMG_UINT64_C(0x0000000fff);
	sRGXMMUDevVAddrConfig_4KBDP.uiPageOffsetShift = 0;

	/*
	 * Setup gsPageSizeConfig4KB
	 */
	gsPageSizeConfig4KB.psPDEConfig = &sRGXMMUPDEConfig_4KBDP;
	gsPageSizeConfig4KB.psPTEConfig = &sRGXMMUPTEConfig_4KBDP;
	gsPageSizeConfig4KB.psDevVAddrConfig = &sRGXMMUDevVAddrConfig_4KBDP;
	gsPageSizeConfig4KB.uiRefCount = 0;
	gsPageSizeConfig4KB.uiMaxRefCount = 0;

/*
 *
 *  Configuration for heaps with 16kB Data-Page size
 *
 */

	/*
	 * Setup sRGXMMUPDEConfig_16KBDP
	 */
	sRGXMMUPDEConfig_16KBDP.uiBytesPerEntry = 8;

	sRGXMMUPDEConfig_16KBDP.uiAddrMask = IMG_UINT64_C(0xfffffffff0);
	sRGXMMUPDEConfig_16KBDP.uiAddrShift = 10;
	sRGXMMUPDEConfig_16KBDP.uiLog2Align = 10;

	sRGXMMUPDEConfig_16KBDP.uiVarCtrlMask = IMG_UINT64_C(0x000000000e);
	sRGXMMUPDEConfig_16KBDP.uiVarCtrlShift = 1;

	sRGXMMUPDEConfig_16KBDP.uiProtMask = RGX_MMUCTRL_PDE_PROTMASK;
	sRGXMMUPDEConfig_16KBDP.uiProtShift = 0;

	/*
	 * Setup sRGXMMUPTEConfig_16KBDP
	 */
	sRGXMMUPTEConfig_16KBDP.uiBytesPerEntry = 8;

	sRGXMMUPTEConfig_16KBDP.uiAddrMask = IMG_UINT64_C(0xffffffc000);
	sRGXMMUPTEConfig_16KBDP.uiAddrShift = 14;
	sRGXMMUPTEConfig_16KBDP.uiLog2Align = 14;

	sRGXMMUPTEConfig_16KBDP.uiProtMask = RGX_MMUCTRL_PTE_PROTMASK;
	sRGXMMUPTEConfig_16KBDP.uiProtShift = 0;

	/*
	 * Setup sRGXMMUDevVAddrConfig_16KBDP
	 */
	sRGXMMUDevVAddrConfig_16KBDP.uiPCIndexMask =
	    ~RGX_MMUCTRL_VADDR_PC_INDEX_CLRMSK;
	sRGXMMUDevVAddrConfig_16KBDP.uiPCIndexShift =
	    RGX_MMUCTRL_VADDR_PC_INDEX_SHIFT;

	sRGXMMUDevVAddrConfig_16KBDP.uiPDIndexMask =
	    ~RGX_MMUCTRL_VADDR_PD_INDEX_CLRMSK;
	sRGXMMUDevVAddrConfig_16KBDP.uiPDIndexShift =
	    RGX_MMUCTRL_VADDR_PD_INDEX_SHIFT;

	sRGXMMUDevVAddrConfig_16KBDP.uiPTIndexMask = IMG_UINT64_C(0x00001fc000);
	sRGXMMUDevVAddrConfig_16KBDP.uiPTIndexShift = 14;

	sRGXMMUDevVAddrConfig_16KBDP.uiPageOffsetMask =
	    IMG_UINT64_C(0x0000003fff);
	sRGXMMUDevVAddrConfig_16KBDP.uiPageOffsetShift = 0;

	/*
	 * Setup gsPageSizeConfig16KB
	 */
	gsPageSizeConfig16KB.psPDEConfig = &sRGXMMUPDEConfig_16KBDP;
	gsPageSizeConfig16KB.psPTEConfig = &sRGXMMUPTEConfig_16KBDP;
	gsPageSizeConfig16KB.psDevVAddrConfig = &sRGXMMUDevVAddrConfig_16KBDP;
	gsPageSizeConfig16KB.uiRefCount = 0;
	gsPageSizeConfig16KB.uiMaxRefCount = 0;

/*
 *
 *  Configuration for heaps with 64kB Data-Page size
 *
 */

	/*
	 * Setup sRGXMMUPDEConfig_64KBDP
	 */
	sRGXMMUPDEConfig_64KBDP.uiBytesPerEntry = 8;

	sRGXMMUPDEConfig_64KBDP.uiAddrMask = IMG_UINT64_C(0xfffffffff0);
	sRGXMMUPDEConfig_64KBDP.uiAddrShift = 8;
	sRGXMMUPDEConfig_64KBDP.uiLog2Align = 8;

	sRGXMMUPDEConfig_64KBDP.uiVarCtrlMask = IMG_UINT64_C(0x000000000e);
	sRGXMMUPDEConfig_64KBDP.uiVarCtrlShift = 1;

	sRGXMMUPDEConfig_64KBDP.uiProtMask = RGX_MMUCTRL_PDE_PROTMASK;
	sRGXMMUPDEConfig_64KBDP.uiProtShift = 0;

	/*
	 * Setup sRGXMMUPTEConfig_64KBDP
	 */
	sRGXMMUPTEConfig_64KBDP.uiBytesPerEntry = 8;

	sRGXMMUPTEConfig_64KBDP.uiAddrMask = IMG_UINT64_C(0xffffff0000);
	sRGXMMUPTEConfig_64KBDP.uiAddrShift = 16;
	sRGXMMUPTEConfig_64KBDP.uiLog2Align = 16;

	sRGXMMUPTEConfig_64KBDP.uiProtMask = RGX_MMUCTRL_PTE_PROTMASK;
	sRGXMMUPTEConfig_64KBDP.uiProtShift = 0;

	/*
	 * Setup sRGXMMUDevVAddrConfig_64KBDP
	 */
	sRGXMMUDevVAddrConfig_64KBDP.uiPCIndexMask =
	    ~RGX_MMUCTRL_VADDR_PC_INDEX_CLRMSK;
	sRGXMMUDevVAddrConfig_64KBDP.uiPCIndexShift =
	    RGX_MMUCTRL_VADDR_PC_INDEX_SHIFT;

	sRGXMMUDevVAddrConfig_64KBDP.uiPDIndexMask =
	    ~RGX_MMUCTRL_VADDR_PD_INDEX_CLRMSK;
	sRGXMMUDevVAddrConfig_64KBDP.uiPDIndexShift =
	    RGX_MMUCTRL_VADDR_PD_INDEX_SHIFT;

	sRGXMMUDevVAddrConfig_64KBDP.uiPTIndexMask = IMG_UINT64_C(0x00001f0000);
	sRGXMMUDevVAddrConfig_64KBDP.uiPTIndexShift = 16;

	sRGXMMUDevVAddrConfig_64KBDP.uiPageOffsetMask =
	    IMG_UINT64_C(0x000000ffff);
	sRGXMMUDevVAddrConfig_64KBDP.uiPageOffsetShift = 0;

	/*
	 * Setup gsPageSizeConfig64KB
	 */
	gsPageSizeConfig64KB.psPDEConfig = &sRGXMMUPDEConfig_64KBDP;
	gsPageSizeConfig64KB.psPTEConfig = &sRGXMMUPTEConfig_64KBDP;
	gsPageSizeConfig64KB.psDevVAddrConfig = &sRGXMMUDevVAddrConfig_64KBDP;
	gsPageSizeConfig64KB.uiRefCount = 0;
	gsPageSizeConfig64KB.uiMaxRefCount = 0;

/*
 *
 *  Configuration for heaps with 256kB Data-Page size
 *
 */

	/*
	 * Setup sRGXMMUPDEConfig_256KBDP
	 */
	sRGXMMUPDEConfig_256KBDP.uiBytesPerEntry = 8;

	sRGXMMUPDEConfig_256KBDP.uiAddrMask = IMG_UINT64_C(0xfffffffff0);
	sRGXMMUPDEConfig_256KBDP.uiAddrShift = 6;
	sRGXMMUPDEConfig_256KBDP.uiLog2Align = 6;

	sRGXMMUPDEConfig_256KBDP.uiVarCtrlMask = IMG_UINT64_C(0x000000000e);
	sRGXMMUPDEConfig_256KBDP.uiVarCtrlShift = 1;

	sRGXMMUPDEConfig_256KBDP.uiProtMask = RGX_MMUCTRL_PDE_PROTMASK;
	sRGXMMUPDEConfig_256KBDP.uiProtShift = 0;

	/*
	 * Setup MMU_PxE_CONFIG sRGXMMUPTEConfig_256KBDP
	 */
	sRGXMMUPTEConfig_256KBDP.uiBytesPerEntry = 8;

	sRGXMMUPTEConfig_256KBDP.uiAddrMask = IMG_UINT64_C(0xfffffc0000);
	sRGXMMUPTEConfig_256KBDP.uiAddrShift = 18;
	sRGXMMUPTEConfig_256KBDP.uiLog2Align = 18;

	sRGXMMUPTEConfig_256KBDP.uiProtMask = RGX_MMUCTRL_PTE_PROTMASK;
	sRGXMMUPTEConfig_256KBDP.uiProtShift = 0;

	/*
	 * Setup sRGXMMUDevVAddrConfig_256KBDP
	 */
	sRGXMMUDevVAddrConfig_256KBDP.uiPCIndexMask =
	    ~RGX_MMUCTRL_VADDR_PC_INDEX_CLRMSK;
	sRGXMMUDevVAddrConfig_256KBDP.uiPCIndexShift =
	    RGX_MMUCTRL_VADDR_PC_INDEX_SHIFT;

	sRGXMMUDevVAddrConfig_256KBDP.uiPDIndexMask =
	    ~RGX_MMUCTRL_VADDR_PD_INDEX_CLRMSK;
	sRGXMMUDevVAddrConfig_256KBDP.uiPDIndexShift =
	    RGX_MMUCTRL_VADDR_PD_INDEX_SHIFT;

	sRGXMMUDevVAddrConfig_256KBDP.uiPTIndexMask =
	    IMG_UINT64_C(0x00001c0000);
	sRGXMMUDevVAddrConfig_256KBDP.uiPTIndexShift = 18;

	sRGXMMUDevVAddrConfig_256KBDP.uiPageOffsetMask =
	    IMG_UINT64_C(0x000003ffff);
	sRGXMMUDevVAddrConfig_256KBDP.uiPageOffsetShift = 0;

	/*
	 * Setup gsPageSizeConfig256KB
	 */
	gsPageSizeConfig256KB.psPDEConfig = &sRGXMMUPDEConfig_256KBDP;
	gsPageSizeConfig256KB.psPTEConfig = &sRGXMMUPTEConfig_256KBDP;
	gsPageSizeConfig256KB.psDevVAddrConfig = &sRGXMMUDevVAddrConfig_256KBDP;
	gsPageSizeConfig256KB.uiRefCount = 0;
	gsPageSizeConfig256KB.uiMaxRefCount = 0;

	/*
	 * Setup sRGXMMUPDEConfig_1MBDP
	 */
	sRGXMMUPDEConfig_1MBDP.uiBytesPerEntry = 8;

	sRGXMMUPDEConfig_1MBDP.uiAddrMask = IMG_UINT64_C(0xfffffffff0);
	sRGXMMUPDEConfig_1MBDP.uiAddrShift = 4;
	sRGXMMUPDEConfig_1MBDP.uiLog2Align = 4;

	sRGXMMUPDEConfig_1MBDP.uiVarCtrlMask = IMG_UINT64_C(0x000000000e);
	sRGXMMUPDEConfig_1MBDP.uiVarCtrlShift = 1;

	sRGXMMUPDEConfig_1MBDP.uiProtMask = RGX_MMUCTRL_PDE_PROTMASK;
	sRGXMMUPDEConfig_1MBDP.uiProtShift = 0;

	/*
	 * Setup sRGXMMUPTEConfig_1MBDP
	 */
	sRGXMMUPTEConfig_1MBDP.uiBytesPerEntry = 8,
	    sRGXMMUPTEConfig_1MBDP.uiAddrMask = IMG_UINT64_C(0xfffff00000);
	sRGXMMUPTEConfig_1MBDP.uiAddrShift = 20;
	sRGXMMUPTEConfig_1MBDP.uiLog2Align = 20;

	sRGXMMUPTEConfig_1MBDP.uiProtMask = RGX_MMUCTRL_PTE_PROTMASK;
	sRGXMMUPTEConfig_1MBDP.uiProtShift = 0;

	/*
	 * Setup sRGXMMUDevVAddrConfig_1MBDP
	 */
	sRGXMMUDevVAddrConfig_1MBDP.uiPCIndexMask =
	    ~RGX_MMUCTRL_VADDR_PC_INDEX_CLRMSK;
	sRGXMMUDevVAddrConfig_1MBDP.uiPCIndexShift =
	    RGX_MMUCTRL_VADDR_PC_INDEX_SHIFT;

	sRGXMMUDevVAddrConfig_1MBDP.uiPDIndexMask =
	    ~RGX_MMUCTRL_VADDR_PD_INDEX_CLRMSK;
	sRGXMMUDevVAddrConfig_1MBDP.uiPDIndexShift =
	    RGX_MMUCTRL_VADDR_PD_INDEX_SHIFT;

	sRGXMMUDevVAddrConfig_1MBDP.uiPTIndexMask = IMG_UINT64_C(0x0000100000);
	sRGXMMUDevVAddrConfig_1MBDP.uiPTIndexShift = 20;

	sRGXMMUDevVAddrConfig_1MBDP.uiPageOffsetMask =
	    IMG_UINT64_C(0x00000fffff);
	sRGXMMUDevVAddrConfig_1MBDP.uiPageOffsetShift = 0;

	/*
	 * Setup gsPageSizeConfig1MB
	 */
	gsPageSizeConfig1MB.psPDEConfig = &sRGXMMUPDEConfig_1MBDP;
	gsPageSizeConfig1MB.psPTEConfig = &sRGXMMUPTEConfig_1MBDP;
	gsPageSizeConfig1MB.psDevVAddrConfig = &sRGXMMUDevVAddrConfig_1MBDP;
	gsPageSizeConfig1MB.uiRefCount = 0;
	gsPageSizeConfig1MB.uiMaxRefCount = 0;

	/*
	 * Setup sRGXMMUPDEConfig_2MBDP
	 */
	sRGXMMUPDEConfig_2MBDP.uiBytesPerEntry = 8;

	sRGXMMUPDEConfig_2MBDP.uiAddrMask = IMG_UINT64_C(0xfffffffff0);
	sRGXMMUPDEConfig_2MBDP.uiAddrShift = 4;
	sRGXMMUPDEConfig_2MBDP.uiLog2Align = 4;

	sRGXMMUPDEConfig_2MBDP.uiVarCtrlMask = IMG_UINT64_C(0x000000000e);
	sRGXMMUPDEConfig_2MBDP.uiVarCtrlShift = 1;

	sRGXMMUPDEConfig_2MBDP.uiProtMask = RGX_MMUCTRL_PDE_PROTMASK;
	sRGXMMUPDEConfig_2MBDP.uiProtShift = 0;

	/*
	 * Setup sRGXMMUPTEConfig_2MBDP
	 */
	sRGXMMUPTEConfig_2MBDP.uiBytesPerEntry = 8;

	sRGXMMUPTEConfig_2MBDP.uiAddrMask = IMG_UINT64_C(0xffffe00000);
	sRGXMMUPTEConfig_2MBDP.uiAddrShift = 21;
	sRGXMMUPTEConfig_2MBDP.uiLog2Align = 21;

	sRGXMMUPTEConfig_2MBDP.uiProtMask = RGX_MMUCTRL_PTE_PROTMASK;
	sRGXMMUPTEConfig_2MBDP.uiProtShift = 0;

	/*
	 * Setup sRGXMMUDevVAddrConfig_2MBDP
	 */
	sRGXMMUDevVAddrConfig_2MBDP.uiPCIndexMask =
	    ~RGX_MMUCTRL_VADDR_PC_INDEX_CLRMSK;
	sRGXMMUDevVAddrConfig_2MBDP.uiPCIndexShift =
	    RGX_MMUCTRL_VADDR_PC_INDEX_SHIFT;

	sRGXMMUDevVAddrConfig_2MBDP.uiPDIndexMask =
	    ~RGX_MMUCTRL_VADDR_PD_INDEX_CLRMSK;
	sRGXMMUDevVAddrConfig_2MBDP.uiPDIndexShift =
	    RGX_MMUCTRL_VADDR_PD_INDEX_SHIFT;

	sRGXMMUDevVAddrConfig_2MBDP.uiPTIndexMask = IMG_UINT64_C(0x0000000000);
	sRGXMMUDevVAddrConfig_2MBDP.uiPTIndexShift = 21;

	sRGXMMUDevVAddrConfig_2MBDP.uiPageOffsetMask =
	    IMG_UINT64_C(0x00001fffff);
	sRGXMMUDevVAddrConfig_2MBDP.uiPageOffsetShift = 0;

	/*
	 * Setup gsPageSizeConfig2MB
	 */
	gsPageSizeConfig2MB.psPDEConfig = &sRGXMMUPDEConfig_2MBDP;
	gsPageSizeConfig2MB.psPTEConfig = &sRGXMMUPTEConfig_2MBDP;
	gsPageSizeConfig2MB.psDevVAddrConfig = &sRGXMMUDevVAddrConfig_2MBDP;
	gsPageSizeConfig2MB.uiRefCount = 0;
	gsPageSizeConfig2MB.uiMaxRefCount = 0;

	/*
	 * Setup sRGXMMUDeviceAttributes
	 */
	sRGXMMUDeviceAttributes.eTopLevel = MMU_LEVEL_3;
	sRGXMMUDeviceAttributes.ui32BaseAlign = 12,	/* FIXME: Is there a define for this? */
	    sRGXMMUDeviceAttributes.psBaseConfig = &sRGXMMUPCEConfig;
	sRGXMMUDeviceAttributes.psTopLevelDevVAddrConfig =
	    &sRGXMMUTopLevelDevVAddrConfig;

	/* Functions for deriving page table/dir/cat protection bits */
	sRGXMMUDeviceAttributes.pfnDerivePCEProt8 = RGXDerivePCEProt8;
	sRGXMMUDeviceAttributes.pfnDerivePCEProt4 = RGXDerivePCEProt4;
	sRGXMMUDeviceAttributes.pfnDerivePDEProt8 = RGXDerivePDEProt8;
	sRGXMMUDeviceAttributes.pfnDerivePDEProt4 = RGXDerivePDEProt4;
	sRGXMMUDeviceAttributes.pfnDerivePTEProt8 = RGXDerivePTEProt8;
	sRGXMMUDeviceAttributes.pfnDerivePTEProt4 = RGXDerivePTEProt4;

	/* Functions for establishing configurations for PDE/PTE/DEVVADDR
	   on per-heap basis */
	sRGXMMUDeviceAttributes.pfnGetPageSizeConfiguration =
	    RGXGetPageSizeConfigCB,
	    sRGXMMUDeviceAttributes.pfnPutPageSizeConfiguration =
	    RGXPutPageSizeConfigCB;

	/* FIXME: This probably ought to be a call into the mmu code. */
	psDeviceNode->psMMUDevAttrs = &sRGXMMUDeviceAttributes;

	return PVRSRV_OK;
}

PVRSRV_ERROR RGXMMUInit_Unregister(PVRSRV_DEVICE_NODE * psDeviceNode)
{
	PVRSRV_ERROR eError;

	eError = PVRSRV_OK;

#if defined(PDUMP)
	psDeviceNode->pfnMMUGetContextID = IMG_NULL;
#endif

	/* FIXME: This probably ought to be a call into the mmu code. */
	psDeviceNode->psMMUDevAttrs = IMG_NULL;	// &sRGXMMUDeviceAttributes;
	//    psDeviceNode->eMMUType = MMU_TYPE_3LEVEL;

#if defined(DEBUG)
	PVR_DPF((PVR_DBG_MESSAGE, "Variable Page Size Heap Stats:"));
	PVR_DPF((PVR_DBG_MESSAGE, "Max 4K page heaps: %d",
		 gsPageSizeConfig4KB.uiMaxRefCount));
	PVR_DPF((PVR_DBG_VERBOSE, "Current 4K page heaps (should be 0): %d",
		 gsPageSizeConfig4KB.uiRefCount));
	PVR_DPF((PVR_DBG_MESSAGE, "Max 16K page heaps: %d",
		 gsPageSizeConfig16KB.uiMaxRefCount));
	PVR_DPF((PVR_DBG_VERBOSE, "Current 16K page heaps (should be 0): %d",
		 gsPageSizeConfig16KB.uiRefCount));
	PVR_DPF((PVR_DBG_MESSAGE, "Max 64K page heaps: %d",
		 gsPageSizeConfig64KB.uiMaxRefCount));
	PVR_DPF((PVR_DBG_VERBOSE, "Current 64K page heaps (should be 0): %d",
		 gsPageSizeConfig64KB.uiRefCount));
	PVR_DPF((PVR_DBG_MESSAGE, "Max 256K page heaps: %d",
		 gsPageSizeConfig256KB.uiMaxRefCount));
	PVR_DPF((PVR_DBG_VERBOSE, "Current 256K page heaps (should be 0): %d",
		 gsPageSizeConfig256KB.uiRefCount));
	PVR_DPF((PVR_DBG_MESSAGE, "Max 1M page heaps: %d",
		 gsPageSizeConfig1MB.uiMaxRefCount));
	PVR_DPF((PVR_DBG_VERBOSE, "Current 1M page heaps (should be 0): %d",
		 gsPageSizeConfig1MB.uiRefCount));
	PVR_DPF((PVR_DBG_MESSAGE, "Max 2M page heaps: %d",
		 gsPageSizeConfig2MB.uiMaxRefCount));
	PVR_DPF((PVR_DBG_VERBOSE, "Current 2M page heaps (should be 0): %d",
		 gsPageSizeConfig2MB.uiRefCount));
#endif
	if (gsPageSizeConfig4KB.uiRefCount > 0 ||
	    gsPageSizeConfig16KB.uiRefCount > 0 ||
	    gsPageSizeConfig64KB.uiRefCount > 0 ||
	    gsPageSizeConfig256KB.uiRefCount > 0 ||
	    gsPageSizeConfig1MB.uiRefCount > 0 ||
	    gsPageSizeConfig2MB.uiRefCount > 0) {
		PVR_DPF((PVR_DBG_ERROR,
			 "RGXMMUInit_Unregister: Unbalanced MMU API Usage (Internal error)"));
	}

	return eError;
}

									    /*************************************************************************//*!
									       @Function       RGXDerivePCEProt4
									       @Description    calculate the PCE protection flags based on a 4 byte entry
									       @Return         PVRSRV_ERROR
    *//**************************************************************************/
static IMG_UINT32 RGXDerivePCEProt4(IMG_UINT32 uiProtFlags)
{
	return (uiProtFlags & MMU_PROTFLAGS_INVALID) ? 0 :
	    RGX_MMUCTRL_PC_DATA_VALID_EN;
}

									    /*************************************************************************//*!
									       @Function       RGXDerivePCEProt8
									       @Description    calculate the PCE protection flags based on an 8 byte entry
									       @Return         PVRSRV_ERROR
    *//**************************************************************************/
static IMG_UINT64 RGXDerivePCEProt8(IMG_UINT32 uiProtFlags)
{
	PVR_UNREFERENCED_PARAMETER(uiProtFlags);
	PVR_DPF((PVR_DBG_ERROR, "8-byte PCE not supported on this device"));
	return 0;
}

									    /*************************************************************************//*!
									       @Function       RGXDerivePDEProt4
									       @Description    derive the PDE protection flags based on a 4 byte entry
									       @Return         PVRSRV_ERROR
    *//**************************************************************************/
static IMG_UINT32 RGXDerivePDEProt4(IMG_UINT32 uiProtFlags)
{
	PVR_UNREFERENCED_PARAMETER(uiProtFlags);
	PVR_DPF((PVR_DBG_ERROR, "4-byte PDE not supported on this device"));
	return 0;
}

									    /*************************************************************************//*!
									       @Function       RGXDerivePDEProt8
									       @Description    derive the PDE protection flags based on an 8 byte entry
									       @Return         PVRSRV_ERROR
    *//**************************************************************************/
static IMG_UINT64 RGXDerivePDEProt8(IMG_UINT32 uiProtFlags)
{
	return (uiProtFlags & MMU_PROTFLAGS_INVALID) ? 0 :
	    RGX_MMUCTRL_PD_DATA_VALID_EN;
	//FIXME:  what about page size?  Where does this get set?
}

									    /*************************************************************************//*!
									       @Function       RGXDerivePTEProt4
									       @Description    calculate the PTE protection flags based on a 4 byte entry
									       @Return         PVRSRV_ERROR
    *//**************************************************************************/
static IMG_UINT32 RGXDerivePTEProt4(IMG_UINT32 uiProtFlags)
{
	PVR_UNREFERENCED_PARAMETER(uiProtFlags);
	PVR_DPF((PVR_DBG_ERROR, "4-byte PTE not supported on this device"));

	return 0;
}

									    /*************************************************************************//*!
									       @Function       RGXDerivePTEProt8
									       @Description    calculate the PTE protection flags based on an 8 byte entry
									       @Return         PVRSRV_ERROR
    *//**************************************************************************/
static IMG_UINT64 RGXDerivePTEProt8(IMG_UINT32 uiProtFlags)
{
	IMG_UINT64 ui64MMUFlags;

	ui64MMUFlags = 0;

	if (((MMU_PROTFLAGS_READABLE | MMU_PROTFLAGS_WRITEABLE) & uiProtFlags)
	    == (MMU_PROTFLAGS_READABLE | MMU_PROTFLAGS_WRITEABLE)) {
		/* read/write */
	} else if (MMU_PROTFLAGS_READABLE & uiProtFlags) {
		/* read only */
		ui64MMUFlags |= RGX_MMUCTRL_PT_DATA_READ_ONLY_EN;
	} else if (MMU_PROTFLAGS_WRITEABLE & uiProtFlags) {
		/* write only */
		PVR_DPF((PVR_DBG_ERROR,
			 "RGXDerivePTEProt8: write-only is not possible on this device"));
	} else if ((MMU_PROTFLAGS_INVALID & uiProtFlags) == 0) {
		PVR_DPF((PVR_DBG_ERROR,
			 "RGXDerivePTEProt8: neither read nor write specified..."));
	}

	/* cache coherency */
	if (MMU_PROTFLAGS_CACHE_COHERENT & uiProtFlags) {
		ui64MMUFlags |= RGX_MMUCTRL_PT_DATA_CC_EN;
	}

	/* cache setup */
	if ((MMU_PROTFLAGS_CACHED & uiProtFlags) == 0) {
		ui64MMUFlags |= RGX_MMUCTRL_PT_DATA_SLC_BYPASS_CTRL_EN;
	}

	if ((uiProtFlags & MMU_PROTFLAGS_INVALID) == 0) {
		ui64MMUFlags |= RGX_MMUCTRL_PT_DATA_VALID_EN;
	}

	if (MMU_PROTFLAGS_DEVICE(PMMETA_PROTECT) & uiProtFlags) {
		ui64MMUFlags |= RGX_MMUCTRL_PT_DATA_PM_META_PROTECT_EN;
	}

	return ui64MMUFlags;
}

									    /*************************************************************************//*!
									       @Function       RGXGetPageSizeConfig
									       @Description    Set up configuration for variable sized data pages
									       @Return         PVRSRV_ERROR
    *//**************************************************************************/
static PVRSRV_ERROR RGXGetPageSizeConfigCB(IMG_UINT32 uiLog2DataPageSize,
					   const MMU_PxE_CONFIG **
					   ppsMMUPDEConfig,
					   const MMU_PxE_CONFIG **
					   ppsMMUPTEConfig,
					   const MMU_DEVVADDR_CONFIG **
					   ppsMMUDevVAddrConfig,
					   IMG_HANDLE * phPriv)
{
	RGX_PAGESIZECONFIG *psPageSizeConfig;

	switch (uiLog2DataPageSize) {
	case 12:
		psPageSizeConfig = &gsPageSizeConfig4KB;
		break;
	case 14:
		psPageSizeConfig = &gsPageSizeConfig16KB;
		break;
	case 16:
		psPageSizeConfig = &gsPageSizeConfig64KB;
		break;
	case 18:
		psPageSizeConfig = &gsPageSizeConfig256KB;
		break;
	case 20:
		psPageSizeConfig = &gsPageSizeConfig1MB;
		break;
	case 21:
		psPageSizeConfig = &gsPageSizeConfig2MB;
		break;
	default:
		PVR_DPF((PVR_DBG_ERROR,
			 "RGXGetPageSizeConfigCB: Invalid Data Page Size 1<<0x%x",
			 uiLog2DataPageSize));
		return PVRSRV_ERROR_MMU_INVALID_PAGE_SIZE_FOR_DEVICE;
	}

	/* Refer caller's pointers to the data */
	*ppsMMUPDEConfig = psPageSizeConfig->psPDEConfig;
	*ppsMMUPTEConfig = psPageSizeConfig->psPTEConfig;
	*ppsMMUDevVAddrConfig = psPageSizeConfig->psDevVAddrConfig;

	/* Increment ref-count - not that we're allocating anything here
	   (I'm using static structs), but one day we might, so we want
	   the Get/Put code to be balanced properly */
	psPageSizeConfig->uiRefCount++;

	/* This is purely for debug statistics */
	psPageSizeConfig->uiMaxRefCount = MAX(psPageSizeConfig->uiMaxRefCount,
					      psPageSizeConfig->uiRefCount);

	*phPriv = (IMG_HANDLE) (IMG_UINTPTR_T) uiLog2DataPageSize;
	PVR_ASSERT(uiLog2DataPageSize == (IMG_UINT32) (IMG_UINTPTR_T) * phPriv);

	return PVRSRV_OK;
}

									    /*************************************************************************//*!
									       @Function       RGXPutPageSizeConfig
									       @Description    Tells this code that the mmu module is done with the
									       configurations set in RGXGetPageSizeConfig.  This can
									       be a no-op.
									       @Return         PVRSRV_ERROR
    *//**************************************************************************/
static PVRSRV_ERROR RGXPutPageSizeConfigCB(IMG_HANDLE hPriv)
{
	RGX_PAGESIZECONFIG *psPageSizeConfig;
	IMG_UINT32 uiLog2DataPageSize;

	uiLog2DataPageSize = (IMG_UINT32) (IMG_UINTPTR_T) hPriv;

	switch (uiLog2DataPageSize) {
	case 12:
		psPageSizeConfig = &gsPageSizeConfig4KB;
		break;
	case 14:
		psPageSizeConfig = &gsPageSizeConfig16KB;
		break;
	case 16:
		psPageSizeConfig = &gsPageSizeConfig64KB;
		break;
	case 18:
		psPageSizeConfig = &gsPageSizeConfig256KB;
		break;
	case 20:
		psPageSizeConfig = &gsPageSizeConfig1MB;
		break;
	case 21:
		psPageSizeConfig = &gsPageSizeConfig2MB;
		break;
	default:
		PVR_DPF((PVR_DBG_ERROR,
			 "RGXPutPageSizeConfigCB: Invalid Data Page Size 1<<0x%x",
			 uiLog2DataPageSize));
		return PVRSRV_ERROR_MMU_INVALID_PAGE_SIZE_FOR_DEVICE;
	}

	/* Ref-count here is not especially useful, but it's an extra
	   check that the API is being used correctly */
	psPageSizeConfig->uiRefCount--;

	return PVRSRV_OK;
}
