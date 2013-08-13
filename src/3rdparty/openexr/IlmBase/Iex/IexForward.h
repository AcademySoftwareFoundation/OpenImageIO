///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2012, Industrial Light & Magic, a division of Lucas
// Digital Ltd. LLC
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Industrial Light & Magic nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#ifndef INCLUDED_IEXFORWARD_H
#define INCLUDED_IEXFORWARD_H

#include "IexNamespace.h"

IEX_INTERNAL_NAMESPACE_HEADER_ENTER

//
// Base exceptions.
//

class BaseExc;
class ArgExc;
class LogicExc;
class InputExc;
class IoExc;
class MathExc;
class ErrnoExc;
class NoImplExc;
class NullExc;
class TypeExc;

//
// Math exceptions.
//

class OverflowExc;
class UnderflowExc;
class DivzeroExc;
class InexactExc;
class InvalidFpOpExc;

//
// Errno exceptions.
//

class EpermExc;
class EnoentExc;
class EsrchExc;
class EintrExc;
class EioExc;
class EnxioExc;
class E2bigExc;
class EnoexecExc;
class EbadfExc;
class EchildExc;
class EagainExc;
class EnomemExc;
class EaccesExc;
class EfaultExc;
class EnotblkExc;
class EbusyExc;
class EexistExc;
class ExdevExc;
class EnodevExc;
class EnotdirExc;
class EisdirExc;
class EinvalExc;
class EnfileExc;
class EmfileExc;
class EnottyExc;
class EtxtbsyExc;
class EfbigExc;
class EnospcExc;
class EspipeExc;
class ErofsExc;
class EmlinkExc;
class EpipeExc;
class EdomExc;
class ErangeExc;
class EnomsgExc;
class EidrmExc;
class EchrngExc;
class El2nsyncExc;
class El3hltExc;
class El3rstExc;
class ElnrngExc;
class EunatchExc;
class EnocsiExc;
class El2hltExc;
class EdeadlkExc;
class EnolckExc;
class EbadeExc;
class EbadrExc;
class ExfullExc;
class EnoanoExc;
class EbadrqcExc;
class EbadsltExc;
class EdeadlockExc;
class EbfontExc;
class EnostrExc;
class EnodataExc;
class EtimeExc;
class EnosrExc;
class EnonetExc;
class EnopkgExc;
class EremoteExc;
class EnolinkExc;
class EadvExc;
class EsrmntExc;
class EcommExc;
class EprotoExc;
class EmultihopExc;
class EbadmsgExc;
class EnametoolongExc;
class EoverflowExc;
class EnotuniqExc;
class EbadfdExc;
class EremchgExc;
class ElibaccExc;
class ElibbadExc;
class ElibscnExc;
class ElibmaxExc;
class ElibexecExc;
class EilseqExc;
class EnosysExc;
class EloopExc;
class ErestartExc;
class EstrpipeExc;
class EnotemptyExc;
class EusersExc;
class EnotsockExc;
class EdestaddrreqExc;
class EmsgsizeExc;
class EprototypeExc;
class EnoprotooptExc;
class EprotonosupportExc;
class EsocktnosupportExc;
class EopnotsuppExc;
class EpfnosupportExc;
class EafnosupportExc;
class EaddrinuseExc;
class EaddrnotavailExc;
class EnetdownExc;
class EnetunreachExc;
class EnetresetExc;
class EconnabortedExc;
class EconnresetExc;
class EnobufsExc;
class EisconnExc;
class EnotconnExc;
class EshutdownExc;
class EtoomanyrefsExc;
class EtimedoutExc;
class EconnrefusedExc;
class EhostdownExc;
class EhostunreachExc;
class EalreadyExc;
class EinprogressExc;
class EstaleExc;
class EioresidExc;
class EucleanExc;
class EnotnamExc;
class EnavailExc;
class EisnamExc;
class EremoteioExc;
class EinitExc;
class EremdevExc;
class EcanceledExc;
class EnolimfileExc;
class EproclimExc;
class EdisjointExc;
class EnologinExc;
class EloginlimExc;
class EgrouploopExc;
class EnoattachExc;
class EnotsupExc;
class EnoattrExc;
class EdircorruptedExc;
class EdquotExc;
class EnfsremoteExc;
class EcontrollerExc;
class EnotcontrollerExc;
class EenqueuedExc;
class EnotenqueuedExc;
class EjoinedExc;
class EnotjoinedExc;
class EnoprocExc;
class EmustrunExc;
class EnotstoppedExc;
class EclockcpuExc;
class EinvalstateExc;
class EnoexistExc;
class EendofminorExc;
class EbufsizeExc;
class EemptyExc;
class EnointrgroupExc;
class EinvalmodeExc;
class EcantextentExc;
class EinvaltimeExc;
class EdestroyedExc;

IEX_INTERNAL_NAMESPACE_HEADER_EXIT

#endif // INCLUDED_IEXFORWARD_H
