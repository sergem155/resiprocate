
#include <cassert>

#include "sip2/util/Socket.hxx"

#include "sip2/sipstack/SipStack.hxx"
#include "sip2/util/Data.hxx"
#include "sip2/util/Logger.hxx"
#include "sip2/util/Random.hxx"
#include "sip2/sipstack/TuIM.hxx"
#include "sip2/sipstack/Contents.hxx"
#include "sip2/sipstack/ParserCategories.hxx"
#include "sip2/sipstack/PlainContents.hxx"
#include "sip2/sipstack/Pkcs7Contents.hxx"
#include "sip2/sipstack/Security.hxx"
#include "sip2/sipstack/Helper.hxx"
#include "sip2/sipstack/Pidf.hxx"

#define VOCAL_SUBSYSTEM Subsystem::SIP

using namespace Vocal2;


void
TuIM::ErrCallback::sendPageFailed(const Uri& dest )
{
   assert(0);
}


TuIM::ErrCallback::~ErrCallback()
{
}


TuIM::PageCallback::~PageCallback()
{
}


TuIM::TuIM(SipStack* stack, 
           const Uri& aor, 
           const Uri& contact,
           PageCallback* msgCallback, 
           ErrCallback* errCallback,
           PresCallback* pressCallback)
   : mPageCallback(msgCallback),
     mErrCallback(errCallback),
     mPressCallback(pressCallback),
     mStack(stack),
     mAor(aor),
     mContact(contact),
     mPassword( Data::Empty ),
     mPidf( new Pidf ),
     mRegistrationDialog(NameAddr(contact)),
     mNextTimeToRegister(0)
{
   assert( mStack );
   assert(mPageCallback);
   assert(mErrCallback);
   assert(mPidf);
   
   mPidf->setSimpleId( Random::getRandomHex(4) );  
   mPidf->setEntity( mAor.getAor() );  
   mPidf->setSimpleStatus( true, Data::Empty, mContact.getAor() );
   
  
}


TuIM::PresCallback::~PresCallback()
{
}


void TuIM::sendPage(const Data& text, const Uri& dest, bool sign, const Data& encryptFor)
{
   if ( text.empty() )
   {
      DebugLog( << "tried to send blank message - dropped " );
      return;
   }
   DebugLog( << "send to <" << dest << ">" << "\n" << text );

   NameAddr target;
   target.uri() = dest;
   
   NameAddr from;
   from.uri() = mAor;

   NameAddr contact;
   contact.uri() = mContact;
      
   SipMessage* msg = Helper::makeRequest(target, from, contact, MESSAGE);
   assert( msg );

   PlainContents plainBody(text);
   Contents* body = &plainBody;
   
#if USE_SSL
   if ( sign )
   {
      Security* sec = mStack->security;
      assert(sec);
    
      Contents* old = body;
      body = sec->sign( body );
      if (old!=&plainBody) delete old;
   }
   
   if ( !encryptFor.empty() )
   {
      Security* sec = mStack->security;
      assert(sec);
      
      Contents* old = body;
      body = sec->encrypt( body, encryptFor );
      if (old!=&plainBody) delete old;
   }
#endif

   msg->setContents(body);
   mStack->send( *msg );

   if ( body != &plainBody )
   {
      delete body;
   }
}


void
TuIM::processRequest(SipMessage* msg)
{
   if ( msg->header(h_RequestLine).getMethod() == MESSAGE )
   {
      processMessageRequest(msg);
      return;
   }
   if ( msg->header(h_RequestLine).getMethod() == SUBSCRIBE )
   {
      processSubscribeRequest(msg);
      return;
   }
   if ( msg->header(h_RequestLine).getMethod() == NOTIFY )
   {
      processNotifyRequest(msg);
      return;
   }

   InfoLog( "Got request that was not handled" );
}


void 
TuIM::processSubscribeRequest(SipMessage* msg)
{
   assert( msg->header(h_RequestLine).getMethod() == SUBSCRIBE );
   CallId id = msg->header(h_CallId);
   
   const int maxExpires = 3600;
   int expires=maxExpires;
   if ( msg->exists(h_Expires) )
   {
      expires = msg->header(h_Expires).value();
   }
   if (expires > maxExpires )
   {
      expires = maxExpires;
   }
   
   Dialog* dialog = NULL;

   // see if we already have this subscription
   for ( unsigned int i=0; i< mSubscribers.size(); i++)
   {
      Dialog* d = mSubscribers[i];
      assert( d );
      
      if ( d->getCallId() == id )
      {
         // found the subscrition in list of current subscrbtions 
         dialog = d;
         break;
      }
   }
   
   if ( !dialog )
   {
      // create a new subscriber 
      DebugLog ( << "Creating new subscrition dialog ");
      
      dialog = new Dialog( NameAddr(mContact) );

      mSubscribers.push_back( dialog );
   }
   
   auto_ptr<SipMessage> response( dialog->makeResponse( *msg, 200 ));
 
   response->header(h_Expires).value() = expires;
   response->header(h_Event).value() = Data("presence");
   
   mStack->send( *response );

   sendNotify( dialog );
}


void 
TuIM::processNotifyRequest(SipMessage* msg)
{
   assert( mPressCallback );
   assert( msg->header(h_RequestLine).getMethod() == NOTIFY ); 

   auto_ptr<SipMessage> response( Helper::makeResponse( *msg, 200 ));
   mStack->send( *response );

   Uri from = msg->header(h_From).uri();
   DebugLog ( << "got notify from " << from );

   Contents* contents = msg->getContents();
   if ( !contents )
   {
      ErrLog( "Received NOTIFY message event with no contents" );
      mPressCallback->presenseUpdate( from, true, Data::Empty );
      return;
   }

   Mime mime = contents->getType();
   DebugLog ( << "got  NOTIFY event with body of type  " << mime.type() << "/" << mime.subType() );
  
   Pidf* body = dynamic_cast<Pidf*>(contents);
   if ( !body )
   {
      ErrLog( "Received NOTIFY message event with no PIDF contents" );
      mPressCallback->presenseUpdate( from, true, Data::Empty );
      return;
   }
 
   Data note;
   bool open = body->getSimpleStatus( &note );;
   mPressCallback->presenseUpdate( from, open, note );
}


void 
TuIM::processMessageRequest(SipMessage* msg)
{
   assert( msg->header(h_RequestLine).getMethod() == MESSAGE );
   
   NameAddr contact; 
   contact.uri() = mContact;
            
   SipMessage* response = Helper::makeResponse(*msg, 200, contact, "OK");
   mStack->send( *response );
   delete response; response=0;
               
   Contents* contents = msg->getContents();
   if ( !contents )
   {
      ErrLog( "Receiveed Message message with no contents" );
      delete msg; msg=0;
      return;
   }

   assert( contents );
   Mime mime = contents->getType();
   DebugLog ( << "got body of type  " << mime.type() << "/" << mime.subType() );

   Data signedBy;
   Security::SignatureStatus sigStat = Security::none;
   bool encrypted=false;

#ifdef USE_SSL
   Pkcs7Contents* sBody = dynamic_cast<Pkcs7Contents*>(contents);
   if ( sBody )
   {
      assert( sBody );
      Security* sec = mStack->security;
      assert(sec);

      contents = sec->uncode( sBody, &signedBy, &sigStat, &encrypted );
      if ( !contents )
      {
         ErrLog( << "Some problem decoding SMIME message");
      }
   }
#endif

   if ( contents )
   {
      PlainContents* body = dynamic_cast<PlainContents*>(contents);
      if ( body )
      {
         assert( body );
         const Data& text = body->text();
         DebugLog ( << "got message from with text of <" << text << ">" );
                 
         Uri from = msg->header(h_From).uri();
         DebugLog ( << "got message from " << from );
                  
         assert( mPageCallback );
         mPageCallback->receivedPage( text, from, signedBy, sigStat, encrypted );
      }
      else
      {
         ErrLog ( << "Can not handle type " << contents->getType() );
         assert(0);
      }
   }
}


void
TuIM::processResponse(SipMessage* msg)
{  
   CallId id = msg->header(h_CallId);
   
   // see if it is a registraition response 
   if ( id == mRegistrationDialog.getCallId() )
   {
      DebugLog ( << "matched the reg dialog" <<  mRegistrationDialog.getCallId() << " = " << id  );
      processRegisterResponse( msg );
      return;
   }
   
   // see if it is a subscribe response 
   for ( int i=0; i<getNumBuddies(); i++)
   {
      Buddy& b = mBuddy[i];
      assert(  b.presDialog );
      if ( b.presDialog->getCallId() == id  )
      {
         DebugLog ( << "matched the sub dialog" );
         processSubscribeResponse( msg, b );
         return;
      }
   }
   
   // likely is a response for some IM thing 
   int number = msg->header(h_StatusLine).responseCode();
   DebugLog ( << "got response of type " << number );
   
   if ( number >= 300 )
   {
      Uri dest = msg->header(h_To).uri();
      mErrCallback->sendPageFailed( dest );
   }
}


void 
TuIM::processRegisterResponse(SipMessage* msg)
{
   int number = msg->header(h_StatusLine).responseCode();
   Uri to = msg->header(h_To).uri();
   ErrLog ( << "register of " << to << " got response " << number );   

   int expires = 3600;
   if ( msg->exists(h_Expires) )
   {
      expires = msg->header(h_Expires).value();
   }
   if ( expires < 5 )
   {
      ErrLog( << "Got very small expiers of " << expires );
      expires = 5;
   }
    
   mRegistrationDialog.createDialogAsUAC( *msg );
   mNextTimeToRegister = Timer::getRandomFutureTimeMs( expires*1000 );
}


void 
TuIM::processSubscribeResponse(SipMessage* msg, Buddy& buddy)
{
   int number = msg->header(h_StatusLine).responseCode();
   Uri to = msg->header(h_To).uri();
   ErrLog ( << "subscribe got response " << number << " from " << to );   

   if ( (number>=200) && (number<300) )
   {
      int expires = 3600;
      if ( msg->exists(h_Expires) )
      {
         expires = msg->header(h_Expires).value();
      }
      if ( expires < 5 )
      {
         ErrLog( << "Got very small expiers of " << expires );
         expires = 5;
      } 
      
      assert( buddy.presDialog );
      buddy.presDialog->createDialogAsUAC( *msg );
      
      buddy.mNextTimeToSubscribe = Timer::getRandomFutureTimeMs( expires*1000 );
   }

   if (  (number>=300) && (number<400) )
   {
      ErrLog( << "Still need to implement 3xx handing" ); // !cj!
   }
   
   if (  (number>=400) )
   {
      InfoLog( << "Got an error to so9me subscription" );     

      // take this buddy off line 
      Uri to = msg->header(h_To).uri();
      assert( mPressCallback );
      mPressCallback->presenseUpdate( to, false, Data::Empty );
      
      // try to contact this buddy agian in 10 minutes 
      buddy.mNextTimeToSubscribe = Timer::getRandomFutureTimeMs( 10*60*1000 );
   }
}


void 
TuIM::process()
{
   assert( mStack );

   UInt64 now = Timer::getTimeMs();
   
   // check if register needs refresh
   if ( now > mNextTimeToRegister )
   {
      if ( mRegistrationDialog.isCreated() )
      {
         auto_ptr<SipMessage> msg( mRegistrationDialog.makeRegister() );
         int expires = 11*60; // time in seconds 
         msg->header(h_Expires).value() = expires;
         mStack->send( *msg );
      }
      mNextTimeToRegister = Timer::getRandomFutureTimeMs( 10*60*1000 /*10 minutes*/ );
   }
   
   // check if any subscribes need refresh
   for ( int i=0; i<getNumBuddies(); i++)
   {
      if (  now > mBuddy[i].mNextTimeToSubscribe )
      {
         Buddy& b = mBuddy[i];
         mBuddy[i].mNextTimeToSubscribe = Timer::getRandomFutureTimeMs( 5*60*1000 /*5 minutes*/ );
         
         assert(  b.presDialog );
         if ( b.presDialog->isCreated() )
         {
            auto_ptr<SipMessage> msg( b.presDialog->makeSubscribe() );
            int expires = 7*60; // time in seconds 
            
            msg->header(h_Event).value() = Data("presence");;
            msg->header(h_Accepts).push_back( Mime( "application","cpim-pidf+xml") );
            msg->header(h_Expires).value() = expires;

            mStack->send( *msg );
         }
      }
   }

   // check for any messages from the sip stack 
   SipMessage* msg( mStack->receive() );
   if ( msg )
   {
      DebugLog ( << "got message: " << *msg);
   
      if ( msg->isResponse() )
      { 
         processResponse( msg );
      }
      
      if ( msg->isRequest() )
      {
         processRequest( msg );
      }

      delete msg; msg=0;
   }
}


void 
TuIM::registerAor( const Uri& uri, const Data& password )
{  
   mRegistrationPassword = password;
   
   //const NameAddr aorName;
   //const NameAddr contactName;
   //aorName.uri() = uri;
   //contactName.uri() = mContact;
   //SipMessage* msg = Helper::makeRegister(aorName,aorName,contactName);

   auto_ptr<SipMessage> msg( mRegistrationDialog.makeInitialRegister(NameAddr(uri),NameAddr(uri)) );

   int expires = 11*60; // time in seconds 
   msg->header(h_Expires).value() = expires;
   
   mNextTimeToRegister = Timer::getRandomFutureTimeMs( expires*1000 );
   
   mStack->send( *msg );
}


int 
TuIM::getNumBuddies() const
{
   return int(mBuddy.size());
}

const Uri 
TuIM::getBuddyUri(const int index)
{
   assert( index >= 0 );
   assert( index < getNumBuddies() );

   return mBuddy[index].uri;
}

const Data 
TuIM::getBuddyGroup(const int index)
{
   assert( index >= 0 );
   assert( index < getNumBuddies() );

   return mBuddy[index].group;
}

void 
TuIM::addBuddy( const Uri& uri, const Data& group )
{
   Buddy b;
   b.uri = uri;
   b.group = group;
   b.presDialog = new Dialog( NameAddr(mContact) );
   assert( b.presDialog );
   
   mBuddy.push_back( b );

   // subscribe to this budy 
   auto_ptr<SipMessage> msg( b.presDialog->makeInitialSubscribe(NameAddr(b.uri),NameAddr(b.uri)) );

   int expires = 7*60; // time in seconds 
   
   msg->header(h_Event).value() = Data("presence");;
   msg->header(h_Accepts).push_back( Mime( "application","cpim-pidf+xml") );
   msg->header(h_Expires).value() = expires;
   
   b.mNextTimeToSubscribe = Timer::getRandomFutureTimeMs( expires*1000 /*5 minutes*/ );
   
   mStack->send( *msg );
}


void 
TuIM::removeBuddy( const Uri& name)
{
   assert(0);
}


void 
TuIM::sendNotify(Dialog* dialog)
{ 
   assert( dialog );
   
   auto_ptr<SipMessage> notify( dialog->makeNotify() );

   Pidf* pidf = new Pidf( *mPidf );

   notify->header(h_Event).value() = "presence";

   Token state;
   state.value() = Data("active");
   state.param(p_expires) = 666; // !cj! - weird this is not showing up in the message
   notify->header(h_SubscriptionStates).push_front(state);

   notify->setContents( pidf );
   
   mStack->send( *notify );
}


void 
TuIM::setMyPresense( const bool open, const Data& status )
{
   assert( mPidf );
   mPidf->setSimpleStatus( open, status, mContact.getAor() );
   
   for ( unsigned int i=0; i< mSubscribers.size(); i++)
   {
      Dialog* dialog = mSubscribers[i];
      assert( dialog );
      
      sendNotify(dialog);
   }
}



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
