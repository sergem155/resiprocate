#if !defined(SIPSTACK_HXX)
#define SIPSTACK_HXX

#include <set>

#include "sip2/util/Fifo.hxx"
#include "sip2/util/Socket.hxx"
#include "sip2/util/DataStream.hxx"

#include "sip2/sipstack/Executive.hxx"
#include "sip2/sipstack/TransportSelector.hxx"
#include "sip2/sipstack/TransactionMap.hxx"
#include "sip2/sipstack/TimerQueue.hxx"
#include "sip2/sipstack/Dialog.hxx"
#include "sip2/sipstack/DnsResolver.hxx"
#include "sip2/sipstack/SdpContents.hxx"
#include "sip2/sipstack/SipMessage.hxx"
#include "sip2/sipstack/Helper.hxx"

namespace Vocal2
{

class Data;
class Message;
class SipMessage;
class DnsResolver;
class Executive;
class TransportSelector;
class TransactionState;
class TestDnsResolver;
class TestFSM;
class Security;

	
class SipStack
{
   public:
      SipStack(bool multiThreaded=false);

      // Used by the application to add in a new transport
      // by default, you will get UDP and TCP on 5060 (for now)
      void addTransport( Transport::Type, int port, const Data& hostName="", const Data& nic="");

      // used to add an alias for this sip element. e.g. foobar.com and boo.com
      // are both handled by this proxy. 
      void addAlias(const Data& domain);
      
      // return true if domain is handled by this stack
      bool isMyDomain(const Data& domain) const;
      
      void send(const SipMessage& msg);

      // this is only if you want to send to a destination not in the route. You
      // probably don't want to use it. 
      void sendTo(const SipMessage& msg, const Data &dest="default" );

      // caller now owns the memory
      // returns 0 if nothing there
      SipMessage* receive(); 
      
      // should call buildFdSet before calling process
      // !jf! what should this do if fdSet = 0? 
      void process(FdSet& fdset);

      // build the FD set to use in a select to find out when process bust be called again
      void buildFdSet(FdSet& fdset);
	
      /// returns time in milliseconds when process next needs to be called 
      int getTimeTillNextProcess(); 

#ifdef USE_SSL
      /// if this object exists, it manages advanced security featues
      Security* security;
#endif

private:
	friend class DnsResolver;
	friend class Executive;
	friend class TransportSelector;
	friend class TransactionState;
	friend class TestDnsResolver;
	friend class TestFSM;
		
			
      Fifo<Message> mTUFifo;
      Fifo<Message> mStateMacFifo;

      Executive mExecutive;
      TransportSelector mTransportSelector;
      TransactionMap mTransactionMap;
      TimerQueue  mTimers;

      DnsResolver mDnsResolver;
      
      bool mDiscardStrayResponses;

      // store all domains that this stack is responsible for
      std::set<Data> mDomains;
};
 
}

#endif


/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * 3. The names "VOCAL", "Vovida Open Communication Application Library",
 *    and "Vovida Open Communication Application Library (VOCAL)" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact vocal@vovida.org.
 *
 * 4. Products derived from this software may not be called "VOCAL", nor
 *    may "VOCAL" appear in their name, without prior written
 *    permission of Vovida Networks, Inc.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
 * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 * ====================================================================
 * 
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks,
 * Inc.  For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>.
 *
 */