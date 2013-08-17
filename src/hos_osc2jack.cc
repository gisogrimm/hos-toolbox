#include "jackclient.h"
#include "osc_helper.h"
#include "errorhandling.h"
#include <stdio.h>
#include <iostream>
#include <getopt.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

static bool b_quit;

class v_event_t {
public:
  v_event_t() : t(0.0), v(0.0) {};
  v_event_t(double t_, float v_) : t(t_),v(v_) {};
  double t;
  float v;
};

class looper_t {
public:
  enum mode_t { play , mute, rec, bypass };
  looper_t();
  void set_mode(mode_t m);
  float process(double t, float oscv);
  std::vector<v_event_t> loop;
  mode_t mode;
  float lastv;
  double lastt;
};

looper_t::looper_t()
  : mode(mute),
    lastv(0.0),
    lastt(0.0)
{
  loop.reserve(8192);
}

void looper_t::set_mode(mode_t m)
{
  if( (m == rec) && (mode != rec) )
    loop.clear();
  mode = m;
}

float looper_t::process(double t, float oscv)
{
  float rv(oscv);
  switch( mode ){
  case play :
    for(std::vector<v_event_t>::iterator it=loop.begin();it!=loop.end();++it){
      if( (it->t <= t) && ((lastt >= t)||(it->t > lastt)) )
        rv = it->v;
    }
    break;
  case mute :
    rv = 0.0f;
    break;
  case rec :
    loop.push_back(v_event_t(t,oscv));
    break;
  case bypass :
    break;
  }
  return rv;
}

class osc2jack_t : public jackc_t, public TASCAR::osc_server_t {
public:
  osc2jack_t(const std::string& oscad,const std::vector<std::string>& names);
  ~osc2jack_t();
  void run();
  int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer);
private:
  uint32_t npar;
  float* v;
  std::vector<looper_t> vloop;
  int32_t bar;
  lo_address lo_addr;
  float matrix[96];
};

osc2jack_t::osc2jack_t(const std::string& oscad,const std::vector<std::string>& names)
  : jackc_t("osc2jack"),TASCAR::osc_server_t("","8000",false),
    npar(names.size()),
    v(new float[std::max(1u,npar)]),
    bar(0),
    lo_addr(lo_address_new_from_url( oscad.c_str() ))
{
  add_input_port("time");
  add_output_port("matrix_1");
  add_output_port("matrix_2");
  add_output_port("matrix_3");
  add_output_port("matrix_4");
  add_output_port("matrix_5");
  add_output_port("matrix_6");
  add_method("/osc2jack/quit","",osc_set_bool_true,&b_quit);
  for(uint32_t k=0;k<names.size();k++){
    add_method(names[k],"f",osc_set_float,&(v[k]));
    add_output_port(names[k]);
  }
  vloop.resize(names.size());
  memset(matrix,0,sizeof(float)*96);
  for(uint32_t ch=0;ch<3;ch++)
    for(uint32_t t=0;t<16;t++){
      matrix[ch+6*t] = 1.0;
    }
  for(uint32_t ch=0;ch<6;ch++)
    for(uint32_t t=0;t<16;t++){
      char addr[1024];
      sprintf(addr,"/2/multitoggle/%d/%d",ch+1,t+1);
      lo_send( lo_addr, addr, "f", matrix[ch+6*t]);
      add_method(addr,"f",osc_set_float,&(matrix[ch+6*t]));
      usleep(20000);
    }
  
}

osc2jack_t::~osc2jack_t()
{
  delete [] v;
}

void osc2jack_t::run()
{
  jackc_t::activate();
  TASCAR::osc_server_t::activate();
  connect_in(0,"mha-house:time",true);
  connect_out(0,"mha-house:cv_bass",true);
  connect_out(1,"mha-house:cv_mid",true);
  connect_out(2,"mha-house:cv_high",true);
  while( !b_quit )
    usleep(50000);
  TASCAR::osc_server_t::deactivate();
  jackc_t::deactivate();
}

int osc2jack_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer)
{
  int32_t nbar(0.25*inBuffer[0][0]);
  nbar = std::min(15,std::max(0,nbar));
  if( nbar != bar ){
    char addr[1024];
    sprintf(addr,"/2/led%d",nbar+1);
    lo_send( lo_addr, addr, "f", 1.0f);
    sprintf(addr,"/2/led%d",bar+1);
    lo_send( lo_addr, addr, "f", 0.0f);
  }
  bar = nbar;
  for(uint32_t ch=0;ch<6;ch++)
    for(uint32_t k=0;k<nframes;k++)
      outBuffer[ch][k] = matrix[ch+6*nbar];
  for(uint32_t ch=0;ch<npar;ch++)
    for(uint32_t k=0;k<nframes;k++)
      outBuffer[ch+6][k] = v[ch];
  return 0;
}


static void sighandler(int sig)
{
  b_quit = true;
}

void usage(struct option * opt)
{
  std::cout << "Usage:\n\nhos_osc2jack [options] path [ path .... ]\n\nOptions:\n\n";
  while( opt->name ){
    std::cout << "  -" << (char)(opt->val) << " " << (opt->has_arg?"#":"") <<
      "\n  --" << opt->name << (opt->has_arg?"=#":"") << "\n\n";
    opt++;
  }
}

int main(int argc,char** argv)
{
  b_quit = false;
  signal(SIGABRT, &sighandler);
  signal(SIGTERM, &sighandler);
  signal(SIGINT, &sighandler);
  std::string jackname("osc2jack");
  std::string touchosc("osc.udp://192.168.178.114:9000/");
  std::vector<std::string> path;
  const char *options = "d:hn:";
  struct option long_options[] = { 
    { "dest",       1, 0, 'd' },
    { "jackname",   1, 0, 'n' },
    { "help",       0, 0, 'h' },
    { 0, 0, 0, 0 }
  };
  int opt(0);
  int option_index(0);
  while( (opt = getopt_long(argc, argv, options,
                            long_options, &option_index)) != -1){
    switch(opt){
    case 'd':
      touchosc = optarg;
      break;
    case 'h':
      usage(long_options);
      return -1;
    case 'n':
      jackname = optarg;
      break;
    }
  }
  while( optind < argc )
    path.push_back(argv[optind++]);
  if( path.empty() ){
    usage(long_options);
    return -1;
  }
  osc2jack_t s(touchosc,path);
  s.run();
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */

