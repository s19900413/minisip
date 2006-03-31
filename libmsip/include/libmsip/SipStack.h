/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Copyright (C) 2004 
 *
 * Authors: Erik Eliasson <eliasson@it.kth.se>
 *          Johan Bilien <jobi@via.ecp.fr>
*/


/*

 +-SipStack-------------------------------------+
 |                                              |  invite(string)
 | +-SipDialogContainer---+                     |<----------------------------------
 | |                      | addDialog(d)        |
 | |                      |<----------------    | handleCommand(SipSMCommand)
 | |                      |                     |<----------------------------------
 | |                      | handleCommand(CS)   |
 | |                      |-------------------->| sipcb_handleCommand(CommandString)
 | |                      |                     |---------------------------------->
 | |                      | sipcb_handleCommand |
 | |                      |-------------------->|  setDefaultDialog()
 | |                      |                     |<----------------------------------
 | |                      | enqueuePacket()     |
 | |                      |<----------------    |  addDialog(d)
 | |                      |                     |<----------------------------------
 | |                      | enqueueCommand()    |
 | |                      |<----------------    |
 | |                      |                     |
 | | [call_list]          | handleSipMessage()  |
 | | [defaultHandler]     |<---+                |
 | +----------------------+    |                |
 |                             |                |
 | [SipMessageTransport]-------+                |
 | [SipSoftPhoneConfiguration]                  |
 |                                              |
 |                                              |
 +----------------------------------------------+

*/

#ifndef LIBMSIP_SipStack_H
#define LIBMSIP_SipStack_H


#include<libmsip/libmsip_config.h>


#include<libmutil/minilist.h>
//#include<libmsip/SipCallback.h>
#include<libmsip/SipTransaction.h>
#include<libmsip/SipDialogContainer.h>
#include<libmsip/SipDialogConfig.h>
#include<libmsip/SipTimers.h>

#include<libmsip/SipMessageTransport.h>
#include<libmutil/cert.h>

class SipDialog;
class SipTransaction;

using namespace std;

class LIBMSIP_API SipStack: public SipSMCommandReceiver, public Runnable{

	public:
		SipStack( MRef<SipCommonConfig*> stackConfig,
				//string localIpString,
				//string externalContactIP,
				//int32_t localUdpPort=5060,
				//int32_t localTcpPort=5060,
				//int32_t externalContactUdpPort=5060,
				//int32_t localTlsPort=5061,
				MRef<certificate_chain *> cert=NULL,	//The certificate chain is used by TLS 
								//TODO: TLS should use the whole chain instead of only the first certificate --EE
				MRef<ca_db *> cert_db = NULL,
				MRef<TimeoutProvider<string, MRef<StateMachine<SipSMCommand,string>*> > *> tp= NULL
			  );


		virtual std::string getMemObjectType(){return "SipStack";}
		
		//void init();
                virtual void run();

		MRef<SipDialogContainer*> getDialogContainer();

		bool handleCommand(const SipSMCommand &command);
		
		void setCallback(MRef<CommandReceiver*> callback);
		MRef<CommandReceiver *> getCallback();

		void setConfCallback(MRef<CommandReceiver*> callback); // Hack to make the conference calling work - should not be here FIXME
		MRef<CommandReceiver *> getConfCallback();
		
		void setDefaultHandler(MRef<SipDialog*> d);

		void addDialog(MRef<SipDialog*> d);

		MRef<SipMessageTransport *> getSipTransportLayer(){return transportLayer;}

		MRef<TimeoutProvider<string, MRef<StateMachine<SipSMCommand,string>*> > *> getTimeoutProvider();

		//void setSipMessageTransport(...);
		MRef<SipTimers*> getTimers();
		MRef<SipCommonConfig*> getStackConfig(){return config;}

		void addSupportedExtension(string extension);
		string getAllSupportedExtensionsStr();
		bool supports(string extension);
                
	private:
		MRef<SipTimers*> timers;
		MRef<SipCommonConfig *> config;
		//SipCallback *callback;
		MRef<CommandReceiver*> callback;
		MRef<CommandReceiver*> confCallback;	//hack to make conference calling work until the ConfMessageRouter is removed
		MRef<SipDialogContainer*> dialogContainer;

		MRef<SipMessageTransport *> transportLayer;
		
		MRef<TimeoutProvider<string, MRef<StateMachine<SipSMCommand,string>*> > *> timeoutProvider;

		list<string> sipExtensions;
};


//#include<libmsip/SipDialog.h>

#endif
