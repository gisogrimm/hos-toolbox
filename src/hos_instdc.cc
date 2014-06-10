#include "jackclient.h"
#include <iostream>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <math.h>
#include "libhos_audiochunks.h"
#include "osc_helper.h"

static bool b_quit;

class dc_t : public jackc_t, public TASCAR::osc_server_t {
public:
  dc_t(const std::string& jackname,
           const std::string& server_address,
           const std::string& server_port,const std::vector<std::string>& names,float c);
  int process(jack_nframes_t n,const std::vector<float*>& inBuf,const std::vector<float*>& outBuf);
private:
  float c_;
  float fcut;
  double c1;
  double c2;
  HoS::wave_t statex;
  HoS::wave_t statey;
};

dc_t::dc_t(const std::string& jackname,
           const std::string& server_address,
           const std::string& server_port,const std::vector<std::string>& names,float c)
  : jackc_t(jackname), osc_server_t(server_address,server_port), c_(c), fcut(1400.0), statex(names.size()), statey(names.size())
{
  for(uint32_t k=0;k<names.size();k++){
    add_input_port("in."+names[k]);
    add_output_port("out."+names[k]);
  }
  set_prefix("/"+jackname+"/");
  add_float("fcut",&fcut);
  add_float("c",&c_);
}

int dc_t::process(jack_nframes_t n,const std::vector<float*>& inBuf,const std::vector<float*>& outBuf)
{
  if( fcut > 0 ){
    c1 = exp( -fcut/srate);
    c2 = 1.0-c1;
  }
  for(uint32_t ch=0;ch<inBuf.size();ch++){
    for(uint32_t k=0;k<n;k++){
      float v(inBuf[ch][k]);
      v -= statex[ch];
      statex[ch] = inBuf[ch][k];
      v = v*c_/(c_+fabsf(v));
      statey[ch] *= c1;
      statey[ch] += c2*v;
      v = statey[ch];
      outBuf[ch][k] = v;
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
  std::cout << "Usage:\n\nhos_instdc [options] name1 [ name2 .... ]\n\nOptions:\n\n";
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
  std::vector<std::string> objnames;
  const char *options = "hn:c:m:p:";
  struct option long_options[] = { 
    { "help",       0, 0, 'h' },
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
  dc_t dc(jackname,serveraddr,serverport,objnames,c);
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
