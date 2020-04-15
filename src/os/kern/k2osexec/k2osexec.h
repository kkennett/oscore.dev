//   
//   BSD 3-Clause License
//   
//   Copyright (c) 2020, Kurt Kennett
//   All rights reserved.
//   
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are met:
//   
//   1. Redistributions of source code must retain the above copyright notice, this
//      list of conditions and the following disclaimer.
//   
//   2. Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the documentation
//      and/or other materials provided with the distribution.
//   
//   3. Neither the name of the copyright holder nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//   
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef __K2OSEXEC_H
#define __K2OSEXEC_H

#include <k2oskern.h>
#include "..\k2osacpi\k2osacpi.h"
#include "..\kernexec.h"
#include <lib/k2heap.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------------- */

typedef struct _PHYS_HEAPNODE   PHYS_HEAPNODE;
typedef struct _PCI_SEGMENT     PCI_SEGMENT;
typedef struct _PCI_DEVICE      PCI_DEVICE;
typedef struct _DEV_NODE_PCI    DEV_NODE_PCI;
typedef struct _DEV_NODE        DEV_NODE;

/* ----------------------------------------------------------------------------- */

extern ACPI_TABLE_MADT * gpMADT;

/* ----------------------------------------------------------------------------- */

void Handlers_Init1(void);
void Handlers_Init2(void);

/* ----------------------------------------------------------------------------- */

struct _PCI_SEGMENT
{
    ACPI_MCFG_ALLOCATION *  mpMcfgAlloc;
    PHYS_HEAPNODE *         mpPhysHeapNode;
    K2TREE_ANCHOR           PciDevTree;
    K2LIST_LINK             PciSegListLink;
};

extern K2LIST_ANCHOR        gPci_SegList;
extern K2OSKERN_SEQLOCK     gPci_SeqLock;

void Pci_Init(void);
void Pci_CheckManualScan(void);
void Pci_DiscoverBridgeFromAcpi(DEV_NODE *apDevNode);
void Pci_DumpRes(DEV_NODE_PCI *apPci);

/* ----------------------------------------------------------------------------- */

#if K2_TARGET_ARCH_IS_INTEL

void X32PIT_InitTo1Khz(void);

#endif

extern UINT32  gTime_BusClockRate;

void
Time_Start(
    K2OSEXEC_INIT_INFO * apInitInfo
);

/* ----------------------------------------------------------------------------- */

struct _PHYS_HEAPNODE
{
    K2HEAP_NODE HeapNode;   // must go first
    UINT32      mDisp;
};

#define PHYS_DISP_ERROR         0
#define PHYS_DISP_MAPPEDIO      1
#define PHYS_DISP_MEMORY        2
#define PHYS_DISP_MAPPEDPORT    3
#define PHYS_DISP_PCI_SEGMENT   4

extern K2HEAP_ANCHOR        gPhys_SpaceHeap;
extern K2OS_CRITSEC         gPhys_SpaceSec;

void 
Phys_Init(
    K2OSEXEC_INIT_INFO * apInitInfo
);

/* ----------------------------------------------------------------------------- */

struct _DEV_NODE_PCI
{
    DEV_NODE *      mpDevNode;
    ACPI_PCI_ID     Id;
    ACPI_HANDLE     mhAcpiDevice;
    UINT32          mVenLo_DevHi;
    PCI_SEGMENT *   mpSeg;
    UINT32          mVirtConfigAddr;    // 0 if on intel system with no ECAM
    K2TREE_NODE     PciTreeNode;
};

#define DEV_NODE_PCIBUSBRIDGE_ISBUSROOT     0x80000000
#define DEV_NODE_PCIBUSBRIDGE_LEGACY        0x40000000
#define DEV_NODE_PCIBUSBRIDGE_BUSID_MASK    0x00FF0000
#define DEV_NODE_PCIBUSBRIDGE_BUSID_SHIFT   16
#define DEV_NODE_PCIBUSBRIDGE_SEGID_MASK    0x0000FFFF
#define DEV_NODE_PCIBUSBRIDGE_SEGID_SHIFT   0

struct _DEV_NODE
{
    K2TREE_NODE         DevTreeNode;   // should be first thing to make casts fast

    DEV_NODE *          mpParent;
    K2LIST_ANCHOR       ChildList;
    K2LIST_LINK         ChildListLink;

    // ACPI_HANDLE of object is TreeNode.mUserVal, indexing gDev_Tree
    // devices with no acpi node are indexed at zero

    ACPI_DEVICE_INFO *  mpAcpiInfo;

    DEV_NODE_PCI *      mpPci;
    UINT32              mPciBusBridgeFlags;
};

void 
Dev_Init(
    void
);

extern K2TREE_ANCHOR    gDev_Tree;
extern K2OSKERN_SEQLOCK gDev_SeqLock;
extern DEV_NODE *       gpDev_RootNode;

/* ----------------------------------------------------------------------------- */

void Res_Init(void);

/* ----------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif // __K2OSEXEC_H