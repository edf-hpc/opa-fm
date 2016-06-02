/* BEGIN_ICS_COPYRIGHT4 ****************************************

Copyright (c) 2015, Intel Corporation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

** END_ICS_COPYRIGHT4   ****************************************/
/*!

  \file subscribe.h

  $Revision: 1.8 $
  $Date: 2016/04/06 10:49:29 $

  \brief Routines to handle subscribing/unsubscribing to trap events
*/

#ifndef _SUBSCRIBE_H_
#define _SUBSCRIBE_H_

#include <stl_types.h>
#include <stl_sd.h>
#include <sdi.h>

#ifdef __cplusplus
extern "C" {
#endif

void SubscribeInitialize(void);

void SubscribeTerminate(void);

void
UnsubscribeClient(ClientListEntry *pClientEntry);

void InvalidatePortTrapSubscriptions(EUI64 PortGuid);

void ReRegisterTrapSubscriptions(EUI64 PortGuid);

void ProcessClientTraps(void *pContext, IB_NOTICE *pNotice, EUI64 PortGuid);

#ifdef __cplusplus
};
#endif

#endif  // _SUBSCRIBE_H
