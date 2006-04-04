/*
 Copyright (C) 2004-2006 the Minisip Team
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/* Copyright (C) 2004 
 *
 * Authors: Erik Eliasson <eliasson@it.kth.se>
 *          Johan Bilien <jobi@via.ecp.fr>
 *	    Joachim Orrblad <joachim[at]orrblad.com>
*/

/* Name
 * 	SipDialogVoip.cxx
 * Author
 * 	Erik Eliasson, eliasson@it.kth.se
 * Purpose
 * 
*/
#include"SipDialogVoip.h"

#include<libmutil/massert.h>

#include<libmsip/SipDialogContainer.h>
#include<libmsip/SipMessageTransport.h>
#include<libmsip/SipTransactionInviteClientUA.h>
#include<libmsip/SipTransactionInviteServerUA.h>
#include<libmsip/SipTransactionNonInviteClient.h>
#include<libmsip/SipTransactionNonInviteServer.h>
#include<libmsip/SipTransactionUtils.h>
#include<libmsip/SipCommandString.h>
#include<libmsip/SipHeaderWarning.h>
#include<libmsip/SipHeaderContact.h>
#include<libmsip/SipHeaderFrom.h>
#include<libmsip/SipHeaderRoute.h>
#include<libmsip/SipHeaderRequire.h>
#include<libmsip/SipHeaderTo.h>
#include<libmsip/SipMIMEContent.h>
#include<libmsip/SipMessageContent.h>
#include<libmutil/itoa.h>
#include<libmutil/base64.h>
#include<libmutil/Timestamp.h>
#include<libmutil/termmanip.h>
#include<libmutil/dbg.h>
#include<libmsip/SipSMCommand.h>
#include <time.h>
#include"../minisip/LogEntry.h"

#include<libmutil/print_hex.h>
#include <iostream>
#include<time.h>

#ifdef _WIN32_WCE
#	include"../include/minisip_wce_extra_includes.h"
#endif

using namespace std;


/*
 
 
  
States are named as follows where X and NN is digit numbers

  a 1 X NN _ "fromstate" _ "tostate" _ "trigger"
    ^ ^ ^^
    | | ++-- Number starting from 1
    | +----- Type of functionality 0: in-call commands such as BYE, bye and re-invite
    |                              1: dialog management 
    |                              2: locally initiated call transfer 
    |                              3: remotely initiated call transfer
    +------- Actions in this file. SipDialogVoipClient=2, SipDialogVoipServer=3
    
Example: a1301_incall_transfaskuser_REFER means that it is the transition
         from the incall state to the transfaskuser state that will happen
	 if it receives a REFER. a1301 means "action 1 in SipDialogVoip that
	 handles remotely initiated call transfer".

	 
State Machine:
 The state machine in this file only deals with a call after it has been set up and
 is in the "in_call" state. Sub-classes implement call setup so that we get into
 this state.
 


  These states are              These states are              These states
  when the local user           for a "normal" call.          are used when a
  requests a call               in_call is during             remote user request
  transfer.                     the call.                     call transfer
 <---------------->            <--------------------->        <---------------->
                   transfer
                   a1201:new TReferInit           REFER
 +---------------+              +---------------+ a1301:      +---------------+
 |               |<-------------|               |---------->|               |
 | transf_request|              |   in call     |refus/a1303|transf_ask_user|
 |               |------------->|               |<----------|               |
 +---------------+ 300-699      +---------------+-------+   +---------------+
         |         a1203: -             |               | hangup          | accept_transfer
         | 202                          | BYE           | a1002:TByeCli   | a1302: 202;start new call
         | a1202: new subscription      V a1001:TByeServ|                 V
 +---------------+              +---------------+       |   +---------------+
 |               |------------->|               |<------+   |               |
 | transf_pending|  bye/BYE     |   termwait    |           |transf_started |
 |               |  a1001/  +---|               |<----------|               |
 +---------------+  a1002   |   +---------------+  bye/BYE  +---------------+
   |          ^             |      ^    |          a1304/a1305             
   |          |   hang_up   +------+    |  a1101: no_transactions             
   +----------+   a1102: call_term_earlyV  gui(call_terminated)           
     NOTIFY                     +---------------+
     a1204: -                   |               |
                                |  terminated   |
                                |               |
                                +---------------+
  
 Why do we inform the GUI that the dialog has ben terminated (in a1101)? --Erik
 
*/


/**
 * Helper function that returns the uri in the referto header.
 */
static string getReferredUri(MRef<SipRequest*> req){
	string referredUri;
	MRef<SipHeaderValue*> hval;
	if (req)
		hval = req->getHeaderValueNo(SIP_HEADER_TYPE_REFERTO, 0);

	if (hval)
		referredUri = hval->getString();
	else{
		cerr << "WARNING: Referred to uri not found!"<<endl;
	}
	return referredUri;
}

/**
 *
 * This transaction is also used a a1001_transfpending_termwait_BYE
 */
bool SipDialogVoip::a1001_incall_termwait_BYE( const SipSMCommand &command)
{
	
	if (transitionMatch("BYE", command, SipSMCommand::remote, IGN) &&
	    dialogState.remoteTag != ""){
		MRef<SipRequest*> bye = (SipRequest*) *command.getCommandPacket();

		//mdbg << "log stuff"<< end;
		if( getLogEntry() ){
			((LogEntrySuccess *)(*( getLogEntry() )))->duration = 
			time( NULL ) - getLogEntry()->start; 

			getLogEntry()->handle();
		}

		MRef<SipTransaction*> byeresp = new SipTransactionNonInviteServer(sipStack, 
				MRef<SipDialog*>(this), 
				bye->getCSeq(),
				bye->getCSeqMethod(),
				bye->getLastViaBranch(), 
				dialogState.callId); //TODO: remove second argument

		registerTransaction(byeresp);
		SipSMCommand cmd(command);
		cmd.setDestination(SipSMCommand::transaction);
		cmd.setSource(command.getSource());
		getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);

		sendByeOk(bye, byeresp->getBranch() );

		CommandString cmdstr(dialogState.callId, SipCommandString::remote_hang_up);
		getDialogContainer()->getCallback()->handleCommand("gui", cmdstr);

		getMediaSession()->stop();

		
		signalIfNoTransactions();
		return true;
	}else{
		return false;
	}
}

/**
 *
 * This transaction is also used a a1001_transfpending_termwait_hangup
 */
bool SipDialogVoip::a1002_incall_termwait_hangup( const SipSMCommand &command)
{
	if (transitionMatch(command, SipCommandString::hang_up)){
		//int bye_seq_no= requestSeqNo();
		++dialogState.seqNo;
		MRef<SipTransaction*> byetrans( new SipTransactionNonInviteClient(sipStack, 
				MRef<SipDialog*>(this), 
				dialogState.seqNo, 
				"BYE",
				dialogState.callId)); 

		registerTransaction(byetrans);
		sendBye(byetrans->getBranch(), dialogState.seqNo);
		
		if (getLogEntry()){
			(dynamic_cast< LogEntrySuccess * >(*( getLogEntry() )))->duration = time( NULL ) - getLogEntry()->start; 
			getLogEntry()->handle();
		}
		
		getMediaSession()->stop();

		signalIfNoTransactions();
		return true;
	}else{
		return false;
	}
}

bool SipDialogVoip::a1101_termwait_terminated_notransactions( const SipSMCommand &command){
	if (transitionMatch(command, SipCommandString::no_transactions) ){
		lastInvite=NULL;
                
		SipSMCommand cmd(
				CommandString( dialogState.callId, 
					SipCommandString::call_terminated),
					SipSMCommand::TU,
					SipSMCommand::DIALOGCONTAINER);

		getDialogContainer()->enqueueCommand( cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE );
		/* Tell the GUI */
		CommandString cmdstr( dialogState.callId, SipCommandString::call_terminated);
		getDialogContainer()->getCallback()->handleCommand("gui", cmdstr );

#ifdef IPSEC_SUPPORT
		if(ipsecSession){
			cerr << "Clearing" << endl;
			if(ipsecSession->stop() != 0)
				cerr << "Not all IPSEC parameters were confired cleared. Check and remove manually." << endl;
		}
#endif
		return true;
	}else{
		return false;
	}
}

//notify the gui that dialog is finished ... just some transactions need to 
//finish absorbing messages
//also, reply to the dialog container when extra hang_up messages are received
//  (this happens when you hang up a call from the gui and then exit minisip,
//  the sipdialogmanagement would send a hang up and receive no call_term_early ...
//  with this, it works).
bool SipDialogVoip::a1102_termwait_termwait_early( const SipSMCommand &command){
	if( transitionMatch(command, SipCommandString::hang_up) ) {
		CommandString cmdstr( dialogState.callId, SipCommandString::call_terminated_early);
		/* Tell the GUI, once only */
		if( notifyEarlyTermination ) {
			getDialogContainer()->getCallback()->handleCommand("gui", cmdstr );
			notifyEarlyTermination = false;
		}
		SipSMCommand cmd( cmdstr, SipSMCommand::TU, SipSMCommand::DIALOGCONTAINER );
		getDialogContainer()->enqueueCommand( cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE ); //this is for the shutdown dialog 
		return true;
	}
	else if ( notifyEarlyTermination && transitionMatch(SipResponse::type, command, IGN, SipSMCommand::TU, "2**")){
		lastInvite=NULL;                
		//Notify the GUI and the dialog container ... 
		CommandString cmdstr( dialogState.callId, SipCommandString::call_terminated_early);
		
		/* Tell the GUI, once only */
		getDialogContainer()->getCallback()->handleCommand("gui", cmdstr );
		notifyEarlyTermination = false;
		
		//Tell the dialog container ... 
		SipSMCommand cmd( cmdstr, SipSMCommand::TU, SipSMCommand::DIALOGCONTAINER );
		getDialogContainer()->enqueueCommand( cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE ); //this is for the shutdown dialog 
		return true;
	}else{
		return false;
	}
}

bool SipDialogVoip::a1201_incall_transferrequested_transfer( const SipSMCommand &command)
{
	if (transitionMatch(command, SipCommandString::user_transfer)){
		//int bye_seq_no= requestSeqNo();
		string referredUri = command.getCommandString().getParam();
		++dialogState.seqNo;
		MRef<SipTransaction*> refertrans( 
			new SipTransactionNonInviteClient(sipStack, 
				MRef<SipDialog*>(this), 
				dialogState.seqNo, 
				"REFER", 
				dialogState.callId)); 

		registerTransaction(refertrans);
		sendRefer(refertrans->getBranch(), dialogState.seqNo, referredUri);

		return true;
	}else{
		return false;
	}
}

bool SipDialogVoip::a1202_transferrequested_transferpending_202( const SipSMCommand &command){
	if (transitionMatch(SipResponse::type, command, IGN, SipSMCommand::TU, "202")){
		MRef<SipResponse*> resp( (SipResponse*)*command.getCommandPacket() );

		CommandString cmdstr(dialogState.callId, 
				SipCommandString::transfer_pending
				);
		getDialogContainer()->getCallback()->handleCommand("gui", cmdstr );
		
		/* Minisip does not actually keep track of the transfer ... 
		if the REFER is accepted by the far end, then shutdown the media
		*/
		getMediaSession()->stop();
		signalIfNoTransactions();

		return true;
	}else{
		return false;
	}
}

bool SipDialogVoip::a1203_transferrequested_incall_36( const SipSMCommand &command)
{
	if (transitionMatch(SipResponse::type, command, IGN, SipSMCommand::TU, "3**\n4**\n5**\n6**")){
                
		CommandString cmdstr( dialogState.callId, 
			SipCommandString::transfer_refused, 
			command.getCommandPacket()->getWarningMessage());
		getDialogContainer()->getCallback()->handleCommand("gui", cmdstr );
		return true;
	}else{
		return false;
	}
}

bool SipDialogVoip::a1204_transferpending_transferpending_notify( const SipSMCommand &command) {
	bool ret = false;
	
	if (transitionMatch("NOTIFY", command, SipSMCommand::transaction, SipSMCommand::TU)){
		//this is just the same notify, lifted from the transaction up to the dialog ... 
		//we can safely absorb it ...
		return true;
	} else if (transitionMatch("NOTIFY", command, SipSMCommand::remote, SipSMCommand::TU)){
		MRef<SipRequest*> notif;
		notif = (SipRequest*)*command.getCommandPacket();
		
		MRef<SipTransaction*> notifyResp = new SipTransactionNonInviteServer(
				sipStack, 
				MRef<SipDialog*>(this), 
				notif->getCSeq(),
				notif->getCSeqMethod(),
				notif->getLastViaBranch(), 
				dialogState.callId); //TODO: remove second argument
		registerTransaction(notifyResp);
		SipSMCommand cmd(command);
		cmd.setDestination(SipSMCommand::transaction);
		cmd.setSource(command.getSource());
		getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
		
		sendNotifyOk(notif, notifyResp->getBranch() );
		
		ret = true;
	}
	return ret;
}

bool SipDialogVoip::a1301_incall_transferaskuser_REFER( const SipSMCommand &command)
{

	if (transitionMatch("REFER", command, IGN, IGN)){

		MRef<SipTransaction*> cr( 
			new SipTransactionNonInviteServer(sipStack, 
				MRef<SipDialog*>(this), 
				command.getCommandPacket()->getCSeq(), 
				command.getCommandPacket()->getCSeqMethod(),
				command.getCommandPacket()->getLastViaBranch(), 
				dialogState.callId) );
		registerTransaction(cr);

		SipSMCommand cmd(command);
		cmd.setDestination(SipSMCommand::transaction);

		getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
		
		if (command.getCommandPacket()->getType()=="REFER"){
			lastRefer = dynamic_cast<SipRequest*>(*command.getCommandPacket());
		}else{
			lastRefer = NULL;
		}
			
		
		
		string referredUri=getReferredUri(lastRefer);
		
		/* Tell the GUI */
		CommandString cmdstr(dialogState.callId, 
				SipCommandString::transfer_requested,
				referredUri);
		getDialogContainer()->getCallback()->handleCommand("gui", cmdstr );
		return true;
	}else{
		return false;
	}
}



bool SipDialogVoip::a1302_transferaskuser_transferstarted_accept( const SipSMCommand &command)
{
	if (transitionMatch(command, SipCommandString::user_transfer_accept)){

		sendReferOk(lastRefer->getDestinationBranch() );

		/* start a new call ... */
		string uri = getReferredUri(lastRefer);

		//string newCallId = phoneconf->sip->invite( uri );
		CommandString invite("",SipCommandString::invite, uri);
		CommandString resp = phoneconf->sip->handleCommandResp("sip",invite);
		string newCallId=resp.getDestinationId();

		/* Send the new callId to the GUI */
		CommandString cmdstr(dialogState.callId, SipCommandString::call_transferred, newCallId);
		getDialogContainer()->getCallback()->handleCommand("gui", cmdstr );

		return true;
	}else{
		return false;
	}
}

bool SipDialogVoip::a1303_transferaskuser_incall_refuse( const SipSMCommand &command)
{
	if (transitionMatch(command, SipCommandString::user_transfer_refuse)){

		sendReferReject(lastRefer->getDestinationBranch());

		return true;
	}else{
		return false;
	}
}


void SipDialogVoip::setUpStateMachine(){

	State<SipSMCommand,string> *s_incall=new State<SipSMCommand,string>(this,"incall");
	addState(s_incall);

	State<SipSMCommand,string> *s_termwait=new State<SipSMCommand,string>(this,"termwait");
	addState(s_termwait);
	
	State<SipSMCommand,string> *s_terminated=new State<SipSMCommand,string>(this,"terminated");
	addState(s_terminated);
	
	
	// call transfer states
	State<SipSMCommand,string> *s_transferrequested=new State<SipSMCommand,string>(this,"transferrequested");
	addState(s_transferrequested);
        
	State<SipSMCommand,string> *s_transferpending=new State<SipSMCommand,string>(this,"transferpending");
	addState(s_transferpending);
	
	State<SipSMCommand,string> *s_transferaskuser=new State<SipSMCommand,string>(this,"transferaskuser");
	addState(s_transferaskuser);
	
	State<SipSMCommand,string> *s_transferstarted=new State<SipSMCommand,string>(this,"transferstarted");
	addState(s_transferstarted);

	
	// Ending a call
	new StateTransition<SipSMCommand,string>(this, "transition_incall_termwait_BYE",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand&)) &SipDialogVoip::a1001_incall_termwait_BYE,
			s_incall, s_termwait); 

	new StateTransition<SipSMCommand,string>(this, "transition_incall_termwait_hangup",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand& )) &SipDialogVoip::a1002_incall_termwait_hangup,
			s_incall, s_termwait);

	// Transaction/dialog management
	new StateTransition<SipSMCommand,string>(this, "transition_termwait_terminated_notransactions",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand&)) &SipDialogVoip::a1101_termwait_terminated_notransactions,
			s_termwait, s_terminated);

	new StateTransition<SipSMCommand,string>(this, "transition_termwait_termwait_earlynotify",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand&)) &SipDialogVoip::a1102_termwait_termwait_early,
			s_termwait, s_termwait);
	
	// Locally initiated call transfer
	new StateTransition<SipSMCommand,string>(this, "transition_incall_transferrequested_transfer",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand&)) &SipDialogVoip::a1201_incall_transferrequested_transfer,
			s_incall, s_transferrequested);
        
	new StateTransition<SipSMCommand,string>(this, "transition_transferrequested_transferpending_202",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand&)) &SipDialogVoip::a1202_transferrequested_transferpending_202,
			s_transferrequested, s_transferpending);
	
	new StateTransition<SipSMCommand,string>(this, "transition_transferrequested_incall_36",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand&)) &SipDialogVoip::a1203_transferrequested_incall_36,
			s_transferrequested, s_incall);
	
	new StateTransition<SipSMCommand,string>(this, "transition_transferpending_transferpending_notify",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand&)) &SipDialogVoip::a1204_transferpending_transferpending_notify,
			s_transferpending, s_transferpending);
	
	new StateTransition<SipSMCommand,string>(this, "transition_transferpending_termwait_bye",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand&)) &SipDialogVoip::a1001_incall_termwait_BYE,
			s_transferpending, s_termwait);

	new StateTransition<SipSMCommand,string>(this, "transition_transferpending_termwait_BYE",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand&)) &SipDialogVoip::a1002_incall_termwait_hangup,
			s_transferpending, s_termwait);
	
	
	// Remotely initiated call transfer
	new StateTransition<SipSMCommand,string>(this, "transition_incall_transferaskuser_REFER",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand&)) &SipDialogVoip::a1301_incall_transferaskuser_REFER,
			s_incall, s_transferaskuser);
	
	new StateTransition<SipSMCommand,string>(this, "transition_transferaskuser_transferstarted_accept",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand&)) &SipDialogVoip::a1302_transferaskuser_transferstarted_accept,
			s_transferaskuser, s_transferstarted);
	
	new StateTransition<SipSMCommand,string>(this, "transition_transferaskuser_incall_refuse",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand&)) &SipDialogVoip::a1303_transferaskuser_incall_refuse,
			s_transferaskuser, s_incall);
	
	new StateTransition<SipSMCommand,string>(this, "transition_transferstarted_termwait_bye",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand&)) &SipDialogVoip::a1001_incall_termwait_BYE,
			s_transferstarted, s_termwait);

	new StateTransition<SipSMCommand,string>(this, "transition_transferstarted_termwait_BYE",
			(bool (StateMachine<SipSMCommand,string>::*)(const SipSMCommand&)) &SipDialogVoip::a1002_incall_termwait_hangup,
			s_transferstarted, s_termwait);
}


SipDialogVoip::SipDialogVoip(	MRef<SipStack*> stack, 
				MRef<SipDialogConfig*> callconfig, 
				MRef<SipSoftPhoneConfiguration*> pconf, 
				MRef<Session *> mediaSession, 
				string cid
#ifdef IPSEC_SUPPORT
				, MRef<MsipIpsecAPI *> ipsecSession
#endif
				) :
		SipDialog(stack,callconfig),
		phoneconf(pconf),
		mediaSession(mediaSession),
		lastInvite(NULL)
#ifdef IPSEC_SUPPORT
		, ipsecSession(ipsecSession)
#endif
{
	if (cid=="")
		dialogState.callId = itoa(rand())+"@"+getDialogConfig()->inherited->externalContactIP;
	else
		dialogState.callId = cid;
	
	dialogState.localTag = itoa(rand());
	
	/* We will fill that later, once we know if that succeeded */
	logEntry = NULL;

	setUpStateMachine();
}

SipDialogVoip::~SipDialogVoip(){	
	mediaSession->unregister();
}

void SipDialogVoip::sendBye(const string &branch, int bye_seq_no){

	MRef<SipRequest*> bye = SipRequest::createSipMessageBye(
			branch,
			dialogState.callId,
			dialogState.getRemoteTarget(),
			dialogState.remoteUri,
			getDialogConfig()->inherited->sipIdentity->getSipUri(),
			bye_seq_no );

	//add route headers, if needed
	if( dialogState.routeSet.size() > 0 ) {
		//merr << "SipDlgVoip:sendBYE : adding header route! " << end;
		MRef<SipHeaderValueRoute *> rset = new SipHeaderValueRoute (dialogState.routeSet);
		bye->addHeader(new SipHeader(*rset) );
	} else {
		//merr << "SipDlgVoip:sendBYE : dialog route set is EMPTY!!! " << end;
	}

	bye->getHeaderValueFrom()->setParameter("tag",dialogState.localTag);
	bye->getHeaderValueTo()->setParameter("tag",dialogState.remoteTag);

	MRef<SipMessage*> pref(*bye);
	SipSMCommand cmd( pref, SipSMCommand::TU, SipSMCommand::transaction);
	getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
}

void SipDialogVoip::sendRefer(const string &branch, int refer_seq_no, const string referredUri){
/*
	string tmp = getDialogConfig()->inherited.sipIdentity->getSipUri();
	uint32_t i = tmp.find("@");
	massert(i!=string::npos);
	i++;
	string domain;
	for ( ; i < tmp.length() ; i++)
		domain = domain+tmp[i];
*/
	//MRef<SipRefer*> refer = new SipRefer(
	MRef<SipRequest*> refer = SipRequest::createSipMessageRefer(
			branch,
			getLastInvite(),
			dialogState.getRemoteTarget(),
			getDialogConfig()->inherited->sipIdentity->getSipUri(),
			referredUri,
			refer_seq_no
			);

	refer->getHeaderValueFrom()->setParameter("tag",dialogState.localTag);
	refer->getHeaderValueTo()->setParameter("tag",dialogState.remoteTag);

	MRef<SipMessage*> pref(*refer);
	SipSMCommand cmd( pref, SipSMCommand::TU, SipSMCommand::transaction);
	getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
}

void SipDialogVoip::sendCancel(const string &branch){
	massert( !lastInvite.isNull());
	
	//merr << "SipDialogVoip::sendCancel : dialogstate.remoteUri=" << dialogState.remoteUri << end;
	
	//MRef<SipCancel*> cancel = new SipCancel(
	MRef<SipRequest*> cancel = SipRequest::createSipMessageCancel(
			branch,
			lastInvite,
			dialogState.remoteUri
			);

	cancel->getHeaderValueFrom()->setParameter("tag",dialogState.localTag);
	// Don't include to-tag, allowing it to be forked by proxies.

	MRef<SipMessage*> pref(*cancel);
	SipSMCommand cmd( pref, SipSMCommand::TU, SipSMCommand::transaction);
	getDialogContainer()->enqueueCommand( cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE );
}

void SipDialogVoip::sendReferOk(const string &branch){
	MRef<SipResponse*> ok= new SipResponse(branch, 202,"OK", MRef<SipMessage*>(*lastRefer));	
	ok->getHeaderValueTo()->setParameter("tag",dialogState.localTag);
	MRef<SipHeaderValue *> contact = 
		new SipHeaderValueContact( 
			getDialogConfig()->inherited->sipIdentity->getSipUri(),
			getDialogConfig()->inherited->externalContactIP,
			getDialogConfig()->inherited->getLocalSipPort(phoneconf->useSTUN),
			"", getDialogConfig()->inherited->getTransport(),
			-1); //set expires to -1, we do not use it (only in register)
	ok->addHeader( new SipHeader(*contact) );

	MRef<SipMessage*> pref(*ok);
	SipSMCommand cmd( pref, SipSMCommand::TU, SipSMCommand::transaction);
	getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
}

void SipDialogVoip::sendByeOk(MRef<SipRequest*> bye, const string &branch){
	MRef<SipResponse*> ok= new SipResponse( branch, 200,"OK", MRef<SipMessage*>(*bye) );
	ok->getHeaderValueTo()->setParameter("tag",dialogState.localTag);

//	setLastResponse(ok);
	MRef<SipMessage*> pref(*ok);
	SipSMCommand cmd( pref, SipSMCommand::TU, SipSMCommand::transaction);
//	handleCommand(cmd);
	getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
}

void SipDialogVoip::sendNotifyOk(MRef<SipRequest*> notif, const string &branch){
	MRef<SipResponse*> ok= new SipResponse( branch, 200, "OK", MRef<SipMessage*>(*notif) );
	ok->getHeaderValueTo()->setParameter("tag",dialogState.localTag);

//	setLastResponse(ok);
	MRef<SipMessage*> pref(*ok);
	SipSMCommand cmd( pref, SipSMCommand::TU, SipSMCommand::transaction);
//	handleCommand(cmd);
	getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
}

void SipDialogVoip::sendReferReject(const string &branch){
	MRef<SipResponse*> forbidden = 
		new SipResponse(branch,
				403,"Forbidden", 
				*lastRefer
				);	
	forbidden->getHeaderValueTo()->setParameter("tag",dialogState.localTag);
	MRef<SipMessage*> pref(*forbidden);
	SipSMCommand cmd( pref,SipSMCommand::TU, SipSMCommand::transaction);
	getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
}

bool SipDialogVoip::handleCommand(const SipSMCommand &c){
	mdbg << "SipDialogVoip::handleCommand got "<< c << end;

	if (c.getType()==SipSMCommand::COMMAND_STRING && dialogState.callId.length()>0){
		if (c.getCommandString().getDestinationId() != dialogState.callId )
			return false;
	}
	
	if (c.getType()==SipSMCommand::COMMAND_PACKET  && dialogState.callId.length()>0){
		if (c.getCommandPacket()->getCallId() != dialogState.callId ){
			return false;
		}
		if (c.getType()!=SipSMCommand::COMMAND_PACKET && 
				c.getCommandPacket()->getCSeq()!= dialogState.seqNo){
			return false;
		}
	
	}
	
	mdbg << "SipDialogVoip::handlePacket() got "<< c << end;
	bool handled = SipDialog::handleCommand(c);
	
	if (!handled && c.getType()==SipSMCommand::COMMAND_STRING && c.getCommandString().getOp()==SipCommandString::no_transactions){
		return true;
	}
	
	if (c.getType()==SipSMCommand::COMMAND_STRING && dialogState.callId.length()>0){
		if (!handled && c.getCommandString().getDestinationId() == dialogState.callId ){
			mdbg << "Warning: SipDialogVoIP ignoring command with matching call id"<< end;
			return true;
		}
	}
	if (c.getType()==SipSMCommand::COMMAND_PACKET && dialogState.callId.length()>0){
		if (!handled && c.getCommandPacket()->getCallId() == dialogState.callId){
			mdbg << "Warning: SipDialogVoIP ignoring packet with matching call id"<< end;
			return true;
		}
	}
	
	return handled;
}

MRef<SipRequest*> SipDialogVoip::getLastInvite(){
	return lastInvite;
}

void SipDialogVoip::setLastInvite(MRef<SipRequest*> i){ 
	lastInvite = i; 
}

MRef<LogEntry *> SipDialogVoip::getLogEntry(){
	return logEntry;
}

void SipDialogVoip::setLogEntry( MRef<LogEntry *> logEntry ){
	this->logEntry = logEntry;
}

MRef<Session *> SipDialogVoip::getMediaSession(){
	return mediaSession;
}

#ifdef IPSEC_SUPPORT
MRef<MsipIpsecAPI *> SipDialogVoip::getIpsecSession(){
	return ipsecSession;
}
#endif

bool SipDialogVoip::sortMIME(MRef<SipMessageContent *> Offer, string peerUri, int type){
	if (Offer){
		if ( Offer->getContentType().substr(0,9) == "multipart"){
			MRef<SipMessageContent *> part;
			part = ((SipMimeContent*)*Offer)->popFirstPart();
			while( *part != NULL){
				sortMIME(part, peerUri, type);
				part = ((SipMimeContent*)*Offer)->popFirstPart();
			}
		}
#ifdef IPSEC_SUPPORT
		if( (Offer->getContentType()).substr(0,17) == "application/mikey"){
			switch (type){
				case 10:
					ts.save("setMikeyIpsecOffer");
					if(!getIpsecSession()->setMikeyIpsecOffer((SipMimeContent*)*Offer))
						return false;
					ts.save("setMikeyIpsecOffer");
					return true;
				case 3:
					ts.save("setMikeyIpsecAnswer");
					if(!getIpsecSession()->setMikeyIpsecAnswer((SipMimeContent*)*Offer))
						return false;
					ts.save("setMikeyIpsecAnswer");
					return true;
				default:
					return false;
			}
		}
#endif
		if( (Offer->getContentType()).substr(0,15) == "application/sdp"){
			switch (type){
				case 10:
#ifdef ENABLE_TS
					ts.save("setSdpOffer");
#endif
					if( !getMediaSession()->setSdpOffer( (SdpPacket*)*Offer, peerUri ) )
						return false;
#ifdef ENABLE_TS
					ts.save("setSdpOffer");
#endif
					return true;
				case 3:
#ifdef ENABLE_TS
					ts.save("setSdpAnswer");
#endif
					if( !getMediaSession()->setSdpAnswer( (SdpPacket*)*Offer, peerUri ) ){
						cerr << "SDP answer rejected" << endl;
						return false;
					}
					getMediaSession()->start();
#ifdef ENABLE_TS
					ts.save("setSdpAnswer");
#endif
					return true;
				default:
					merr << "No SDP match" << end;
					return false;
			}
		}
	}
	return true;
}
