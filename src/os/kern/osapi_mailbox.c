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

#include "kern.h"

BOOL K2_CALLCONV_CALLERCLEANS K2OS_MailboxCreate(UINT32 aMailboxCount, K2OS_TOKEN *apRetTokMailboxes)
{
    K2OSKERN_OBJ_MAILBOX ** ppMailbox;
    K2OS_TOKEN *            pTokens;
    UINT32                  ix;
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_MAILBOX *  pMailbox;
    BOOL                    ok;

    if ((aMailboxCount == 0) ||
        (apRetTokMailboxes == NULL))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    stat = K2STAT_NO_ERROR;
    K2MEM_Zero(apRetTokMailboxes, aMailboxCount * sizeof(K2OS_TOKEN));

    ppMailbox = (K2OSKERN_OBJ_MAILBOX **)K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_MAILBOX *) * aMailboxCount);
    if (ppMailbox == NULL)
        return FALSE;

    K2MEM_Zero(ppMailbox, sizeof(K2OSKERN_OBJ_MAILBOX *) * aMailboxCount);

    do {
        pTokens = (K2OS_TOKEN *)K2OS_HeapAlloc(sizeof(K2OS_TOKEN) * aMailboxCount);
        if (pTokens == NULL)
        {
            stat = K2OS_ThreadGetStatus();
            break;
        }

        K2MEM_Zero(pTokens, sizeof(K2OS_TOKEN) * aMailboxCount);

        do {
            for (ix = 0; ix < aMailboxCount; ix++)
            {
                pMailbox = (K2OSKERN_OBJ_MAILBOX *)K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_MAILBOX));
                if (pMailbox == NULL)
                {
                    stat = K2OS_ThreadGetStatus();
                    break;
                }

                stat = KernMailbox_Create(pMailbox, ix);
                if (K2STAT_IS_ERROR(stat))
                    break;

                ppMailbox[ix] = pMailbox;
            }
            if (ix < aMailboxCount)
            {
                if (ix > 0)
                {
                    do {
                        --ix;
                        stat2 = KernObj_Release(&ppMailbox[ix]->Hdr);
                        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
                        ppMailbox[ix] = NULL;
                    } while (ix > 0);
                }
                K2_ASSERT(K2STAT_IS_ERROR(stat));
                break;
            }

            stat = KernTok_CreateNoAddRef(aMailboxCount, (K2OSKERN_OBJ_HEADER **)ppMailbox, pTokens);
            if (K2STAT_IS_ERROR(stat))
            {
                for (ix = 0; ix < aMailboxCount; ix++)
                {
                    stat2 = KernObj_Release(&ppMailbox[ix]->Hdr);
                    K2_ASSERT(!K2STAT_IS_ERROR(stat2));
                    ppMailbox[ix] = NULL;
                }
            }
            else
            {
                K2MEM_Copy(apRetTokMailboxes, pTokens, sizeof(K2OS_TOKEN) * aMailboxCount);
            }

        } while (0);

        ok = K2OS_HeapFree(pTokens);
        K2_ASSERT(ok);

    } while (0);

    ok = K2OS_HeapFree(ppMailbox);
    K2_ASSERT(ok);

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_MailboxRecv(K2OS_TOKEN aTokMailbox, K2OS_MSGIO *apRetMsgIo, UINT32 *apRetRequestId)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_MailboxRespond(K2OS_TOKEN aTokMailbox, UINT32 aRequestId, K2OS_MSGIO const *apRespIo)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}


