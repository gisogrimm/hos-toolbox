/**
   \file hos_markerbroadcast.cc
   \ingroup apphos
   \brief Receive and process OSC marker messages.

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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


int b_run;

void err_handler(int num, const char *msg, const char *where)
{
  fprintf(stderr,"liblo: %d %s (%s)\n",num,msg,where);
}

int _quit(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  b_run = 0;
  return 0;
}

int _marker(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  char cmd[1024];
  char* m;
  if( (argc == 1) && (types[0] == 's') ){
    m = &(argv[0]->s);
    printf("%s\n",m);
    if( strncmp(m,"preset:",7)==0 ){
      sprintf(cmd,"bash -c \"cat presets/%s.preset | ./src/sendosc osc.udp://224.1.2.3:6978/\"",m+7);
      //DEBUG(cmd);
      system(cmd);
    }
    if(strncmp(m,"ardour:",7)==0 ){
      sprintf(cmd,"bash -c \"send_osc 3819 /ardour/%s\"",m+7);
      //DEBUG(cmd);
      system(cmd);
    }
  }
  return 0;
}

int main(int argc, char** argv)
{
  lo_server_thread lost;
  b_run = 1;
  lost = lo_server_thread_new("9999",err_handler);
  lo_server_thread_add_method(lost,"/quit","",_quit,NULL);
  lo_server_thread_add_method(lost,"/marker","s",_marker,NULL);
  lo_server_thread_start(lost);
  while(b_run){
    sleep(1);
  }
  lo_server_thread_stop(lost);
  lo_server_thread_free(lost);
}
