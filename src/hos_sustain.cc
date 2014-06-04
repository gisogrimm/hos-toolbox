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
#include "libhos_audiochunks.h"
#include "hos_defs.h"
#include "libhos_random.h"
#include "ringbuffer.h"
#include <math.h>
#include "errorhandling.h"
#include <getopt.h>
#include <signal.h>

static bool b_quit;

//class doublebuffer_t {
//public:
//  doublebuffer_t(uint32_t fifolen,uint32_t inner_size);
//  ~doublebuffer_t();
//  virtual void inner_process(HoS::wave_t &) = 0;
//  void start_service();
//  void stop_service();
//private:
//  void service();
//  static void * service(void* h);
//  //pthread_t srv_thread;
//  //bool service_running;
//  //bool run_service;
//  HoS::wave_t audio;
//protected:
//  TASCAR::ringbuffer_t in2out;
//  TASCAR::ringbuffer_t out2in;
//};
//
//doublebuffer_t::doublebuffer_t(uint32_t fifolen,uint32_t inner_size)
//  : service_running(false),
//    run_service(true),
//    audio(inner_size),
//    in2out(fifolen,1),
//    out2in(fifolen,1)
//{
//  in2out.write_zeros( 2*audio.size() );
//}
//
//
//void doublebuffer_t::start_service()
//{
//  if( !service_running){
//    run_service = true;
//    int err = pthread_create( &srv_thread, NULL, &doublebuffer_t::service, this);
//    if( err < 0 )
//      throw TASCAR::ErrMsg("pthread_create failed");
//    service_running = true;
//  }
//}
//
//void doublebuffer_t::stop_service()
//{
//  if( service_running){
//    run_service = false;
//    pthread_join( srv_thread, NULL );
//    service_running = false;
//  }
//}
//
//void * doublebuffer_t::service(void* h)
//{
//  ((doublebuffer_t*)h)->service();
//  return NULL;
//}
//
//void doublebuffer_t::service()
//{
//  while( run_service ){
//    usleep( 500 );
//    if( out2in.read_space() >= audio.size() ){
//      // enough data to process inner chunk:
//      out2in.read(audio.b,audio.size());
//      inner_process(audio);
//      if( in2out.write_space() < audio.size() ){
//        DEBUG(in2out.write_space());
//        DEBUG(audio.size());
//      }
//      in2out.write(audio.b,audio.size());
//    }
//  }
//}
//
//doublebuffer_t::~doublebuffer_t()
//{
//  stop_service();
//}

class sustain_t : public TASCAR::osc_server_t, public jackc_db_t {
public:
  sustain_t(const std::string& server_addr,const std::string& server_port,const std::string& name,uint32_t wlen);
  virtual ~sustain_t();
  virtual int inner_process(jack_nframes_t, const std::vector<float*>&, const std::vector<float*>&);
  virtual int process(jack_nframes_t, const std::vector<float*>&, const std::vector<float*>&);
  void activate();
  void deactivate();
protected:
  HoS::ola_t ola;
  HoS::wave_t absspec;
  float tau_sustain;
  float tau_envelope;
  double Lin;
  double Lout;
};

int sustain_t::process(jack_nframes_t n, const std::vector<float*>& vIn, const std::vector<float*>& vOut)
{
  jackc_db_t::process(n,vIn,vOut);
  HoS::wave_t w_in(n,vIn[0]);
  HoS::wave_t w_out(n,vOut[0]);
  float env_c1(0);
  if( tau_envelope > 0 )
    env_c1 = exp( -1.0/(tau_envelope*(double)srate));
  float env_c2(1.0f-env_c1);
  // envelope reconstruction:
  for(uint32_t k=0;k<w_in.size();k++){
    Lin *= env_c1;
    Lin += env_c2*w_in[k]*w_in[k];
    Lout *= env_c1;
    Lout += env_c2*w_out[k]*w_out[k];
    if( Lout > 0 )
      w_out[k] *= sqrt(Lin/Lout);
  }
  return 0;
}


int sustain_t::inner_process(jack_nframes_t n, const std::vector<float*>& vIn, const std::vector<float*>& vOut)
{
  HoS::wave_t w_in(n,vIn[0]);
  HoS::wave_t w_out(n,vOut[0]);
  ola.process(w_in);
  float sus_c1(0);
  if( tau_sustain > 0 )
    sus_c1 = exp( -1.0/(tau_sustain*(double)srate/(double)(w_in.size())));
  float sus_c2(1.0f-sus_c1);
  ola.s *= sus_c2;
  absspec *= sus_c1;
  for(uint32_t k=0;k<ola.s.size();k++){
    absspec[k] += cabsf(ola.s[k]);
    ola.s[k] = absspec[k]*cexpf(I*drand()*PI2);
  }
  ola.ifft(w_out);
  return 0;
}

sustain_t::sustain_t(const std::string& server_addr,const std::string& server_port,const std::string& name,uint32_t wlen)
  : osc_server_t(server_addr,server_port),
    jackc_db_t(name,wlen),
    //doublebuffer_t(4*wlen,wlen),
    ola(2*wlen,2*wlen,wlen,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_RECT,HoS::stft_t::WND_SQRTHANN),
    absspec(ola.s.size()),
    tau_sustain(20),
    tau_envelope(1),
    Lin(0),
    Lout(0)
{
  set_prefix("/"+name);
  add_input_port("in");
  add_output_port("out");
  add_float("/tau_sus",&tau_sustain);
  add_float("/tau_env",&tau_envelope);
}

void sustain_t::activate()
{
  jackc_db_t::activate();
  osc_server_t::activate();
  //try{
  //  connect_in(0,"Rhythmbox:out_autoaudiosink0-actual-sink-jackaudio_1");
  //  connect_out(0,"system:playback_1");
  //}
  //catch( const std::exception& e ){
  //  std::cerr << "Warning: " << e.what() << std::endl;
  //};
}


void sustain_t::deactivate()
{
  //doublebuffer_t::stop_service();
  osc_server_t::deactivate();
  jackc_db_t::deactivate();
}

sustain_t::~sustain_t()
{
}
  

void lo_err_handler_cb(int num, const char *msg, const char *where) 
{
  std::cerr << "lo error " << num << ": " << msg << " (" << where << ")\n";
}

static void sighandler(int sig)
{
  b_quit = true;
}

void usage(struct option * opt)
{
  std::cout << "Usage:\n\nhos_sustain [options] path [ path .... ]\n\nOptions:\n\n";
  while( opt->name ){
    std::cout << "  -" << (char)(opt->val) << " " << (opt->has_arg?"#":"") <<
      "\n  --" << opt->name << (opt->has_arg?"=#":"") << "\n\n";
    opt++;
  }
}

int main(int argc, char** argv)
{
  b_quit = false;
  signal(SIGABRT, &sighandler);
  signal(SIGTERM, &sighandler);
  signal(SIGINT, &sighandler);
  std::string jackname("sustain");
  std::string serverport("6978");
  std::string serveraddr("239.255.1.7");
  uint32_t wlen(8192);
  const char *options = "hn:p:m:w:";
  struct option long_options[] = { 
    { "help",       0, 0, 'h' },
    { "multicast",  1, 0, 'm' },
    { "port",       1, 0, 'p' },
    { "jackname",   1, 0, 'n' },
    { "wlen",       1, 0, 'w' },
    { 0, 0, 0, 0 }
  };
  int opt(0);
  int option_index(0);
  while( (opt = getopt_long(argc, argv, options,
                            long_options, &option_index)) != -1){
    switch(opt){
    case 'p':
      serverport = optarg;
      break;
    case 'm':
      serveraddr = optarg;
      break;
    case 'w':
      wlen = atoi(optarg);
      break;
    case 'h':
      usage(long_options);
      return -1;
    case 'n':
      jackname = optarg;
      break;
    }
  }
  sustain_t c(serveraddr,serverport,jackname,wlen);
  c.activate();
  while( !b_quit )
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
