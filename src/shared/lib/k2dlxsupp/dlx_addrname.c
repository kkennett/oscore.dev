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
#include "idlx.h"

static char const * const sgpSegName[3] = 
{
    ".text",
    ".rodata",
    ".data"
};

K2STAT
DLX_FindAddrName(
    UINT32  aAddr,
    char *  apRetNameBuffer,
    UINT32  aBufferLen
)
{
    DLX *                   pDlx;
    UINT32                  segIx;
    K2STAT                  status;
    char const *            pBaseName;
    UINT32                  baseAddr;
    K2TREE_ANCHOR *         pAnchor;
    K2TREE_NODE *           pTreeNode;
    K2DLX_SYMTREE_NODE *    pSymTreeNode;

    if ((apRetNameBuffer == NULL) ||
        (aBufferLen == 0))
    {
        return K2STAT_ERROR_BAD_ARGUMENT;
    }

    status = DLX_AcquireContaining(aAddr, &pDlx, &segIx);
    if (K2STAT_IS_ERROR(status))
    {
        return status;
    }

    do
    {
        baseAddr = 0;

        if (pDlx->mFlags & K2DLXSUPP_FLAG_KEEP_SYMBOLS)
        {
            // find the closest matching symbol in the segment and set baseAddr and pBaseName
            pAnchor = &pDlx->SymTree[segIx - DlxSeg_Text];
            pTreeNode = K2TREE_FindOrAfter(pAnchor, aAddr);
            if (pTreeNode != NULL)
            {
                if (pTreeNode->mUserVal != aAddr)
                    pTreeNode = K2TREE_PrevNode(pAnchor, pTreeNode);
                if (pTreeNode != NULL)
                {
                    pSymTreeNode = K2_GET_CONTAINER(K2DLX_SYMTREE_NODE, pTreeNode, TreeNode);
                    baseAddr = pSymTreeNode->TreeNode.mUserVal;
                    pBaseName = pSymTreeNode->mpSymName;
                }
            }
        }

        if (baseAddr == 0)
        {
            pBaseName = sgpSegName[segIx - DlxSeg_Text];
            baseAddr = pDlx->SegAlloc.Segment[segIx].mLinkAddr;
        }

        aAddr -= baseAddr;
        if (aAddr == 0)
        {
            K2ASC_PrintfLen(apRetNameBuffer, aBufferLen, "%.*s|%s",
                pDlx->mIntNameLen, pDlx->mpIntName, pBaseName);
        }
        else
        {
            K2ASC_PrintfLen(apRetNameBuffer, aBufferLen, "%.*s|%s+0x%X",
                pDlx->mIntNameLen, pDlx->mpIntName, pBaseName, aAddr - baseAddr);
        }

    } while (0);

    DLX_Release(pDlx);

    return K2STAT_OK;
}

