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

#include "ik2osexec.h"

K2_GUID128 const gK2OSEXEC_MailslotGuid = 
// {51646997-5DE4-4175-AC0B-94BF2B57C0F4}
{ 0x51646997, 0x5de4, 0x4175, { 0xac, 0xb, 0x94, 0xbf, 0x2b, 0x57, 0xc0, 0xf4 } };

char const * gpK2OSEXEC_MailslotGuidStr =
"{51646997-5DE4-4175-AC0B-94BF2B57C0F4}";

static K2OS_TOKEN sgTokMailslot;

void sRecvNotify(UINT32 aOpCode, UINT32 *apParam)
{
    if ((aOpCode & SYSMSG_OPCODE_HIGH_MASK) != SYSMSG_OPCODE_HIGH)
        return;

    K2OSKERN_ReflectNotify(aOpCode, apParam);
}

K2STAT sRecvCall(UINT32 aOpCode, UINT32 *apParam)
{
    if ((aOpCode & SYSMSG_OPCODE_HIGH_MASK) != SYSMSG_OPCODE_HIGH)
        return K2STAT_ERROR_BAD_ARGUMENT;

    K2OSKERN_Debug("K2OSEXEC:RecvCall(%d)\n", aOpCode);

    return K2STAT_ERROR_NOT_IMPL;
}

UINT32 K2_CALLCONV_REGS sMsgThread(void *apParam)
{
    K2OS_TOKEN  tokName;
    K2OS_TOKEN  tokMailbox;
    K2OS_MSGIO  msgIo;
    UINT32      requestId;
    UINT32      waitResult;
    BOOL        ok;

    if (!K2OS_MailboxCreate(1, &tokMailbox))
    {
        K2OSKERN_Panic("Could not create k2osexec mailbox\n");
    }

    tokName = K2OS_NameDefine(gpK2OSEXEC_MailslotGuidStr);
    if (tokName == NULL)
    {
        K2OSKERN_Panic("Could not create k2osexec mailslot name\n");
    }

    sgTokMailslot = K2OS_MailslotCreate(tokName, 1, &tokMailbox, FALSE);
    if (sgTokMailslot == NULL)
    {
        K2OSKERN_Panic("Could not create k2osexec mailslot\n");
    }

    do {
        waitResult = K2OS_ThreadWaitOne(tokMailbox, K2OS_TIMEOUT_INFINITE);
        K2_ASSERT(waitResult == K2OS_WAIT_SIGNALLED_0);
        requestId = 0;
        ok = K2OS_MailboxRecv(tokMailbox, &msgIo, &requestId);
        K2_ASSERT(ok);
        if (requestId == 0)
            sRecvNotify(msgIo.mOpCode, msgIo.mPayload);
        else
        {
            msgIo.mStatus = sRecvCall(msgIo.mOpCode, msgIo.mPayload);
            ok = K2OS_MailboxRespond(tokMailbox, requestId, &msgIo);
            K2_ASSERT(ok);
        }
    } while (1);

    //
    // should never reach here
    //

    return 0;
}

void Msg_Init(void)
{
    K2OS_TOKEN          tokThread;
    K2OS_THREADCREATE   cret;

    K2MEM_Zero(&cret, sizeof(cret));
    cret.mStructBytes = sizeof(cret);
    cret.mEntrypoint = sMsgThread;

    tokThread = K2OS_ThreadCreate(&cret);
    K2_ASSERT(NULL != tokThread);

    K2OS_TokenDestroy(tokThread);
}
 
