/**
   \file hos_cyclephase.cc
   \ingroup apphos
   \brief Phase generator (planned to be optional wheel sensor processor)

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
#include <math.h>
#include "defs.h"

#define MAXDELAY 480000
#define OSC_ADDR "224.1.2.3"
#define OSC_PORT "6978"

namespace HoS {

  class maxtrack_t {
  public:
    maxtrack_t(float fs, float tau, float tau2);
    inline bool filter(float val){
      if( val >= state2 ){
        state2 = val;
      }else{
        state2 *= c3;
        state2 += c4*val;
      }
      if( val >= state ){
        was_rising = true;
        state = val;
        emit = false;
      }else{
        state *= c1;
        state += c2*val;
        emit = was_rising && (cnt==0) && (state >= 0.5*state2) && (state > 2e-4);
        was_rising = false;
        if( emit )
          cnt = timeout;
      }
      if( cnt )
        cnt--;
      return emit;
    };
    float state;
    bool emit;
  private:
    bool was_rising;
    float c1, c2;
    float c3, c4;
    float state2;
    uint32_t timeout;
    uint32_t cnt;
  };

  /**
     \ingroup apphos
  */
  class cyclephase_t : public jackc_t, public TASCAR::osc_server_t {
  public:
    cyclephase_t(const std::string& name);
    ~cyclephase_t();
    void set_t0(double t0);
    void run();
    void quit() { b_quit = true;};
    static int osc_set_t0(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    static int osc_quit(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  private:
    int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer);
    double dt;
    double t;
    bool b_quit;
    std::vector<maxtrack_t> mt;
    uint32_t phase_i;
    uint32_t last_phase_i;
    double bpm;
    double pps;
    double p0, p1, p2, p3;
  };

}

using namespace HoS;

maxtrack_t::maxtrack_t(float fs,float tau, float tau2)
  : state(0.0f),
    emit(false),
    was_rising(false),
    c1(expf( -1.0f/(tau*fs))),
    c2(1.0f-c1),
    c3(expf( -1.0f/(tau2*fs))),
    c4(1.0f-c3),
    state2(0.0f),
    timeout(fs*tau),
    cnt(0)
{
}

cyclephase_t::cyclephase_t(const std::string& name)
  : jackc_t(name),
    TASCAR::osc_server_t(OSC_ADDR,OSC_PORT),
    dt(1.0/srate),
    t(0.0),
    b_quit(false),
    mt(std::vector<maxtrack_t>(4,maxtrack_t(srate,0.3,2.2))),
    phase_i(0),
    last_phase_i(0),
    bpm(1.0),
    pps(1.0),
    p0(0.0),p1(10.0/36.0),p2(18.0/36.0),p3(28.0/36.0)
{
  add_input_port("L1");
  add_input_port("L2");
  add_input_port("L3");
  add_input_port("L4");
  add_output_port("phase");
  set_prefix("/"+name);
  add_method("/t0","f",cyclephase_t::osc_set_t0,this);
  add_method("/quit","",cyclephase_t::osc_quit,this);
}

cyclephase_t::~cyclephase_t()
{
}

int cyclephase_t::osc_set_t0(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 1) && (types[0]=='f') ){
    ((cyclephase_t*)user_data)->set_t0(argv[0]->f);
  }
  return 0;
}

int cyclephase_t::osc_quit(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data )
    ((cyclephase_t*)user_data)->quit();
  return 0;
}

void cyclephase_t::set_t0(double t0)
{
  dt = 1.0/(t0*srate);
}

int cyclephase_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer)
{
  float* v_out(outBuffer[0]);
  // main loop:
  for (jack_nframes_t i = 0; i < nframes; ++i){
    phase_i++;
    t += dt;
    if( t > 1.0 )
      t = 0.0;
    v_out[i] = t;
    for( uint32_t ch=0;ch<4;ch++){
      if( mt[ch].filter(inBuffer[ch][i])){
        if( ch==0){
          last_phase_i = phase_i;
          pps = 1.0/(double)phase_i;
          bpm = srate*pps*60.0*4.0;
          phase_i = 0;
        }
        std::cout << ch << "  " << mt[ch].state << " " << phase_i << " " << (double)phase_i/(double)last_phase_i << " " << bpm << " " << mt[ch].state/pps << std::endl;
      }
    }
  }
  return 0;
}

void cyclephase_t::run()
{
  TASCAR::osc_server_t::activate();
  jackc_t::activate();
  connect_in(0,"system:capture_17");
  connect_in(1,"system:capture_18");
  connect_in(2,"system:capture_19");
  connect_in(3,"system:capture_20");
  while( !b_quit ){
    sleep( 1 );
  }
  jackc_t::deactivate();
  TASCAR::osc_server_t::deactivate();
}

int main(int argc, char** argv)
{
  std::string name("phase");
  if( argc > 1 )
    name = argv[1];
  cyclephase_t S(name);
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
