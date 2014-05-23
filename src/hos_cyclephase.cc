/**
   \file hos_cyclephase.cc
   \ingroup apphos
   \brief Phase tracker

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
#include <complex.h>
#include <math.h>
#include "hos_defs.h"

#define OSC_ADDR "239.255.1.7"
#define OSC_PORT "6978"

#define PI2 6.283185307179586232
#define PI2INV 0.15915494309189534561

namespace HoS {

  inline double c2normphase(double complex p)
  {
    double rp(atan2(cimag(p),creal(p)));
    if( rp < 0 )
      rp += PI2;
    return PI2INV*rp;
  }

  class clp_t {
  public:
    clp_t();
    void set_tau(double tau);
    inline double complex filter(double complex val){
      state *= c1;
      return (state += c2*val);
    }
  private:
    double complex state;
    double c1;
    double c2;
  };

  class maxtrack_t {
  public:
    maxtrack_t(double fs, double tau, double tau2);
    inline bool filter(double val){
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
    double state;
    bool emit;
  private:
    bool was_rising;
    double c1, c2;
    double c3, c4;
    double state2;
    uint32_t timeout;
    uint32_t cnt;
  };

  class drift_filter_t {
  public:
    drift_filter_t();
    inline double filter(double x){
      y1 = c*y1 + c1*(x-xn1);
      y2 = y2+y1;
      y3 = y2+c2*(xn1-y3);
      xn1 = x;
      return y3;
    };
  private:
    double xn1;
    double y1;
    double y2;
    double y3;
    double c;
    double c1;
    double c2;
  };

  class unwrapper_t {
  public:
    unwrapper_t() : delta(0.0),cnt(0.0) {};
    void set(double c) { cnt = c; };
    inline double operator()(double x){
      delta -= x;
      if( delta < -0.5 )
        cnt -= 1.0;
      if( delta > 0.5 )
        cnt += 1.0;
      delta = x;
      return cnt+x;
    };
  private:
    double delta;
    double cnt;
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
    bool b_quit;
    std::vector<maxtrack_t> mt;
    std::vector<uint32_t> phase_i;
    std::vector<double> p0;
    double complex cphase_raw;
    double complex cphase_lp;
    double complex cphase_lpdrift;
    double complex cphase_if;
    double complex cdrift_raw;
    double complex cdrift_lp;
    double complex cdphase;
    double complex cdphase_lp;
    unwrapper_t unwrap;
    clp_t lp_phase;
    clp_t lp_drift;
    clp_t lp_if;
    uint32_t loop;
  };

}

using namespace HoS;

clp_t::clp_t()
  : state(0.0),
    c1(0.0),
    c2(1.0)
{
}

void clp_t::set_tau(double tau)
{
  c1 = exp( -1.0/tau);
  c2 = 1.0 - c1;
}

maxtrack_t::maxtrack_t(double fs,double tau, double tau2)
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

drift_filter_t::drift_filter_t()
  : xn1(0),
    y1(0),
    y2(0),
    y3(0),
    c(0.99),
    c1(1.0-c),
    c2(0.001)
{
}

cyclephase_t::cyclephase_t(const std::string& name)
  : jackc_t(name),
    TASCAR::osc_server_t(OSC_ADDR,OSC_PORT),
    b_quit(false),
    mt(std::vector<maxtrack_t>(4,maxtrack_t(srate,0.3,8.0))),
    phase_i(std::vector<uint32_t>(4,0)),
    cphase_raw(0.0),
    cphase_lp(0.0),
    cphase_if(1.0),
    cdrift_raw(0.0),
    cdrift_lp(0.0),
    loop(0)
{
  add_input_port("L1");
  add_input_port("L2");
  add_input_port("L3");
  add_input_port("L4");
  add_output_port("phase");
  add_output_port("time");
  set_prefix("/"+name);
  add_method("/t0","f",cyclephase_t::osc_set_t0,this);
  add_method("/loop","i",osc_set_int32,&loop);
  add_method("/quit","",cyclephase_t::osc_quit,this);
  p0.resize(4);
  p0[0] = 0.0;
  p0[1] = 10.2/36.0;
  p0[2] = 18.4/36.0;
  p0[3] = 27.5/36.0;
  lp_phase.set_tau(0.25*srate);
  lp_if.set_tau(4.0*srate);
  lp_drift.set_tau(4.0*srate);
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
  unwrap.set(t0-1.0);
}

int cyclephase_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer)
{
  float* v_phase_lp(outBuffer[0]);
  float* v_time(outBuffer[1]);
  // main loop:
  for (jack_nframes_t i = 0; i < nframes; ++i){
    for( uint32_t ch=0;ch<4;ch++){
      phase_i[ch]++;
      if( mt[ch].filter(inBuffer[ch][i])){
        cphase_raw = cexp(I*PI2*p0[ch]);
        cdrift_raw = cphase_raw * conj(cphase_if);
        lp_phase.set_tau(std::min(2.0*srate,0.5*(double)(phase_i[ch])));
        lp_if.set_tau(std::min(2.0*srate,0.5*(double)(phase_i[ch])));
        lp_drift.set_tau(std::min(2.0*srate,1.0*(double)(phase_i[ch])));
        phase_i[ch] = 0;
      }
    }
    cdrift_lp = lp_drift.filter(cdrift_raw);
    cdphase = conj(cphase_lp);
    cphase_lp = lp_phase.filter(cphase_raw);
    cdphase *= cphase_lp;
    cdphase_lp = lp_if.filter(cdphase);
    cphase_if *= cexp(I*carg(cdphase_lp));
    cphase_lpdrift = cphase_if*cdrift_lp;
    double current_phase(c2normphase(cphase_lpdrift));
    v_phase_lp[i] = current_phase;
    double current_time(unwrap(current_phase));
    if( (loop > 0) && (current_time >= loop) ){
      unwrap.set(0.0);
      current_time = 0;
    }
    v_time[i] = current_time;
  }
  cphase_if /= cabs(cphase_if);
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
  connect_out(0,"hos_scope:in_1",true);
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
