/**
   \file hos_delay.cc
   \ingroup apphos
   \brief Time varying delay

   \author Giso Grimm
   \date 2013

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

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include "jackclient.h"
#include "osc_helper.h"

#define MAXDELAY 480000
#define OSC_ADDR "224.1.2.3"
#define OSC_PORT "6978"

namespace HoS {

  /**
     \ingroup apphos
  */
  class delay_t : public jackc_t, public TASCAR::osc_server_t {
  public:
    delay_t(const std::string& name);
    ~delay_t();
    void set_delay(double delay, double ttime);
    void run();
    void quit() { b_quit = true;};
    static int osc_set_delay(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    static int osc_quit(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  private:
    int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer);
    double dt;
    float delayline[MAXDELAY];
    double target_delay;
    double target_time;
    double osc_delay;
    double osc_targettime;
    double current_delay;
    uint32_t in_pos;
    uint32_t out_pos;
    bool b_quit;
    pthread_mutex_t mutex;
  };

}

using namespace HoS;

delay_t::delay_t(const std::string& name)
  : jackc_t(name),
    TASCAR::osc_server_t(OSC_ADDR,OSC_PORT),
    dt(1.0/srate),
    target_delay(0.0),
    target_time(0.0),
    osc_delay(0.0),
    osc_targettime(0.0),
    current_delay(0.0),
    in_pos(0),
    out_pos(0),
    b_quit(false)
{
  pthread_mutex_init( &mutex, NULL );
  memset(delayline,0,sizeof(float)*MAXDELAY);
  add_input_port("in");
  add_output_port("out");
  set_prefix("/"+name);
  add_method("/delay","ff",delay_t::osc_set_delay,this);
  add_method("/quit","",delay_t::osc_quit,this);
}

delay_t::~delay_t()
{
  pthread_mutex_trylock( &mutex );
  pthread_mutex_unlock(  &mutex );
  pthread_mutex_destroy( &mutex );
}

int delay_t::osc_set_delay(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 2) && (types[0]=='f') && (types[1]=='f') ){
    ((delay_t*)user_data)->set_delay(argv[0]->f,argv[1]->f);
  }
  return 0;
}

int delay_t::osc_quit(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data )
    ((delay_t*)user_data)->quit();
  return 0;
}

void delay_t::set_delay(double delay, double ttime)
{
  pthread_mutex_lock( &mutex );
  osc_delay = delay*srate;
  osc_targettime = ttime*srate;
  pthread_mutex_unlock( &mutex );
}

int delay_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer)
{
  //set_preset();
  float* v_in(inBuffer[0]);
  float* v_out(outBuffer[0]);
  if( pthread_mutex_trylock( &mutex ) == 0 ){
    target_delay = osc_delay;
    target_time = osc_targettime;
    pthread_mutex_unlock( &mutex );
  }
  // main loop:
  for (jack_nframes_t i = 0; i < nframes; ++i){
    if( target_time > 0 ){
      current_delay += (target_delay-current_delay)/target_time;
      target_time -= 1.0;
    }else{
      current_delay = target_delay;
    }
    current_delay = std::min(std::max(current_delay,0.0),MAXDELAY-1.0);
    out_pos = in_pos + current_delay;
    while( out_pos >= MAXDELAY )
      out_pos -= MAXDELAY;
    delayline[in_pos] = v_in[i];
    v_out[i] = delayline[out_pos];
    if( !in_pos )
      in_pos = MAXDELAY;
    in_pos--;
  }
  if( pthread_mutex_trylock( &mutex ) == 0 ){
    osc_targettime = target_time;
    pthread_mutex_unlock( &mutex );
  }
  return 0;
}

void delay_t::run()
{
  TASCAR::osc_server_t::activate();
  jackc_t::activate();
  while( !b_quit ){
    sleep( 1 );
  }
  jackc_t::deactivate();
  TASCAR::osc_server_t::deactivate();
}

int main(int argc, char** argv)
{
  std::string name("delay");
  if( argc > 1 )
    name = argv[1];
  delay_t S(name);
  S.run();
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
