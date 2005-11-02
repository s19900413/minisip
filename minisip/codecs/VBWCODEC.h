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

#ifndef VBWCODEC_H
#define VBWCODEC_H

#include"Codec.h"
#include"../soundcard/resampler/Resampler.h"

class VBWCodecState : public CodecState{
	public:
		VBWCodecState();

                /**
                 * @returns Number of bytes in output buffer
                 */
                virtual uint32_t encode(void *in_buf, int32_t in_buf_size, void *out_buf);

                /**
                 *
                 * @returns Number of frames in output buffer
                 */
                virtual uint32_t decode(void *in_buf, int32_t in_buf_size, void *out_buf);

	private:
		MRef<Resampler *> resampler;
};

class VBWCodec : public AudioCodec{
	public:
		virtual MRef<CodecState *> newInstance();
		
		VBWCodec();
		virtual ~VBWCodec();
	
		/**
		 * @return Requested sampling freq for the CODEC
		 */
		virtual int32_t getSamplingFreq();

		/**
		 * Time in milliseconds to put in each frame/packet. This is 20ms for the VBW codec.
		 */
		virtual int32_t getSamplingSizeMs();

		/**
		 * size of the output of the codec in bytes. This is 160.
		 */
//		virtual int32_t getEncodedNrBytes();
		
		virtual int32_t getInputNrSamples();
		
		virtual string getCodecName();
		
		virtual string getCodecDescription();

		virtual uint8_t getSdpMediaType();

		virtual string getSdpMediaAttributes();
		
		virtual std::string getMemObjectType(){return "VBWCodec";}		
		
		
};

#endif