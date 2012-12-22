/**
   \file hos_sendosc.cc
   \ingroup apphos
   \brief Send OSC messages, read from stdin

   \author Giso Grimm
   \date 2011

   \section license License (GPL)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

*/

#include <lo/lo.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <iostream>

int main(int argc, char** argv)
{
  if( argc != 2){
    fprintf(stderr,"Usage: sendosc url\n");
    return 1;
  }
  lo_address lo_addr(lo_address_new_from_url( argv[1] ));
  lo_address_set_ttl( lo_addr, 1 );
  char rbuf[0x4000];
  while(!feof(stdin)){
    memset(rbuf,0,0x4000);
    char* s = fgets(rbuf,0x4000-1,stdin);
    if( s ){
      rbuf[0x4000-1] = 0;
      if( rbuf[0] == '#' )
	rbuf[0] = 0;
      if( strlen(rbuf) )
	if( rbuf[strlen(rbuf)-1] == 10 )
	  rbuf[strlen(rbuf)-1] = 0;
      if( strlen(rbuf) ){
	if( rbuf[0] == ',' ){
	  double val(0.0f);
	  sscanf(&rbuf[1],"%lg",&val);
	  struct timespec slp;
	  slp.tv_sec = floor(val);
	  val -= slp.tv_sec;
	  slp.tv_nsec = 1000000000*val;
	  nanosleep(&slp,NULL);
	}else{
	  char addr[0x4000];
	  memset(addr,0,0x4000);
	  float val(0.0f);
	  float val1(0.0f);
	  char sval[0x4000];
	  int narg = sscanf(rbuf,"%s %g %g",addr,&val,&val1);
	  switch( narg ){
	  case 1 :
	    narg = sscanf(rbuf,"%s %s",addr,sval);
	    if( narg == 1 )
	      lo_send( lo_addr, addr, "");
	    else
	      lo_send( lo_addr, addr, "s", sval );
	    break;
	  case 2 :
	    lo_send( lo_addr, addr, "f", val);
	    break;
	  case 3 :
	    lo_send( lo_addr, addr, "ff", val, val1);
	    break;
	  }
	}
      }
      //fprintf(stderr,"-%s-%s-%g-\n",rbuf,addr,val);
    }
  }
}
