/*
  Copyright (C) 2005, 2004 Erik Eliasson, Johan Bilien
  
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
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * Authors: Erik Eliasson <eliasson@it.kth.se>
 *          Johan Bilien <jobi@via.ecp.fr>
*/

/* Name
 * 	SipURI.h
 * Author
 * 	Cesc Santasusana, c e s c dot s a n t a A{T g m a i l dot co m; 2005
 * Purpose
 * 
*/

#ifndef SIPURI_H
#define SIPURI_H

#include<libmsip/libmsip_config.h>

#define SIP_URI_USERNAME_DEFAULT "UNKNOWN"
#define SIP_URI_USER_TYPE_DEFAULT "phone"

#include<sys/types.h>

#include<libmutil/MemObject.h>

using namespace std;

/**
A SipUri representation.
It can create a uri from a string (parse it), 
or from parameters that can be set.

The scheme is:

"displayName" <protocolId:userName@ip:port;userType;transport>

FIXME: User and transport are the only understood params ... the rest
are discarded.
*/
class LIBMSIP_API SipURI : public MObject{
	public:
		/**
		Basic constructor ... it creates an invalid uri
		*/
		SipURI() { clear(); };

		~SipURI();
		
		/**
		This constructor, and the setUri function, parse 
		a uri string. If everything is ok, the uri object is valid
		*/
		SipURI(string build_from);
		void setUri( string buildFrom );

		/**
		Given a string with the inside part of the uri (between, but without, 
		the '<' and '>' ), parse and set the right parameters.
		*/
		void parseUserInfo( string userInfo );
		                
		/**
		Sets at once all these params and creates a valid uri ...
		dangerous ... use at own risk.
		What it does?
		For some obscure reason, some objects create sip uris providing as username a kinda uri, 
		for example cesc@domain.org, and then also provide an ip address, which has "precedence".
		So ... we parse the username info provided and then overwrite the ip part.
		Also ... note that if we are provided a username like sip:user (we don't know the domain, 
		so when calling we don't use it, for example, the TO: ) ... but this sip uri class interprets
		(correctly) that the user is an ip ... so ... if this happens, in this particular function,
		we move the "ip = user" to the "userName = user" ...
		*/
		void setParams(string userName, string ip, string userType, int32_t port);
		
		/**
		Return the whole uri ...
		*/
		string getString() const;
		
		/**
		Return only userName@ip
		*/
		string getUserIpString() const;
		
		/**
		Return the uri in a valid form to be used as a request uri, 
		that is, without: display name, '<', '>' and parameters.
		*/
		string getRequestUriString() const;

		virtual std::string getMemObjectType(){return "SipURI";}

		void setDisplayName(string id);
		string getDisplayName() const;
		
		void setProtocolId(string protocolId);
		string getProtocolId() const;
	
		void setUser(string id);
		string getUserName() const;

		void setIp(string ip);
		string getIp() const;

		void setPort(int32_t port);
		int32_t getPort() const;

		void setUserType(string userType);
		string getUserType() const;

		void setTransport(string transp);
		string getTransport() const;
		
		/**
		Use it to check whether the uri has valid stuff in it ... 
		don't use if not valid.
		If invalid and used, the getXXXString functions will return nothing
		*/
		bool isValid() const { return validUri; };
		
		/**
		Use at your own risk ... forces uri in a (in)valid state
		*/
		void makeValid( bool valid ) { validUri = valid; };
		
		void clear();

	private:
		string displayName;
		string protocolId;
		string userName;
		string ip;
		int32_t port;
		string userType;
		string transport;
		
		bool validUri;
};

#endif
