									    /*************************************************************************//*!
									       @File
									       @Title          Self scaling hash tables.
									       @Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
									       @Description 
									       Implements simple self scaling hash tables. Hash collisions are
									       handled by chaining entries together. Hash tables are increased in
									       size when they become more than (50%?) full and decreased in size
									       when less than (25%?) full. Hash tables are never decreased below
									       their initial size.
									       @License        Strictly Confidential.
    *//**************************************************************************/

/* include/ */
#include "img_defs.h"
#include "img_types.h"
#include "pvr_debug.h"
#include "pvrsrv_error.h"

/* services/shared/include/ */
#include "hash.h"

/* services/client/include/ or services/server/include/ */
#include "osfunc.h"
#include "allocmem.h"

#if defined(__KERNEL__)
#include "pvrsrv.h"
#endif

#define PRIVATE_MAX(a,b) ((a)>(b)?(a):(b))

#define	KEY_TO_INDEX(pHash, key, uSize) \
	((pHash)->pfnHashFunc((pHash)->uKeySize, (key), (uSize)) % (uSize))

#define	KEY_COMPARE(pHash, pKey1, pKey2) \
	((pHash)->pfnKeyComp((pHash)->uKeySize, (pKey1), (pKey2)))

/* Each entry in a hash table is placed into a bucket */
struct _BUCKET_ {
	/* the next bucket on the same chain */
	struct _BUCKET_ *pNext;

	/* entry value */
	IMG_UINTPTR_T v;

	/* entry key */
#if defined (WIN32)
	IMG_UINTPTR_T k[1];
#else
	IMG_UINTPTR_T k[];	/* PRQA S 0642 *//* override dynamic array declaration warning */
#endif
};
typedef struct _BUCKET_ BUCKET;

struct _HASH_TABLE_ {
	/* the hash table array */
	BUCKET **ppBucketTable;

	/* current size of the hash table */
	IMG_UINT32 uSize;

	/* number of entries currently in the hash table */
	IMG_UINT32 uCount;

	/* the minimum size that the hash table should be re-sized to */
	IMG_UINT32 uMinimumSize;

	/* size of key in bytes */
	IMG_UINT32 uKeySize;

	/* hash function */
	HASH_FUNC *pfnHashFunc;

	/* key comparison function */
	HASH_KEY_COMP *pfnKeyComp;
};

									    /*************************************************************************//*!
									       @Function       HASH_Func_Default
									       @Description    Hash function intended for hashing keys composed of
									       IMG_UINTPTR_T arrays.
									       @Input          uKeySize     The size of the hash key, in bytes.
									       @Input          pKey         A pointer to the key to hash.
									       @Input          uHashTabLen  The length of the hash table.
									       @Return         The hash value.
    *//**************************************************************************/
IMG_INTERNAL IMG_UINT32
HASH_Func_Default(IMG_SIZE_T uKeySize, IMG_VOID * pKey, IMG_UINT32 uHashTabLen)
{
	IMG_UINTPTR_T *p = (IMG_UINTPTR_T *) pKey;
	IMG_UINT32 uKeyLen = uKeySize / sizeof(IMG_UINTPTR_T);
	IMG_UINT32 ui;
	IMG_UINT32 uHashKey = 0;

	PVR_UNREFERENCED_PARAMETER(uHashTabLen);

	PVR_ASSERT((uKeySize % sizeof(IMG_UINTPTR_T)) == 0);

	for (ui = 0; ui < uKeyLen; ui++) {
		IMG_UINT32 uHashPart = (IMG_UINT32) * p++;

		uHashPart += (uHashPart << 12);
		uHashPart ^= (uHashPart >> 22);
		uHashPart += (uHashPart << 4);
		uHashPart ^= (uHashPart >> 9);
		uHashPart += (uHashPart << 10);
		uHashPart ^= (uHashPart >> 2);
		uHashPart += (uHashPart << 7);
		uHashPart ^= (uHashPart >> 12);

		uHashKey += uHashPart;
	}

	return uHashKey;
}

									    /*************************************************************************//*!
									       @Function       HASH_Key_Comp_Default
									       @Description    Compares keys composed of IMG_UINTPTR_T arrays.
									       @Input          uKeySize    The size of the hash key, in bytes.
									       @Input          pKey1       Pointer to first hash key to compare.
									       @Input          pKey2       Pointer to second hash key to compare.
									       @Return         IMG_TRUE    The keys match.
									       IMG_FALSE   The keys don't match.
    *//**************************************************************************/
IMG_INTERNAL IMG_BOOL
HASH_Key_Comp_Default(IMG_SIZE_T uKeySize, IMG_VOID * pKey1, IMG_VOID * pKey2)
{
	IMG_UINTPTR_T *p1 = (IMG_UINTPTR_T *) pKey1;
	IMG_UINTPTR_T *p2 = (IMG_UINTPTR_T *) pKey2;
	IMG_UINT32 uKeyLen = uKeySize / sizeof(IMG_UINTPTR_T);
	IMG_UINT32 ui;

	PVR_ASSERT((uKeySize % sizeof(IMG_UINTPTR_T)) == 0);

	for (ui = 0; ui < uKeyLen; ui++) {
		if (*p1++ != *p2++)
			return IMG_FALSE;
	}

	return IMG_TRUE;
}

									    /*************************************************************************//*!
									       @Function       _ChainInsert
									       @Description    Insert a bucket into the appropriate hash table chain.
									       @Input          pBucket       The bucket
									       @Input          ppBucketTable The hash table
									       @Input          uSize         The size of the hash table
									       @Return         PVRSRV_ERROR
    *//**************************************************************************/
static PVRSRV_ERROR
_ChainInsert(HASH_TABLE * pHash, BUCKET * pBucket, BUCKET ** ppBucketTable,
	     IMG_UINT32 uSize)
{
	IMG_UINT32 uIndex;

	PVR_ASSERT(pBucket != IMG_NULL);
	PVR_ASSERT(ppBucketTable != IMG_NULL);
	PVR_ASSERT(uSize != 0);

	if ((pBucket == IMG_NULL) || (ppBucketTable == IMG_NULL)
	    || (uSize == 0)) {
		PVR_DPF((PVR_DBG_ERROR, "_ChainInsert: invalid parameter"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	uIndex = KEY_TO_INDEX(pHash, pBucket->k, uSize);	/* PRQA S 0432,0541 *//* ignore dynamic array warning */
	pBucket->pNext = ppBucketTable[uIndex];
	ppBucketTable[uIndex] = pBucket;

	return PVRSRV_OK;
}

									    /*************************************************************************//*!
									       @Function       _Rehash
									       @Description    Iterate over every entry in an old hash table and
									       rehash into the new table.
									       @Input          ppOldTable   The old hash table
									       @Input          uOldSize     The size of the old hash table
									       @Input          ppNewTable   The new hash table
									       @Input          uNewSize     The size of the new hash table
									       @Return         None
    *//**************************************************************************/
static PVRSRV_ERROR
_Rehash(HASH_TABLE * pHash,
	BUCKET ** ppOldTable, IMG_UINT32 uOldSize,
	BUCKET ** ppNewTable, IMG_UINT32 uNewSize)
{
	IMG_UINT32 uIndex;
	for (uIndex = 0; uIndex < uOldSize; uIndex++) {
		BUCKET *pBucket;
		pBucket = ppOldTable[uIndex];
		while (pBucket != IMG_NULL) {
			PVRSRV_ERROR eError;
			BUCKET *pNextBucket = pBucket->pNext;
			eError =
			    _ChainInsert(pHash, pBucket, ppNewTable, uNewSize);
			if (eError != PVRSRV_OK) {
				PVR_DPF((PVR_DBG_ERROR,
					 "_Rehash: call to _ChainInsert failed"));
				return eError;
			}
			pBucket = pNextBucket;
		}
	}
	return PVRSRV_OK;
}

									    /*************************************************************************//*!
									       @Function       _Resize
									       @Description    Attempt to resize a hash table, failure to allocate a
									       new larger hash table is not considered a hard failure.
									       We simply continue and allow the table to fill up, the
									       effect is to allow hash chains to become longer.
									       @Input          pHash      Hash table to resize.
									       @Input          uNewSize   Required table size.
									       @Return         IMG_TRUE Success
									       IMG_FALSE Failed
    *//**************************************************************************/
static IMG_BOOL _Resize(HASH_TABLE * pHash, IMG_UINT32 uNewSize)
{
	if (uNewSize != pHash->uSize) {
		BUCKET **ppNewTable;
		IMG_UINT32 uIndex;

		ppNewTable = OSAllocMem(sizeof(BUCKET *) * uNewSize);
		if (ppNewTable == IMG_NULL) {
			return IMG_FALSE;
		}

		for (uIndex = 0; uIndex < uNewSize; uIndex++)
			ppNewTable[uIndex] = IMG_NULL;

		if (_Rehash
		    (pHash, pHash->ppBucketTable, pHash->uSize, ppNewTable,
		     uNewSize) != PVRSRV_OK) {
			return IMG_FALSE;
		}

		OSFreeMem(pHash->ppBucketTable);
		/*not nulling pointer, being reassigned just below */
		pHash->ppBucketTable = ppNewTable;
		pHash->uSize = uNewSize;
	}
	return IMG_TRUE;
}

									    /*************************************************************************//*!
									       @Function       HASH_Create_Extended
									       @Description    Create a self scaling hash table, using the supplied
									       key size, and the supplied hash and key comparsion
									       functions.
									       @Input          uInitialLen   Initial and minimum length of the
									       hash table, where the length refers to the number
									       of entries in the hash table, not its size in
									       bytes.
									       @Input          uKeySize      The size of the key, in bytes.
									       @Input          pfnHashFunc   Pointer to hash function.
									       @Input          pfnKeyComp    Pointer to key comparsion function.
									       @Return         IMG_NULL or hash table handle.
    *//**************************************************************************/
IMG_INTERNAL
    HASH_TABLE * HASH_Create_Extended(IMG_UINT32 uInitialLen,
				      IMG_SIZE_T uKeySize,
				      HASH_FUNC * pfnHashFunc,
				      HASH_KEY_COMP * pfnKeyComp)
{
	HASH_TABLE *pHash;
	IMG_UINT32 uIndex;

	PVR_DPF((PVR_DBG_MESSAGE, "HASH_Create_Extended: InitialSize=0x%x",
		 uInitialLen));

	pHash = OSAllocMem(sizeof(HASH_TABLE));
	if (pHash == IMG_NULL) {
		return IMG_NULL;
	}

	pHash->uCount = 0;
	pHash->uSize = uInitialLen;
	pHash->uMinimumSize = uInitialLen;
	pHash->uKeySize = uKeySize;
	pHash->pfnHashFunc = pfnHashFunc;
	pHash->pfnKeyComp = pfnKeyComp;

	pHash->ppBucketTable = OSAllocMem(sizeof(BUCKET *) * pHash->uSize);
	if (pHash->ppBucketTable == IMG_NULL) {
		OSFreeMem(pHash);
		/*not nulling pointer, out of scope */
		return IMG_NULL;
	}

	if (pHash->ppBucketTable == IMG_NULL) {
		OSFreeMem(pHash);
		/*not nulling pointer, out of scope */
		return IMG_NULL;
	}

	for (uIndex = 0; uIndex < pHash->uSize; uIndex++)
		pHash->ppBucketTable[uIndex] = IMG_NULL;
	return pHash;
}

									    /*************************************************************************//*!
									       @Function       HASH_Create
									       @Description    Create a self scaling hash table with a key
									       consisting of a single IMG_UINTPTR_T, and using
									       the default hash and key comparison functions.
									       @Input          uInitialLen   Initial and minimum length of the
									       hash table, where the length refers to the
									       number of entries in the hash table, not its size
									       in bytes.
									       @Return         IMG_NULL or hash table handle.
    *//**************************************************************************/
IMG_INTERNAL HASH_TABLE * HASH_Create(IMG_UINT32 uInitialLen)
{
	return HASH_Create_Extended(uInitialLen, sizeof(IMG_UINTPTR_T),
				    &HASH_Func_Default, &HASH_Key_Comp_Default);
}

									    /*************************************************************************//*!
									       @Function       HASH_Delete
									       @Description    Delete a hash table created by HASH_Create_Extended or
									       HASH_Create.  All entries in the table must have been
									       removed before calling this function.
									       @Input          pHash     Hash table
									       @Return         None
    *//**************************************************************************/
IMG_INTERNAL IMG_VOID HASH_Delete(HASH_TABLE * pHash)
{
	IMG_BOOL bDoCheck = IMG_TRUE;
#if defined(__KERNEL__)
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	if (psPVRSRVData->eServicesState != PVRSRV_SERVICES_STATE_OK) {
		bDoCheck = IMG_FALSE;
	}
#endif
	if (pHash != IMG_NULL) {
		PVR_DPF((PVR_DBG_MESSAGE, "HASH_Delete"));

		if (bDoCheck) {
			PVR_ASSERT(pHash->uCount == 0);
		}
		if (pHash->uCount != 0) {
			PVR_DPF((PVR_DBG_ERROR,
				 "HASH_Delete: leak detected in hash table!"));
			PVR_DPF((PVR_DBG_ERROR,
				 "Likely Cause: client drivers not freeing alocations before destroying devmemcontext"));
		}
		OSFreeMem(pHash->ppBucketTable);
		pHash->ppBucketTable = IMG_NULL;
		OSFreeMem(pHash);
		/*not nulling pointer, copy on stack */
	}
}

									    /*************************************************************************//*!
									       @Function       HASH_Insert_Extended
									       @Description    Insert a key value pair into a hash table created
									       with HASH_Create_Extended.
									       @Input          pHash     Hash table
									       @Input          pKey      Pointer to the key.
									       @Input          v         The value associated with the key.
									       @Return         IMG_TRUE  - success
									       IMG_FALSE  - failure
    *//**************************************************************************/
IMG_INTERNAL IMG_BOOL
HASH_Insert_Extended(HASH_TABLE * pHash, IMG_VOID * pKey, IMG_UINTPTR_T v)
{
	BUCKET *pBucket;

	PVR_ASSERT(pHash != IMG_NULL);

	if (pHash == IMG_NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "HASH_Insert_Extended: invalid parameter"));
		return IMG_FALSE;
	}

	pBucket = OSAllocMem(sizeof(BUCKET) + pHash->uKeySize);
	if (pBucket == IMG_NULL) {
		return IMG_FALSE;
	}

	pBucket->v = v;
	/* PRQA S 0432,0541 1 *//* ignore warning about dynamic array k (linux) */
	OSMemCopy(pBucket->k, pKey, pHash->uKeySize);
	if (_ChainInsert(pHash, pBucket, pHash->ppBucketTable, pHash->uSize) !=
	    PVRSRV_OK) {
		OSFreeMem(pBucket);
		return IMG_FALSE;
	}

	pHash->uCount++;

	/* check if we need to think about re-balencing */
	if (pHash->uCount << 1 > pHash->uSize) {
		/* Ignore the return code from _Resize because the hash table is
		   still in a valid state and although not ideally sized, it is still
		   functional */
		_Resize(pHash, pHash->uSize << 1);
	}

	return IMG_TRUE;
}

									    /*************************************************************************//*!
									       @Function       HASH_Insert
									       @Description    Insert a key value pair into a hash table created with
									       HASH_Create.
									       @Input          pHash     Hash table
									       @Input          k         The key value.
									       @Input          v         The value associated with the key.
									       @Return         IMG_TRUE - success.
									       IMG_FALSE - failure.
    *//**************************************************************************/
IMG_INTERNAL IMG_BOOL
HASH_Insert(HASH_TABLE * pHash, IMG_UINTPTR_T k, IMG_UINTPTR_T v)
{
	return HASH_Insert_Extended(pHash, &k, v);
}

									    /*************************************************************************//*!
									       @Function       HASH_Remove_Extended
									       @Description    Remove a key from a hash table created with
									       HASH_Create_Extended.
									       @Input          pHash     Hash table
									       @Input          pKey      Pointer to key.
									       @Return         0 if the key is missing, or the value associated with the key.
    *//**************************************************************************/
IMG_INTERNAL IMG_UINTPTR_T
HASH_Remove_Extended(HASH_TABLE * pHash, IMG_VOID * pKey)
{
	BUCKET **ppBucket;
	IMG_UINT32 uIndex;

	PVR_ASSERT(pHash != IMG_NULL);

	if (pHash == IMG_NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "HASH_Remove_Extended: Null hash table"));
		return 0;
	}

	uIndex = KEY_TO_INDEX(pHash, pKey, pHash->uSize);

	for (ppBucket = &(pHash->ppBucketTable[uIndex]); *ppBucket != IMG_NULL;
	     ppBucket = &((*ppBucket)->pNext)) {
		/* PRQA S 0432,0541 1 *//* ignore warning about dynamic array k */
		if (KEY_COMPARE(pHash, (*ppBucket)->k, pKey)) {
			BUCKET *pBucket = *ppBucket;
			IMG_UINTPTR_T v = pBucket->v;
			(*ppBucket) = pBucket->pNext;

			OSFreeMem(pBucket);
			/*not nulling original pointer, already overwritten */

			pHash->uCount--;

			/* check if we need to think about re-balencing */
			if (pHash->uSize > (pHash->uCount << 2) &&
			    pHash->uSize > pHash->uMinimumSize) {
				/* Ignore the return code from _Resize because the
				   hash table is still in a valid state and although
				   not ideally sized, it is still functional */
				_Resize(pHash,
					PRIVATE_MAX(pHash->uSize >> 1,
						    pHash->uMinimumSize));
			}

			return v;
		}
	}
	return 0;
}

									    /*************************************************************************//*!
									       @Function       HASH_Remove
									       @Description    Remove a key value pair from a hash table created
									       with HASH_Create.
									       @Input          pHash     Hash table
									       @Input          k         The key
									       @Return         0 if the key is missing, or the value associated with the key.
    *//**************************************************************************/
IMG_INTERNAL IMG_UINTPTR_T HASH_Remove(HASH_TABLE * pHash, IMG_UINTPTR_T k)
{
	return HASH_Remove_Extended(pHash, &k);
}

									    /*************************************************************************//*!
									       @Function       HASH_Retrieve_Extended
									       @Description    Retrieve a value from a hash table created with
									       HASH_Create_Extended.
									       @Input          pHash     Hash table
									       @Input          pKey      Pointer to the key.
									       @Return         0 if the key is missing, or the value associated with the key.
    *//**************************************************************************/
IMG_INTERNAL IMG_UINTPTR_T
HASH_Retrieve_Extended(HASH_TABLE * pHash, IMG_VOID * pKey)
{
	BUCKET **ppBucket;
	IMG_UINT32 uIndex;

	PVR_ASSERT(pHash != IMG_NULL);

	if (pHash == IMG_NULL) {
		PVR_DPF((PVR_DBG_ERROR,
			 "HASH_Retrieve_Extended: Null hash table"));
		return 0;
	}

	uIndex = KEY_TO_INDEX(pHash, pKey, pHash->uSize);

	for (ppBucket = &(pHash->ppBucketTable[uIndex]); *ppBucket != IMG_NULL;
	     ppBucket = &((*ppBucket)->pNext)) {
		/* PRQA S 0432,0541 1 *//* ignore warning about dynamic array k */
		if (KEY_COMPARE(pHash, (*ppBucket)->k, pKey)) {
			BUCKET *pBucket = *ppBucket;
			IMG_UINTPTR_T v = pBucket->v;

			return v;
		}
	}
	return 0;
}

									    /*************************************************************************//*!
									       @Function       HASH_Retrieve
									       @Description    Retrieve a value from a hash table created with
									       HASH_Create.
									       @Input          pHash     Hash table
									       @Input          k         The key
									       @Return         0 if the key is missing, or the value associated with the key.
    *//**************************************************************************/
IMG_INTERNAL IMG_UINTPTR_T HASH_Retrieve(HASH_TABLE * pHash, IMG_UINTPTR_T k)
{
	return HASH_Retrieve_Extended(pHash, &k);
}

#ifdef HASH_TRACE
									    /*************************************************************************//*!
									       @Function       HASH_Dump
									       @Description    To dump the contents of a hash table in human readable
									       form.
									       @Input          pHash     Hash table
    *//**************************************************************************/
IMG_VOID HASH_Dump(HASH_TABLE * pHash)
{
	IMG_UINT32 uIndex;
	IMG_UINT32 uMaxLength = 0;
	IMG_UINT32 uEmptyCount = 0;

	PVR_ASSERT(pHash != IMG_NULL);
	for (uIndex = 0; uIndex < pHash->uSize; uIndex++) {
		BUCKET *pBucket;
		IMG_UINT32 uLength = 0;
		if (pHash->ppBucketTable[uIndex] == IMG_NULL) {
			uEmptyCount++;
		}
		for (pBucket = pHash->ppBucketTable[uIndex];
		     pBucket != IMG_NULL; pBucket = pBucket->pNext) {
			uLength++;
		}
		uMaxLength = PRIVATE_MAX(uMaxLength, uLength);
	}

	PVR_TRACE(("hash table: uMinimumSize=%d  size=%d  count=%d",
		   pHash->uMinimumSize, pHash->uSize, pHash->uCount));
	PVR_TRACE(("  empty=%d  max=%d", uEmptyCount, uMaxLength));
}
#endif
