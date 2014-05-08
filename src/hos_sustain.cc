/**
   \file hos_sustain.cc
   \ingroup apphos
   \brief Sustain effect
   \author Giso Grimm
   \date 2014

   \section license License (GPL)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2
   of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include "jackclient.h"
#include "osc_helper.h"
#include <stdlib.h>
#include <iostream>
#include "audiochunks.h"
#include "defs.h"
#include "libhos_random.h"
#include <math.h>

#define OSC_ADDR "224.1.2.3"
#define OSC_PORT "6978"
#define HIST_SIZE 256

class sustain_t : public TASCAR::osc_server_t, public jackc_t
{
public:
  sustain_t(const std::string& name,uint32_t wlen);
  virtual ~sustain_t();
  virtual int process(jack_nframes_t, const std::vector<float*>&, const std::vector<float*>&);
  void activate();
  void deactivate();
protected:
  uint32_t chunksize;
  uint32_t fftlen;
  uint32_t wndlen;
  HoS::ola_t ola;
  HoS::wave_t absspec;
  float lp_c;
  float env_c;
  double Lin;
  double Lout;
};


int sustain_t::process(jack_nframes_t n, const std::vector<float*>& vIn, const std::vector<float*>& vOut)
{
  HoS::wave_t in(n,vIn[0]);
  HoS::wave_t out(n,vOut[0]);
  ola.process(in);
  float c1(lp_c);
  float c2(1.0f-c1);
  float env_c1(env_c);
  float env_c2(1.0f-env_c1);
  ola.s *= c2;
  absspec *= c1;
  for(uint32_t k=0;k<ola.s.size();k++){
    absspec[k] += cabsf(ola.s[k]);
    ola.s[k] = absspec[k]*cexpf(I*drand()*PI2);
  }
  ola.ifft(out);
  // envelope reconstruction:
  for(uint32_t k=0;k<in.size();k++){
    Lin *= env_c1;
    Lin += env_c2*in[k]*in[k];
    Lout *= env_c1;
    Lout += env_c2*out[k]*out[k];
    if( Lout > 0 )
      out[k] *= sqrt(Lin/Lout);
  }
  return 0;
}

sustain_t::sustain_t(const std::string& name,uint32_t wlen)
  : osc_server_t(OSC_ADDR,OSC_PORT),
    jackc_t(name),
    chunksize(get_fragsize()),
    fftlen(std::max(wlen,2*chunksize)),
    wndlen(std::max(wlen,2*chunksize)),
    ola(fftlen,wndlen,chunksize,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_HANNING),
    absspec(ola.s.size()),
    lp_c(0.9),
    env_c(0.99999),
    Lin(0),
    Lout(0)
{
  set_prefix("/"+name);
  add_input_port("in");
  add_output_port("out");
  add_float("/c",&lp_c);
  add_float("/env",&env_c);
}

void sustain_t::activate()
{
  jackc_t::activate();
  osc_server_t::activate();
  //try{
  //  connect_in(0,"ardour:amb_1st/out 1");
  //  connect_in(1,"ardour:amb_1st/out 2");
  //  connect_in(2,"ardour:amb_1st/out 3");
  //  connect_out(0,"ambdec:in.0w");
  //  connect_out(1,"ambdec:in.1x");
  //  connect_out(2,"ambdec:in.1y");
  //}
  //catch( const std::exception& e ){
  //  std::cerr << "Warning: " << e.what() << std::endl;
  //};
}


void sustain_t::deactivate()
{
  osc_server_t::deactivate();
  jackc_t::deactivate();
}

sustain_t::~sustain_t()
{
}
  

void lo_err_handler_cb(int num, const char *msg, const char *where) 
{
  std::cerr << "lo error " << num << ": " << msg << " (" << where << ")\n";
}

int main(int argc, char** argv)
{
  sustain_t c("sustain",1<<10);
  c.activate();
  while( true )
    sleep(1);
  c.deactivate();
  return 0;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
