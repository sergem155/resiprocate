#if !defined(RemoteParticipantDialogSet_hxx)
#define RemoteParticipantDialogSet_hxx

#include <map>
#include <list>
#include <resip/stack/SdpContents.hxx>
#include <resip/dum/AppDialogSet.hxx>
#include <resip/dum/AppDialog.hxx>
#include <resip/dum/InviteSessionHandler.hxx>
#include <resip/dum/DialogSetHandler.hxx>
#include <resip/dum/SubscriptionHandler.hxx>

#include "ConversationManager.hxx"
#include "ConversationProfile.hxx"
#include "Participant.hxx"

namespace sdpcontainer
{
class Sdp; 
}

namespace resip
{
class DialogUsageManager;
class SipMessage;
}

namespace recon
{
class ConversationManager;
class RemoteParticipant;

/**
  This class is used by Invite DialogSets.  Non-Invite DialogSets
  are managed by DefaultDialogSet.  This class contains logic
  to handle forking and RTP connections.

  Author: Scott Godin (sgodin AT SipSpectrum DOT com)
*/

class RemoteParticipantDialogSet : public resip::AppDialogSet
{
public:
   RemoteParticipantDialogSet(ConversationManager& conversationManager,        
                              ConversationManager::ParticipantForkSelectMode forkSelectMode = ConversationManager::ForkSelectAutomatic,
                              std::shared_ptr<ConversationProfile> conversationProfile = nullptr);

   virtual ~RemoteParticipantDialogSet();

   virtual RemoteParticipant* createUACOriginalRemoteParticipant(ParticipantHandle handle);
   virtual resip::AppDialog* createAppDialog(const resip::SipMessage& msg);

   virtual void setProposedSdp(ParticipantHandle handle, const resip::SdpContents& sdp);
   virtual resip::SdpContents* getProposedSdp() { return mProposedSdp; }
   virtual void setUACConnected(const resip::DialogId& dialogId, ParticipantHandle partHandle);
   virtual bool isUACConnected();
   virtual bool isStaleFork(const resip::DialogId& dialogId);

   virtual void removeDialog(const resip::DialogId& dialogId);
   virtual ConversationManager::ParticipantForkSelectMode getForkSelectMode();
   virtual ParticipantHandle getActiveRemoteParticipantHandle() { return mActiveRemoteParticipantHandle; }
   virtual void setActiveRemoteParticipantHandle(ParticipantHandle handle) { mActiveRemoteParticipantHandle = handle; }

   // DialogSetHandler
   virtual void onTrying(resip::AppDialogSetHandle, const resip::SipMessage& msg);
   virtual void onNonDialogCreatingProvisional(resip::AppDialogSetHandle, const resip::SipMessage& msg);

   // Media Stream Processing
   virtual void processMediaStreamReadyEvent(const StunTuple& remoteRtpTuple, const StunTuple& remoteRtcpTuple);
   virtual void processMediaStreamErrorEvent(unsigned int errorCode);

   void sendInvite(std::shared_ptr<resip::SipMessage> invite);
   void provideOffer(std::unique_ptr<resip::SdpContents> offer, resip::InviteSessionHandle& inviteSessionHandle, bool postOfferAccept);
   void provideAnswer(std::unique_ptr<resip::SdpContents> answer, resip::InviteSessionHandle& inviteSessionHandle, bool postAnswerAccept, bool postAnswerAlert);
   void accept(resip::InviteSessionHandle& inviteSessionHandle);

protected:
   virtual std::shared_ptr<resip::UserProfile> selectUASUserProfile(const resip::SipMessage&);

   virtual bool isAsyncMediaSetup() = 0;

   virtual void fixUpSdp(resip::SdpContents* sdp) = 0;

private:
   ConversationManager& mConversationManager;   
   RemoteParticipant* mUACOriginalRemoteParticipant;
   std::list<ConversationHandle> mUACOriginalConversationHandles;
   unsigned int mNumDialogs;
   ConversationManager::ParticipantForkSelectMode mForkSelectMode;
   std::shared_ptr<ConversationProfile> mConversationProfile;
   resip::DialogId mUACConnectedDialogId;
   ParticipantHandle mActiveRemoteParticipantHandle;
   std::map<resip::DialogId, RemoteParticipant*> mDialogs;

   // SDP Negotiations that may need to be delayed due to media stack binding/allocation
   std::shared_ptr<resip::SipMessage> mPendingInvite;
   void doSendInvite(std::shared_ptr<resip::SipMessage> invite);
   class PendingOfferAnswer
   {
   public:
      PendingOfferAnswer() {}
      bool mOffer;
      std::unique_ptr<resip::SdpContents> mSdp;
      resip::InviteSessionHandle mInviteSessionHandle;
      bool mPostOfferAnswerAccept;
      bool mPostAnswerAlert;
   };
   PendingOfferAnswer mPendingOfferAnswer;
   void doProvideOfferAnswer(bool offer, std::unique_ptr<resip::SdpContents> sdp, resip::InviteSessionHandle& inviteSessionHandle, bool postOfferAnswerAccept, bool postAnswerAlert);
   resip::SdpContents* mProposedSdp;
};

}

#endif


/* ====================================================================

 Copyright (c) 2007-2008, Plantronics, Inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are 
 met:

 1. Redistributions of source code must retain the above copyright 
    notice, this list of conditions and the following disclaimer. 

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution. 

 3. Neither the name of Plantronics nor the names of its contributors 
    may be used to endorse or promote products derived from this 
    software without specific prior written permission. 

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ==================================================================== */
