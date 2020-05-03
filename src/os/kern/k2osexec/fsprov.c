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

// {0440C404-CAD6-4333-A23D-24FDBF335DA8}
K2_GUID128 const    gK2OSEXEC_FsProvInterfaceGuid       = K2OSEXEC_FSPROV_INTERFACE_ID;
char const *        gpK2OSEXEC_FsProvInterfaceGuidStr   = "{0440C404-CAD6-4333-A23D-24FDBF335DA8}";

static K2OS_TOKEN sgTokSubscrip;

void
sIfPopCallback(
    K2_GUID128 const *          apInterfaceId,
    void *                      apContext,
    K2OSKERN_SVC_IFINST const * apInterfaceInstance,
    BOOL                        aIsArrival
)
{
    K2_ASSERT(0 == K2MEM_Compare(apInterfaceId, &gK2OSEXEC_FsProvInterfaceGuid, sizeof(K2_GUID128)));
    K2_ASSERT(apContext == NULL);

    if (aIsArrival)
    {
        //
        // file system provider interface was published
        //
        K2OSKERN_Debug("ARRIVE - FILESYS PROVIDER: %d / %d\n",
            apInterfaceInstance->mServiceInstanceId,
            apInterfaceInstance->mInterfaceInstanceId);
    }
    else
    {
        //
        // file system provider interface left
        //
        K2OSKERN_Debug("DEPART - FILESYS PROVIDER: %d / %d\n",
            apInterfaceInstance->mServiceInstanceId,
            apInterfaceInstance->mInterfaceInstanceId);
    }
}

void FsProv_Init(void)
{
    sgTokSubscrip = K2OSKERN_InterfaceSubscribe(&gK2OSEXEC_FsProvInterfaceGuid, sIfPopCallback, NULL);
    K2_ASSERT(sgTokSubscrip != NULL);
}