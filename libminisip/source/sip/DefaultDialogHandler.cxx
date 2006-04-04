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
*/

#include"DefaultDialogHandler.h"
#include<libmnetutil/IP4Address.h>
#include<libmnetutil/NetworkException.h>
#include<libmsip/SipDialogRegister.h>
#include<libmsip/SipDialogContainer.h>
#include"SipDialogVoipServer.h"
#include"SipDialogConfVoip.h"
#include"SipDialogPresenceClient.h"
#include"SipDialogPresenceServer.h"
#include<libmsip/SipHeaderFrom.h>
#include<libmsip/SipHeaderTo.h>
#include<libmsip/SipHeaderAcceptContact.h>
#include<libmsip/SipMessage.h>
#include<libmsip/SipMessageContentIM.h>
#include<libmsip/SipMessageTransport.h>
#include<libmsip/SipCommandString.h>
#include<libmsip/SipTransactionInviteServer.h>
#include<libmsip/SipTransactionInviteServerUA.h>
#include<libmsip/SipTransactionNonInviteServer.h>
#include<libmsip/SipTransactionNonInviteClient.h>
#include<libmutil/massert.h>

#ifdef _WIN32_WCE
#	include"../include/minisip_wce_extra_includes.h"
#endif

#ifdef P2T_SUPPORT
#include"../p2t/SipDialogP2T.h"
#include"../p2t/SipDialogP2Tuser.h"
#endif

#include"../mediahandler/MediaHandler.h"

#ifdef IPSEC_SUPPORT
#include<../ipsec/MsipIpsecAPI.h>
#endif


#include<libmutil/dbg.h>


DefaultDialogHandler::DefaultDialogHandler(MRef<SipStack*> stack, 
            //SipDialogConfig &conf,
	    MRef<SipDialogConfig *> conf,
	    MRef<SipSoftPhoneConfiguration*> pconf,
	    MRef<MediaHandler *>mediaHandler): 
                SipDialog(stack, conf),
		phoneconf(pconf),
		mediaHandler(mediaHandler)
{
	dialogState.callId = string("DCH_")+itoa(rand())+"@"+getDialogConfig()->inherited->externalContactIP;
#ifdef P2T_SUPPORT
	//Initialize GroupListServer
	grpListServer=NULL;
#endif
}

DefaultDialogHandler::~DefaultDialogHandler(){
// 	cerr << "~DefaultDialogHandler" << endl;
}

string DefaultDialogHandler::getName(){
	return "DefaultDialogHandler";
}

bool DefaultDialogHandler::handleCommandPacket(int source, 
		int /*destination*/,	//Not used here - we know it's either ANY or 
					//TU (checked in handleCommand)
		MRef<SipMessage*> pkt, 
		int dispatchCount){

	/* First, check if this is a packet that could not be handled by
	 * any transaction and send 481 response if that is the case */
	if (source==SipSMCommand::remote && dispatchCount>=2){ // this is the packets second run of handling.
		mdbg << "DefaultCallHandler::handleCommand: Detected dispatched already - sending 481"<< end;

		//FIXME: Check what branch parameter to send.
		MRef<SipResponse*> no_call= new SipResponse("nobranch", 481,"Call Leg/Transaction Does Not Exist", MRef<SipMessage*>(*pkt));
		MRef<SipMessage*> pref(*no_call);

		sipStack->getSipTransportLayer()->sendMessage(pref,
				string(""), //branch
				false
				);

		return true;
	}

#ifdef DEBUG_OUTPUT
	if (source==SipSMCommand::remote && dispatchCount>=2){ // this is the packets second run of handling.
		cerr << "WARNING: INTERNAL ERROR: command was not handled (dispatched flag indication)"<<endl;
		return true;
	}
#endif

	if (source==SipSMCommand::remote && pkt->getType()=="INVITE"){

		//type casting
		MRef<SipRequest*> inv = MRef<SipRequest*>((SipRequest*)*pkt);
		//inv->checkAcceptContact();
		//check if it's a regular INVITE or a P2T INVITE
#ifdef P2T_SUPPORT
		if(inv->is_P2T()) {
			inviteP2Treceived(SipSMCommand(pkt,source,destination));	
		}
#endif


	bool isConfJoin=false;
	bool isP2T=false;
	bool isConfConnect=false;
	MRef<SipHeaderValueAcceptContact*> acp;
	int i=0;
	MRef<SipHeaderValue*> hdr=inv->getHeaderValueNo(SIP_HEADER_TYPE_ACCEPTCONTACT, i);
	do{
		if (hdr){
			MRef<SipHeaderValueAcceptContact*> acp = (SipHeaderValueAcceptContact*)*hdr;
                        if(acp && acp->getFeaturetag()=="+sip.p2t=\"TRUE\"")
                                isP2T=true;
                        else if(acp && acp->getFeaturetag()=="+sip.confjoin=\"TRUE\"") {
                                //cout << "SIPINVITE: Setting conjoin to true" << endl;
                                isConfJoin=true;
                        }
                        else if(acp && acp->getFeaturetag()=="+sip.confconnect=\"TRUE\""){
                                isConfConnect=true;
			}
		}
		i++;
		hdr = inv->getHeaderValueNo(SIP_HEADER_TYPE_ACCEPTCONTACT, i);
	}while(hdr);
			
        if(isConfJoin) {
			MRef<SipHeaderValueTo*> to = pkt->getHeaderValueTo();
			string uri;
			MRef<SipIdentity *> id = NULL;

			if( to ){
				id = phoneconf->getIdentity( to->getUri() );
			}
#ifdef DEBUG_OUTPUT			
			mdbg << "DefaultDialogHandler:: creating new SipDialogConfVoip" << end;
#endif			
			//MRef<SipMessage*> pack = command.getCommandPacket();
			//MRef<SipInvite*> inv = MRef<SipInvite*>((SipInvite*)*pack);
	
			//get the GroupList from the remote GroupListServer
			//MRef<GroupList*>grpList;
			massert(dynamic_cast<SdpPacket*>(*inv->getContent())!=NULL);
			MRef<SdpPacket*> sdp = (SdpPacket*)*inv->getContent();
			string confid = sdp->getSessionLevelAttribute("confId");
			string numToConnect = sdp->getSessionLevelAttribute("conf_#participants");
			
			
			//this is a join packet and contains an advice list. The list is created from the
			// packet here in order to send it to the GUI for display.
			
			
			int num = 0;

   //--- Convert each digit char and add into result.
   			int t=0;
			while (numToConnect[t] >= '0' && numToConnect[t] <='9') {
      				num = (num * 10) + (numToConnect[t] - '0');
      				t++;
   			}
			for(t=0;t<num;t++)
				//connectList[t]=  sdp->getSessionLevelAttribute("participant_"+itoa(t+1));
				connectList.push_back((ConfMember(sdp->getSessionLevelAttribute("participant_"+itoa(t+1)),"")));
			//cerr << "DDH: "+numToConnect[0]<< endl;
			//cerr << "DDH: "+numToConnect[1]<< endl;
			cerr << "DDH: "+itoa(num)<< endl;
			//int num=atoi(numToConnect);
			//string gID = sdp->getSessionLevelAttribute("p2tGroupIdentity");
			//string prot = sdp->getSessionLevelAttribute("p2tGroupListProt");
			// get a session from the mediaHandler
			MRef<Session *> mediaSession = 
				mediaHandler->createSession(phoneconf->securityConfig, pkt->getCallId() );

			MRef<SipDialogConfig*> callConf = new SipDialogConfig(phoneconf->inherited);
			if( id ){
				cerr << "Got a call from Id " << id->getSipUri() << endl;
				callConf->useIdentity( id, false );
			}

#ifdef IPSEC_SUPPORT
			MRef<MsipIpsecAPI *> ipsecSession = new MsipIpsecAPI(mediaHandler->getExtIP(), phoneconf->securityConfig);
			
			//MRef<SipDialogVoip*> voipCall = new SipDialogVoip(getDialogContainer(), callConf, 
			//					phoneconf, mediaSession, pkt->getCallId(), ipsecSession ); 
			//BM is it safe to pass the list like this?
			MRef<SipDialog*> voipConfCall( new SipDialogConfVoip(sipStack, callConf, 
								phoneconf, mediaSession, &connectList,confid, pkt->getCallId(), ipsecSession )); 
	
#else	
			
			MRef<SipDialog*> voipConfCall( new SipDialogConfVoip(sipStack, callConf, 
								phoneconf, mediaSession, &connectList,confid, pkt->getCallId()));
#endif
			sipStack->addDialog(voipConfCall);

			SipSMCommand cmd(pkt, SipSMCommand::remote, SipSMCommand::TU);
			cmd.setDispatchCount(dispatchCount);

			getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
			mdbg << cmd << end;
		}
		else if(isConfConnect) {
			MRef<SipHeaderValueTo*> to = pkt->getHeaderValueTo();
			string uri;
			MRef<SipIdentity *> id = NULL;

			if( to ){
				id = phoneconf->getIdentity( to->getUri() );
			}
#ifdef DEBUG_OUTPUT			
			mdbg << "DefaultDialogHandler:: creating new SipDialogConfVoip" << end;
#endif			
			

			massert(dynamic_cast<SdpPacket*>(*inv->getContent())!=NULL);
			MRef<SdpPacket*> sdp = (SdpPacket*)*inv->getContent();
			string confid = sdp->getSessionLevelAttribute("confId");
			MRef<Session *> mediaSession = 
				mediaHandler->createSession(phoneconf->securityConfig, pkt->getCallId() );

			MRef<SipDialogConfig*> callConf = new SipDialogConfig(phoneconf->inherited);

			if( id ){
				cerr << "Got a call from Id " << id->getSipUri() << endl;
				callConf->useIdentity( id, false );
			}

#ifdef IPSEC_SUPPORT
			MRef<MsipIpsecAPI *> ipsecSession = new MsipIpsecAPI(mediaHandler->getExtIP(), phoneconf->securityConfig);
			
			//MRef<SipDialogVoip*> voipCall = new SipDialogVoip(getDialogContainer(), callConf, 
			//					phoneconf, mediaSession, pkt->getCallId(), ipsecSession ); 
			//BM is it safe to pass the list like this?
			MRef<SipDialog*> voipConfCall( new SipDialogConfVoip(sipStack, callConf, 
								phoneconf, mediaSession, confid, pkt->getCallId(), ipsecSession )); 
	
#else	
			
			MRef<SipDialog*> voipConfCall( new SipDialogConfVoip(sipStack, callConf, 
								phoneconf, mediaSession, confid, pkt->getCallId()));
#endif
			sipStack->addDialog(voipConfCall);

			SipSMCommand cmd(pkt, SipSMCommand::remote, SipSMCommand::TU);
			cmd.setDispatchCount(dispatchCount);

			getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
			mdbg << cmd << end;
		}
		//start SipDialogVoIP
		else{
			MRef<SipHeaderValueTo*> to = pkt->getHeaderValueTo();
			string uri;
			MRef<SipIdentity *> id = NULL;

			if( to ){
				id = phoneconf->getIdentity( to->getUri() );
			}

			// get a session from the mediaHandler
			MRef<Session *> mediaSession = 
				mediaHandler->createSession(phoneconf->securityConfig, pkt->getCallId() );

			MRef<SipDialogConfig*> callConf = new SipDialogConfig(phoneconf->inherited);

			if( id ){
				cerr << "Got a call from Id " << id->getSipUri() << endl;
				callConf->useIdentity( id, false );
			}

#ifdef IPSEC_SUPPORT
			MRef<MsipIpsecAPI *> ipsecSession = new MsipIpsecAPI(mediaHandler->getExtIP(), phoneconf->securityConfig);
			
			//MRef<SipDialogVoip*> voipCall = new SipDialogVoip(getDialogContainer(), callConf, 
			//					phoneconf, mediaSession, pkt->getCallId(), ipsecSession );
			MRef<SipDialog*> voipCall( new SipDialogVoipServer(sipStack, callConf, 
								phoneconf, mediaSession, pkt->getCallId(), ipsecSession )); 
	
#else	
			
			MRef<SipDialog*> voipCall( new SipDialogVoipServer(sipStack, callConf, 
								phoneconf, mediaSession, pkt->getCallId()));
#endif
			sipStack->addDialog(voipCall);

			
			MRef<SipTransaction*> ir( new SipTransactionInviteServerUA(
						sipStack,
						voipCall,
						pkt->getCSeq(),
						pkt->getCSeqMethod(),
						pkt->getLastViaBranch(),
						voipCall->dialogState.callId) );
			voipCall->registerTransaction(ir);

/*			MRef<SipTransaction*> ir( new SipTransactionInviteServerUA(
						sipStack,
						voipCall,
						command.getCommandPacket()->getCSeq(),
						command.getCommandPacket()->getCSeqMethod(),
						command.getCommandPacket()->getLastViaBranch(),
						dialogState.callId) );
			voipCall->registerTransaction(ir);
*/

			
			
			SipSMCommand cmd(pkt, SipSMCommand::remote, SipSMCommand::transaction);
			cmd.setDispatchCount(dispatchCount);

			getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
			mdbg << cmd << end;
		}
		return true;
	}


	if (source==SipSMCommand::remote && pkt->getType()=="MESSAGE"){

		MRef<SipRequest*> im = (SipRequest*)*pkt;
			 
#ifdef DEBUG_OUTPUT			
		mdbg << "DefaultDialogHandler:: creating new server transaction for incoming SipIMMessage" << end;
#endif			
		string branch = im->getDestinationBranch();

		MRef<SipTransaction*> trans = new SipTransactionNonInviteServer(sipStack, this, im->getCSeq(), im->getCSeqMethod(), branch, im->getCallId());
		registerTransaction(trans);

		SipSMCommand cmd(pkt, SipSMCommand::remote, SipSMCommand::transaction);
		cmd.setDispatchCount(dispatchCount);
		//SipSMCommand cmd(command);
		//cmd.setSource(SipSMCommand::remote);
		//cmd.setDestination(SipSMCommand::transaction);
			
				//voipCall->handleCommand(command);
		getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);

		sendIMOk(im, trans->getBranch() );

		massert(dynamic_cast<SipMessageContentIM*>(*im->getContent())!=NULL);

		MRef<SipMessageContentIM*> imref = (SipMessageContentIM*)*im->getContent();

		string from =  im->getHeaderValueFrom()->getUri().getUserName()+"@"+ 
			im->getHeaderValueFrom()->getUri().getIp();
		string to =  im->getHeaderValueTo()->getUri().getUserName()+"@"+ 
			im->getHeaderValueTo()->getUri().getIp();

		CommandString cmdstr("", SipCommandString::incoming_im, imref->getString(), from, to );
		getDialogContainer()->getCallback()->handleCommand("gui", cmdstr );
		return true;

	}

//	if (command.getType()==SipSMCommand::COMMAND_PACKET){
	mdbg << "DefaultDialogHandler ignoring " << pkt->getString() << end; 
//	}

	return false;

}

bool DefaultDialogHandler::handleCommandString(int source, 
		int /*destination*/,  	//Not used (it's either TU or ANY)
		CommandString &cmdstr, 
		int dispatchCount ){
	if (dispatchCount>=2){
		mdbg << "WARNING: Command ["<< cmdstr.getOp()<<"] ignored (dispatched flag indication)"<< end;
		return true;
	}

	if (cmdstr.getOp() == SipCommandString::start_presence_client){
		cerr << "DefaultDialogHandler: Creating SipDialogPresenceClient for start_presence_client command"<< endl;
		
		MRef<SipDialogConfig*> conf = new SipDialogConfig(phoneconf->inherited);

		MRef<SipDialogPresenceClient*> pres(new SipDialogPresenceClient(sipStack, conf, phoneconf->useSTUN ));

		sipStack->addDialog( MRef<SipDialog*>(*pres) );
		
		CommandString command(cmdstr);
		cmdstr.setDestinationId(pres->getCallId());
		SipSMCommand cmd( cmdstr, SipSMCommand::remote, SipSMCommand::TU);
		cmd.setDispatchCount(dispatchCount);
		getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);

		return true;
	}
	
	if (cmdstr.getOp() == SipCommandString::start_presence_server){
		cerr << "DefaultDialogHandler: Creating SipDialogPresenceServer for start_presence_server command"<< endl;
		
		MRef<SipDialogConfig*> conf = new SipDialogConfig(phoneconf->inherited);

		MRef<SipDialogPresenceServer*> pres(new SipDialogPresenceServer(sipStack, conf, phoneconf->useSTUN ));

		sipStack->addDialog( MRef<SipDialog*>(*pres) );
		
		CommandString command(cmdstr);
		cmdstr.setDestinationId(pres->getCallId());
		SipSMCommand cmd( cmdstr, SipSMCommand::remote, SipSMCommand::TU);
		cmd.setDispatchCount(dispatchCount);
		getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);

		return true;
	}


	if (cmdstr.getOp() == SipCommandString::outgoing_im){
		//cerr << "DefaultDialogHandler: Creating SipTransactionClient for outgoing_im command"<< endl;
		//int im_seq_no= requestSeqNo();
		++dialogState.seqNo;
		MRef<SipTransaction*> trans = new SipTransactionNonInviteClient(sipStack, this, dialogState.seqNo, "MESSAGE", dialogState.callId);
		mdbg << "WWWWWWW: transaction created, branch id is <"<<trans->getBranch()<<">."<< end; 
		//cerr << "command standard arguments is <"<< command.getCommandString().getString() <<">"<< endl;
		registerTransaction(trans);
		sendIM( trans->getBranch(), cmdstr.getParam(), dialogState.seqNo, cmdstr.getParam2() );
		return true;
	}

	if (cmdstr.getOp() == SipCommandString::proxy_register){
		//merr << end << "CESC: DefaultDialogHandler: got proxy_register: "<< cmdstr.getString() << end << end;
		
		MRef<SipDialogConfig*> conf = new SipDialogConfig(phoneconf->inherited);
		
		MRef<SipIdentity *> identity;
		identity = phoneconf->getIdentity( cmdstr["identityId"] );
		
		string proxyDomainArg = cmdstr["proxy_domain"];
		
		/* Use appropriate identity ... 
		*/
		if( ! identity.isNull() ) {
			//FIXME: useSecurity is false ... useIdentity doesn't use it anyway ... 
			conf->useIdentity( identity, false ); 
		} else if (phoneconf->pstnIdentity && (cmdstr.getDestinationId()=="pstn" 
					|| (proxyDomainArg!="" && proxyDomainArg==phoneconf->pstnIdentity->sipDomain))){
			conf->useIdentity( phoneconf->pstnIdentity, false);
		}
		
		MRef<SipDialogRegister*> reg(new SipDialogRegister(sipStack, conf));
		
		sipStack->addDialog( MRef<SipDialog*>(*reg) );
		SipSMCommand cmd( cmdstr, SipSMCommand::remote, SipSMCommand::TU);
		cmd.setDispatchCount(dispatchCount);
		getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
		return true;
	}

#ifdef P2T_SUPPORT

	/*****
	 * P2T commands:
	 ****/

	//start P2T Session 
	if(cmdstr.getOp() == "p2tStartSession") {
		startP2TSession( SipSMCommand(cmdstr,source,destination) );
		return true;
	}

	//start GroupListServer 
	if(cmdstr.getOp() == "p2tStartGroupListServer") {
		grpListServer = new GroupListServer(phoneconf, 0);
		grpListServer->start();
		return true;
	}

		//stop GroupListServer 
	if(cmdstr.getOp() == "p2tStopGroupListServer") {
		grpListServer->stop();
		grpListServer=NULL;
		return true;
	}
		
	//p2tSession is accepted
	if(cmdstr.getOp() == "p2tSessionAccepted") {
		inviteP2Taccepted(SipSMCommand(cmdstr, source , destination) );
		return true;
	}

	//add user to a P2T Session
	if(cmdstr.getOp() == "p2tAddUser") {

		//get SipDialogP2T			
		MRef<SipDialogP2T*>p2tDialog;
		getP2TDialog(cmdstr.getParam(), p2tDialog);

		//add user
		string user = cmdstr.getParam2();
		p2tDialog->getGroupList()->addUser(user);

		//create callConfig
		MRef<SipDialogConfig*> callConf = MRef<SipDialogConfig*>(new SipDialogConfig(phoneconf->inherited) );


		//check user uri and modify DialogConfig
		if(modifyDialogConfig(user, callConf)==false){
			p2tDialog->removeUser(user, "uri malformed", "");
			return true;
		}

		MRef<SipDialogP2Tuser*> p2tUserDialog = new SipDialogP2Tuser(sipStack, callConf, phoneconf, p2tDialog);
		sipStack->addDialog(*p2tUserDialog);

		//set CallId and localStarted in GroupMemberList
		p2tDialog->getGroupList()->getUser(user)->setCallId(p2tUserDialog->getCallId());
		p2tDialog->getGroupList()->getUser(user)->setLocalStarted(true);

		//send invite message
		CommandString inv(p2tUserDialog->getCallId(), SipCommandString::invite, user);
		SipSMCommand cmd(SipSMCommand(inv, SipSMCommand::remote, SipSMCommand::TU));
		getDialogContainer()->enqueueCommand( cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE );

		return true;
	}
#endif

	mdbg << "DefaultDialogHandler ignoring command " << cmdstr.getString() << end; 

	return false;
}


bool DefaultDialogHandler::handleCommand(const SipSMCommand &command){
	mdbg << "DefaultDialogHandler: got command "<< command << end;
	int src = command.getSource();
	int dst = command.getDestination();
	if ( !(dst==SipSMCommand::ANY || dst==SipSMCommand::TU) )
		return false;
	
	int dispatchCount = command.getDispatchCount();
	if (command.getType()==SipSMCommand::COMMAND_PACKET){
		return handleCommandPacket( src, dst, command.getCommandPacket(), dispatchCount );
	}else{
		massert(command.getType()==SipSMCommand::COMMAND_STRING);
		CommandString cmdstr = command.getCommandString();
		return handleCommandString( src, dst, cmdstr, dispatchCount );
	}
}

#ifdef P2T_SUPPORT
void DefaultDialogHandler::inviteP2Treceived(const SipSMCommand &command){
	//type casting
	MRef<SipMessage*> pack = command.getCommandPacket();
	MRef<SipRequest*> inv = MRef<SipRequest*>((SipRequest*)*pack);
	
	//get the GroupList from the remote GroupListServer
	MRef<GroupList*>grpList;
	massert(dynamic_cast<SdpPacket*>(*inv->getContent())!=NULL);
	MRef<SdpPacket*> sdp = (SdpPacket*)*inv->getContent();
	string gServer = sdp->getSessionLevelAttribute("p2tGroupListServer");
	string gID = sdp->getSessionLevelAttribute("p2tGroupIdentity");
	string prot = sdp->getSessionLevelAttribute("p2tGroupListProt");
				
	//parse gServer
	string server="";
	int port=0;
	uint32_t k=0;
				
	for(/*k*/;k<gServer.size();k++){
		if(gServer[k]!=':')
			server+=gServer[k];
		else
			break;
	}
				
	for(++k;k<gServer.size();k++)
		port = (port*10) + (gServer[k]-'0');
				
	
	//download Group Member List if protocol is http/xml
	if(prot=="http/xml"){		
		MRef<GroupListClient*>client = new GroupListClient();
		grpList = client->getGroupList(gID,&server[0],port);
		client=NULL;
	}
	else{
#ifdef DEBUG_OUTPUT			
		mdbg << "DefaultDialogHandler:: Unknown GroupListProtocol "<<prot<< end;
#endif
		return;
	}
	
	
	//get inviting user name
	string inv_user = inv->getHeaderValueFrom()->getUri().getUserId()+"@"+ 
		inv->getHeaderValueFrom()->getUri().getIp();	
	
	//start SipDialogP2T only, if there isn't already
	//one started with this GroupIdentity
	MRef<SipDialogP2T*>p2tDialog;
	MRef<SipDialogConfig*> callConf;
	
	if(getP2TDialog(gID, p2tDialog)==false){
		//start new SipDialogP2T
		callConf = MRef<SipDialogConfig*>(new SipDialogConfig(phoneconf->inherited) );
		p2tDialog = new SipDialogP2T(sipStack, callConf, phoneconf); 
		p2tDialog->setGroupList(grpList);
		//p2tDialog->getDialogConfig().callId = gID;
		p2tDialog->setCallId(gID);
		MRef<SipDialog*> dlg = *p2tDialog;
		sipStack->addDialog(dlg);
		
		//send invitation to the GUI
		CommandString cmdstr(gID, "p2tInvitation", grpList->print(), inv_user);
		getDialogContainer()->getCallback()->handleCommand("gui", cmdstr );
		
		
	}
	
	//set now correct GroupList
	grpList = p2tDialog->getGroupList();
	
	//start SipDialogP2Tuser for inviting user
	callConf = MRef<SipDialogConfig*>(new SipDialogConfig(phoneconf->inherited) );
	MRef<SipDialogP2Tuser*> p2tDialogUser = new SipDialogP2Tuser(sipStack, callConf, phoneconf, p2tDialog);
	//p2tDialogUser->getDialogConfig().callId = command.getCommandPacket()->getCallId();
	p2tDialogUser->setCallId( command.getCommandPacket()->getCallId() );
	
	sipStack->addDialog(*p2tDialogUser);
	
	//user doesn't exist in grpList and will be added to it
	if(grpList->isParticipant(inv_user)==false){
		
		//check memberlist
		if(grpList->getMembership()==P2T::MEMBERSHIP_OPEN ||
			(grpList->getMembership()==P2T::MEMBERSHIP_RESTRICTED && grpList->isMember(inv_user))){
		
			//adduser to grpList
			grpList->addUser(inv_user);
			
			//set CallId in Group Member List
			grpList->getUser(inv_user)->setCallId(p2tDialogUser->getCallId());
			grpList->getUser(inv_user)->setStatus(P2T::STATUS_WAITACCEPT);
		
			SipSMCommand cmd(command);
			cmd.setSource(SipSMCommand::remote);
			cmd.setDestination(SipSMCommand::TU);
			getDialogContainer()->enqueueCommand(cmd, LOW_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
		
			//inform GUI. 
			CommandString cmds(p2tDialogUser->getCallId(), "p2tAddUser", inv_user);
			getDialogContainer()->getCallback()->handleCommand("gui", cmds );
		}
		//if user is not in GroupList deny it
		else {
			CommandString rej(/*p2tDialogUser->getDialogConfig().callId*/ p2tDialogUser->getCallId(), SipCommandString::reject_invite);
			SipSMCommand cmd(rej, SipSMCommand::remote, SipSMCommand::TU);
			getDialogContainer()->enqueueCommand(cmd, LOW_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
			 
		}
		
	
	}
	else{
		//there is already a call started for this user
		if(grpList->getUser(inv_user)->getCallId()!=""){
			
			//we have a collision
			if(grpList->getUser(inv_user)->getLocalStarted()==true){
			
			 //not implemented
			
			}
			
			SipSMCommand cmd(command);
			cmd.setSource(SipSMCommand::remote);
			cmd.setDestination(SipSMCommand::TU);
			getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
			
			//reject this call
			CommandString cmds(p2tDialogUser->getCallId(), SipCommandString::reject_invite);
			SipSMCommand scmd(cmds, SipSMCommand::remote, SipSMCommand::TU);
			getDialogContainer()->enqueueCommand(scmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
		}
		
		//there is no call started for this user
		else{
			
			//set CallId in Group Member List
			grpList->getUser(inv_user)->setCallId(p2tDialogUser->getCallId());
			grpList->getUser(inv_user)->setStatus(P2T::STATUS_WAITACCEPT);
			
			SipSMCommand cmd(command);
			cmd.setSource(SipSMCommand::remote);
			cmd.setDestination(SipSMCommand::TU);
			getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
			
			//user has to accept it
			CommandString cmds(p2tDialogUser->getCallId(), "p2tAddUser");
			getDialogContainer()->getCallback()->handleCommand("gui", cmds );

		}
	
	
	}
	
	

	


}

void DefaultDialogHandler::inviteP2Taccepted(const SipSMCommand &command){
	
	//get P2TDialog
	MRef<SipDialogP2T*> p2tDialog;
	if(getP2TDialog(command.getCommandString().getParam(), p2tDialog)==false){	
		mdbg<<"DefaultDialogHandler::Couldn't find SipDialogP2T!"<<end;
		return;
	}

	//start the Group List Server and add GroupList
	MRef<GroupList*> grpList = p2tDialog->getGroupList();
	grpListServer = new GroupListServer(phoneconf, 0);
	grpListServer->start();
	grpListServer->addGroupList(grpList);
	
	//send accept_invite to all waiting SipDialogP2Tuser dialogs for this session
	for(uint32_t l=0; l<grpList->getAllUser().size(); l++){
		if(grpList->getAllUser()[l]->getStatus()==P2T::STATUS_WAITACCEPT){
			CommandString cmds(grpList->getAllUser()[l]->getCallId(), SipCommandString::accept_invite);
			SipSMCommand scmd(cmds, SipSMCommand::remote, SipSMCommand::TU);
			getDialogContainer()->enqueueCommand(scmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
		}
	}

			
	//Start SipDialogP2Tuser for all remaining participants in the Group Member List
	MRef<SipDialogConfig*> callConf;
	string user="";
	for(uint32_t k=0; k<grpList->getAllUser().size(); k++){
		user=grpList->getAllUser()[k]->getUri();
		
		//filter out own username
//		if(user==getDialogConfig().inherited.userUri)
		if(user==getDialogConfig()->inherited->sipIdentity->getSipUri())
			continue;
			
		// filter out users that have already started
		// a dialog resp. has a callId in the grpList.
		if(grpList->getAllUser()[k]->getCallId()!="")
			continue;
		
		//create callConfig
		callConf = MRef<SipDialogConfig*>(new SipDialogConfig(phoneconf->inherited) );	
		
		//check user uri and modify DialogConfig
		if(modifyDialogConfig(user, callConf)==false){
			p2tDialog->removeUser(user, "uri malformed", "");
			continue;
		}
		
			
				
		MRef<SipDialogP2Tuser*> p2tUserDialog = new SipDialogP2Tuser(sipStack, callConf, phoneconf, p2tDialog);
		sipStack->addDialog(*p2tUserDialog);
		
		//set CallId and localStarted in GroupMemberList
		grpList->getAllUser()[k]->setCallId(p2tUserDialog->getCallId());
		grpList->getAllUser()[k]->setLocalStarted(true);
		
		CommandString inv(p2tUserDialog->getCallId(), SipCommandString::invite, user);
        	SipSMCommand cmd(SipSMCommand(inv, SipSMCommand::remote, SipSMCommand::TU));
		getDialogContainer()->enqueueCommand( cmd, LOW_PRIO_QUEUE, PRIO_LAST_IN_QUEUE );
	}
	
	//Inform GUI
	CommandString cmdstr("", "p2tSessionCreated", p2tDialog->getCallId());
	getDialogContainer()->getCallback()->handleCommand("gui", cmdstr );



}


void DefaultDialogHandler::startP2TSession(const SipSMCommand &command){
	string xml;

	//Start SipDialogP2T
	MRef<SipDialogConfig*> callConf = MRef<SipDialogConfig*>(new SipDialogConfig(phoneconf->inherited) );
	MRef<SipDialogP2T*> p2tDialog( new SipDialogP2T(sipStack, callConf, phoneconf)); 
		
	//Create Group Member List from the first parameter of the
	//CommandString and set the correct Group Identity
	MRef<GroupList*> grpList = new GroupList(command.getCommandString().getParam());
	grpList->setGroupIdentity(p2tDialog->getCallId());
	p2tDialog->setGroupList(grpList);
	
	//add the Group Member List to the GroupListServer
	grpListServer->addGroupList(grpList);
		
	//add SipDialogP2T to the DialogContainer
	MRef<SipDialog*> dialog = *p2tDialog;
	sipStack->addDialog(dialog);
		
		
	//Start SipDialogP2Tuser for all participants in the Group Member List
	string user="";
	for(uint32_t k=0; k<grpList->getAllUser().size(); k++){
		user=grpList->getAllUser()[k]->getUri();
		
		//filter out own username
		//if(user==getDialogConfig().inherited.userUri)
		if(user==getDialogConfig()->inherited->sipIdentity->getSipUri())
			continue;
		
		
		//create callConfig
		callConf = MRef<SipDialogConfig*>(new SipDialogConfig(phoneconf->inherited) );	
		
		//check user uri and modify DialogConfig
		if(modifyDialogConfig(user, callConf)==false){
			p2tDialog->removeUser(user, "uri malformed", "");
			continue;
		}
		
		MRef<SipDialogP2Tuser*> p2tUserDialog = new SipDialogP2Tuser(sipStack, callConf, phoneconf, p2tDialog);
		sipStack->addDialog(*p2tUserDialog);
		
		//set CallId and localStarted in GroupMemberList
		grpList->getAllUser()[k]->setCallId(p2tUserDialog->getCallId());
		grpList->getAllUser()[k]->setLocalStarted(true);
		
		CommandString inv(p2tUserDialog->getCallId(), SipCommandString::invite, user);
        	SipSMCommand cmd(SipSMCommand(inv, SipSMCommand::remote, SipSMCommand::TU));
		getDialogContainer()->enqueueCommand( cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE );
	}

	//send GUI the Group Identity
	CommandString cmdstr("", "p2tSessionCreated", p2tDialog->getCallId());
	getDialogContainer()->getCallback()->handleCommand("gui", cmdstr );
	
}

bool DefaultDialogHandler::getP2TDialog(string GroupId, MRef<SipDialogP2T*>&p2tDialog){
	bool match=false;
//	list<MRef<SipDialog*> > *dialogs = getDialogContainer()->getDispatcher()->getDialogs();
	list<MRef<SipDialog*> > dialogs = getDialogContainer()->getDispatcher()->getDialogs();
	
	for (list<MRef<SipDialog*> >::iterator i=dialogs.begin(); i!=dialogs.end(); i++){
		MRef<SipDialog*> tmpd= *i;
		if ( tmpd->getCallId()==GroupId){
			match=true;
			p2tDialog = MRef<SipDialogP2T*>((SipDialogP2T*)*tmpd);
			break;
		}
	}
	
	return match;
}
#endif


bool DefaultDialogHandler::modifyDialogConfig(string user, MRef<SipDialogConfig *> dialogConfig){
	int startAddr=0;
	if (user.substr(0,4)=="sip:")
		startAddr = 4;
	
	if (user.substr(0,4)=="sips:")
		startAddr = 5;

	bool onlydigits=true;
	for (unsigned i=0; i<user.length(); i++)
		if (user[i]<'0' || user[i]>'9')
			onlydigits=false;
	if (onlydigits && phoneconf->usePSTNProxy){
		dialogConfig->useIdentity( phoneconf->pstnIdentity, false);
	}
	
	if (user.find(":", startAddr)!=string::npos){
		if (user.find("@", startAddr)==string::npos){
			//malformed
			return false;
		}
		
		string proxy;
		string port;
		uint32_t i=startAddr;
		while (user[i]!='@')
			if (user[i]==':')
				return false; //malformed
			else
				i++;
		i++;
		while (user[i]!=':')
			proxy = proxy + user[i++];
		i++;
		while (i<user.size())
			if (user[i]<'0' || user[i]>'9')
				return false; //malformed
			else
				port = port + user[i++];
		
		int iport = atoi(port.c_str());
				
//		merr << "IN URI PARSER: Parsed port=<"<< port <<"> and proxy=<"<< proxy<<">"<<end;
		
		try{
			//dialogConfig->inherited->sipIdentity->getSipProxy()->sipProxyIpAddr = new IP4Address(proxy);
			dialogConfig->inherited->sipIdentity->getSipProxy()->sipProxyAddressString = proxy;
			dialogConfig->inherited->sipIdentity->getSipProxy()->sipProxyPort = iport;
		}catch(HostNotFound & exc){
			merr << "Could not resolve PSTN proxy address:" << end;
			merr << exc.what();
			merr << "Will use default proxy instead" << end;
		}
	
	}
	
	return true;
}


void DefaultDialogHandler::sendIMOk(MRef<SipRequest*> bye, const string &branch){
        MRef<SipResponse*> ok= new SipResponse( branch, 200,"OK", MRef<SipMessage*>(*bye) );
        ok->getHeaderValueTo()->setParameter("tag",dialogState.localTag);

        MRef<SipMessage*> pref(*ok);
        SipSMCommand cmd( pref, SipSMCommand::TU, SipSMCommand::transaction);
        getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
}


void DefaultDialogHandler::sendIM(const string &branch, string msg, int im_seq_no, string toUri){
	
	uint32_t posAt;
	
	posAt = toUri.find("@");
	if( posAt == string::npos ) { //toUri does not have a domain ...
		//get one, from the default identity
		if( getDialogConfig()->inherited->sipIdentity->sipDomain != "" ) {
			toUri += "@" + getDialogConfig()->inherited->sipIdentity->sipDomain;
		} else {
			#ifdef DEBUG_OUTPUT
			cerr << "DefaultDialogHandler::sendIM - toUri without domain" << endl;
			#endif
		}
	}
	#ifdef DEBUG_OUTPUT
	cerr << "DefaultDialogHandler::sendIM - toUri = " << toUri <<  endl;
	#endif
	
	MRef<SipRequest*> im = SipRequest::createSipMessageIMMessage(
			std::string(branch),
			std::string(dialogState.callId),
			toUri, 	
			getDialogConfig()->inherited->sipIdentity->getSipUri(),
			im_seq_no,
			msg
			);

	//FIXME: there should be a SipIMDialog, just like for register messages ...
	// 	otherwise, we cannot keep track of local/remote tags, callids, etc ... 
	//	very useful for matching incoming and outgoing IMs ...
	im->getHeaderValueFrom()->setParameter("tag","12345678"); //we need a from tag ... anything ... 

	MRef<SipMessage*> pref(*im);
	SipSMCommand cmd( pref, SipSMCommand::TU, SipSMCommand::transaction);
	getDialogContainer()->enqueueCommand(cmd, HIGH_PRIO_QUEUE, PRIO_LAST_IN_QUEUE);
}

