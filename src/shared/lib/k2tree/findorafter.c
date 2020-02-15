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
#include <lib/k2tree.h>

K2TREE_NODE * 
K2TREE_FindOrAfter(
    K2TREE_ANCHOR * apAnchor,
    UINT32          aFindKey
)
{
    K2TREE_NODE *   pCur;
    K2TREE_NODE *   pNext;
    K2TREE_NODE *   nil;

    K2_ASSERT(apAnchor != NULL);

    nil = &apAnchor->NilNode;

    pCur = apAnchor->RootNode.mpLeftChild;

    if (pCur == nil)
        return NULL;

    do
    {
        int rc = apAnchor->mfCompareKeyToNode(aFindKey, pCur);
        if (rc == 0)
            return pCur;
        if (rc < 0)
        {
            /* looking for key before current key.
               if there isn't one then there isn't a node "at or after"
               the key we are searching for */
            pNext = pCur->mpLeftChild;
            if (pNext == nil)
                return pCur;
        }
        else
        {
            pNext = pCur->mpRightChild;
            if (pNext == nil)
            {
                /* return successor to pCur */
                return K2TREE_NextNode(apAnchor, pCur);
            }
        }
        pCur = pNext;
    } while (pCur != nil);

    return NULL;
}

