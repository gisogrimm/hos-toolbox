#include "jackclient.h"
#include <iostream>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <math.h>
#include "libhos_audiochunks.h"
#include "osc_helper.h"
#include <unistd.h>
#include <complex.h>

static bool b_quit;

class dc_t : public jackc_t, public TASCAR::osc_server_t {
public:
  dc_t(const std::string& jackname,
       const std::string& server_address,
       const std::string& server_port,const std::vector<std::string>& names,float c_,float fc,float q_);
  int process(jack_nframes_t n,const std::vector<float*>& inBuf,const std::vector<float*>& outBuf);
private:
  float c;
  float f;
  float q;
  double b1;
  double b2;
  HoS::wave_t statex;
  HoS::wave_t statey1;
  HoS::wave_t statey2;
};

dc_t::dc_t(const std::string& jackname,
           const std::string& server_address,
           const std::string& server_port,const std::vector<std::string>& names,float c_,float fc,float q_)
  : jackc_t(jackname), 
    osc_server_t(server_address,server_port), 
    c(c_), 
    f(fc), 
    q(q_),
    statex(names.size()),
    statey1(names.size()),
    statey2(names.size())
{
  for(uint32_t k=0;k<names.size();k++){
    add_input_port("in."+names[k]);
    add_output_port("out."+names[k]);
  }
  set_prefix("/"+jackname+"/");
  add_float("f",&f);
  add_float("c",&c);
  add_float("q",&q);
}

int dc_t::process(jack_nframes_t n,const std::vector<float*>& inBuf,const std::vector<float*>& outBuf)
{
  b1 = 2.0*q*cos(2.0*M_PI*f/srate);
  b2 = -q*q;
  float complex z(cexpf(I*2*M_PI*f/srate));
  float complex z0(q*cexpf(-I*2*M_PI*f/srate));
  float a1((1.0f-q)*(cabsf(z-z0))*srate/(2.0*M_PI*f));
  //if( fcut > 0 ){
  //  c1 = exp( -fcut/srate);
  //  c2 = 1.0-c1;
  //}
  for(uint32_t ch=0;ch<inBuf.size();ch++){
    float* vx(inBuf[ch]);
    float* vy(outBuf[ch]);
    for(uint32_t k=0;k<n;k++){
      // input resonance filter:
      float y(a1*vx[k]+b1*statey1[ch]+b2*statey2[ch]);
      statey2[ch] = statey1[ch];
      statey1[ch] = y;
      // non-linearity:
      y = y*c/(c+fabsf(y));
      // air coupling to velocity:
      statex[ch] = y - statex[ch];
      vy[k] = statex[ch];
    }
  }
  return 0;
}

static void sighandler(int sig)
{
  b_quit = true;
}

void usage(struct option * opt)
{
  std::cout << "Usage:\n\nhos_spksim [options] name1 [ name2 .... ]\n\nOptions:\n\n";
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
  std::string serverport("7655");
  //std::string serveraddr("239.255.1.7");
  std::string serveraddr("");
  std::string jackname("instdc");
  float c(0.5);
  float fres(1200);
  float q(0.8);
  std::vector<std::string> objnames;
  const char *options = "hn:c:m:p:f:q:";
  struct option long_options[] = { 
    { "help",       0, 0, 'h' },
    { "fres",       1, 0, 'f' },
    { "q",          1, 0, 'q' },
    { "scale",      1, 0, 'c' },
    { "multicast",  1, 0, 'm' },
    { "port",       1, 0, 'p' },
    { "jackname",   1, 0, 'n' },
    { 0, 0, 0, 0 }
  };
  int opt(0);
  int option_index(0);
  while( (opt = getopt_long(argc, argv, options,
                            long_options, &option_index)) != -1){
    switch(opt){
    case 'c':
      c = atof(optarg);
      break;
    case 'f':
      fres = atof(optarg);
      break;
    case 'q':
      q = atof(optarg);
      break;
    case 'h':
      usage(long_options);
      return -1;
    case 'p':
      serverport = optarg;
      break;
    case 'm':
      serveraddr = optarg;
      break;
    case 'n':
      jackname = optarg;
      break;
    }
  }
  while( optind < argc )
    objnames.push_back(argv[optind++]);
  if( objnames.empty() ){
    objnames.push_back("0");
  }
  dc_t dc(jackname,serveraddr,serverport,objnames,c,fres,q);
  dc.jackc_t::activate();
  dc.osc_server_t::activate();
  while( !b_quit )
    sleep(1);
  dc.jackc_t::deactivate();
  dc.osc_server_t::deactivate();
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
