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



#ifndef WIN32
#ifndef _MSC_VER
#include<libmutil/Timestamp.h>




Timestamp ts;

using namespace std;

#define MAX_TIMESTAMPS 256

string id_names[25] = { "invite_start", "invite_end", "mikey_start", "mikey_end", "ringing", "packet_in", "packet_out", "tls_start", "tls_end", "dh_precompute_start", "dh_precompute_end", "mikey_create_start", "mikey_create_end", "rand_start", "rand_end", "sign_start", "sign_end", "auth_start", "auth_end",  "mikey_parse_start", "mikey_parse_end", "tgk_start", "tgk_end", "user_accept" ,"tmp"};

Timestamp::Timestamp(){
	//tv = new struct timeval;
	tz = new struct timezone;
	values = new struct timeval[ MAX_TIMESTAMPS ];
	strings= new string[ MAX_TIMESTAMPS + 1];
	ids = new int32_t[MAX_TIMESTAMPS];
	index = 0;
	auto_id=-1;
}

Timestamp::~Timestamp(){
	delete tz;
	//delete tv;
	delete [] values;
	delete [] strings;
	delete [] ids;
}

void Timestamp::save( uint32_t id ){
	ids[index] = id;
	gettimeofday( &values[index], tz );
	index = ( index + 1 ) % MAX_TIMESTAMPS;
	//values[ index++ ] = ((uint64_t)tv->tv_sec << 32) |tv->tv_usec;
}

void Timestamp::save( string s){
	ids[index] = auto_id--;
//	cerr << "Placing string "<< s << " on index " << -(auto_id+1) << endl;
	strings[-(auto_id+1)] = s;
//	cerr << "strings[1]="<<strings[1]<< endl;
	gettimeofday( &values[index++], tz );
	//values[ index++ ] = ((uint64_t)tv->tv_sec << 32) |tv->tv_usec;
}
void Timestamp::print(){
	uint32_t i;
	struct timeval temp;
	ofstream file( FILE_NAME );
	file << "Saved timestamps: " << endl;
	temp = values[0];
	for( i = 0 ; i < index && i < MAX_TIMESTAMPS; i++ ){
//		cerr << "using string index "<< -ids[i]<< endl;
		if (ids[i]<0){
			string val = strings[-ids[i]];
//			cerr << "will write "<<val<< endl;
			file << "  " << val << ":\t" << values[i].tv_sec << ":\t" << values[i].tv_usec <<"\t"<< (values[i].tv_sec - temp.tv_sec)*1000000 + values[i].tv_usec - temp.tv_usec <<endl;
		}else
			file << "  " << id_names[ ids[i] ] << ":\t" << values[i].tv_sec << ":\t" << values[i].tv_usec <<"\t"<< (values[i].tv_sec - temp.tv_sec)*1000000 + values[i].tv_usec - temp.tv_usec <<endl;
		temp = values[i];
	}

	
}

void Timestamp::init(std::string filename, std::string init_data){
	
	this->filename=filename;
	
	ofstream file (&filename[0]);
	//write init_data in file
	file << init_data << endl;
	file.close();
}


void Timestamp::start(){
	timeval tim;
	gettimeofday(&tim, NULL);
	startTime=tim.tv_sec + (tim.tv_usec/1000000.0);
}

void Timestamp::stop(){
	timeval tim;
	gettimeofday(&tim, NULL);
	stopTime=tim.tv_sec + (tim.tv_usec/1000000.0);
}

string Timestamp::writeElapsedTime(std::string descr){
	double elapsedTime = stopTime-startTime;
	
	//convert to string
	ostringstream Str;
	Str<<elapsedTime;
	string s_elapsedTime(Str.str());
	
	//enter value into file
	ofstream file (&this->filename[0], ios::app);
	file << descr << ";" << s_elapsedTime <<endl;
	file.close();
	
	return s_elapsedTime;
	
}
#endif
#endif
