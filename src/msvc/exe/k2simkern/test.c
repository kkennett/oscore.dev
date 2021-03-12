#include <lib/k2win32.h>
#include <lib/k2ramheap.h>

static UINT32 sLock(K2RAMHEAP *apRamHeap)
{
    return 0;
}

static void   sUnlock(K2RAMHEAP *apRamHeap, UINT32 aDisp)
{

}

static K2STAT sCreateRange(UINT32 aTotalPageCount, UINT32 aInitCommitPageCount, void **appRetRange, UINT32 *apRetStartAddr)
{
    UINT32 start;
    UINT32 alloc;

    start = (UINT32)VirtualAlloc(NULL, aTotalPageCount * K2_VA32_MEMPAGE_BYTES, MEM_RESERVE, PAGE_NOACCESS);
    if (0 == start)
        return K2STAT_ERROR_UNKNOWN;

    if (aInitCommitPageCount > 0)
    {
        alloc = (UINT32)VirtualAlloc((void *)start, aInitCommitPageCount * K2_VA32_MEMPAGE_BYTES, MEM_COMMIT, PAGE_READWRITE);
        if (0 == alloc)
        {
            VirtualFree((void *)start, 0, MEM_RELEASE);
            return K2STAT_ERROR_UNKNOWN;
        }
        K2_ASSERT(alloc == start);
    }
    *appRetRange = (void *)start;
    *apRetStartAddr = start;
    return K2STAT_NO_ERROR;
}

static K2STAT sCommitPages(void *aRange, UINT32 aAddr, UINT32 aPageCount)
{
    UINT32 alloc;

    alloc = (UINT32)VirtualAlloc((void *)aAddr, aPageCount * K2_VA32_MEMPAGE_BYTES, MEM_COMMIT, PAGE_READWRITE);
    if (0 == alloc)
        return K2STAT_ERROR_UNKNOWN;
    K2_ASSERT(alloc == aAddr);
    return K2STAT_NO_ERROR;
}

static K2STAT sDecommitPages(void *aRange, UINT32 aAddr, UINT32 aPageCount)
{
    if (!VirtualFree((void *)aAddr, aPageCount * K2_VA32_MEMPAGE_BYTES, MEM_DECOMMIT))
        return K2STAT_ERROR_UNKNOWN;
    return K2STAT_NO_ERROR;
}

static K2STAT sDestroyRange(void *aRange)
{
    if (!VirtualFree(aRange, 0, MEM_RELEASE))
        return K2STAT_ERROR_UNKNOWN;
    return K2STAT_NO_ERROR;
}

K2RAMHEAP       sgHeap;
K2RAMHEAP_SUPP  sgHeapSupp =
{
    sLock,
    sUnlock,
    sCreateRange,
    sCommitPages,
    sDecommitPages,
    sDestroyRange
};

void RunTest(void)
{
    K2RAMHEAP_Init(&sgHeap, &sgHeapSupp);

    void *ptr;
    K2STAT stat;
    stat = K2RAMHEAP_Alloc(&sgHeap, 40, TRUE, &ptr);

    K2RAMHEAP_Free(&sgHeap, ptr);
}