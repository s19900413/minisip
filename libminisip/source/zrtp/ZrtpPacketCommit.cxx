/*
 Copyright (C) 2006 Werner Dittmann
 
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

/*
 * Authors: Werner Dittmann <Werner.Dittmann@t-online.de>
 */

#include <libminisip/zrtp/ZrtpPacketCommit.h>
#include <malloc.h>


ZrtpPacketCommit::ZrtpPacketCommit() {

    void *p = malloc(sizeof (CommitPacket_t));

    if ( p == NULL) {
    }

    zrtpHeader = (zrtpPacketHeader_t *)&((CommitPacket_t *)p)->hdr;	// the standard header
    commitHeader = (Commit_t *)&((CommitPacket_t *)p)->commit;

    setZrtpId();
    setLength(COMMIT_LENGTH + MESSAGE_LENGTH);
    setMessage(CommitMsg);
}

ZrtpPacketCommit::ZrtpPacketCommit(char *data) {
    zrtpHeader = (zrtpPacketHeader_t *)&((CommitPacket_t *)data)->hdr;	// the standard header
    commitHeader = (Commit_t *)&((CommitPacket_t *)data)->commit;
}

