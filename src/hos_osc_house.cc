#include "jackclient.h"
#include "osc_helper.h"
#include "errorhandling.h"
#include <stdio.h>
#include <iostream>
#include <getopt.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <fstream>
#include "hos_defs.h"
#include <math.h>
#include <unistd.h>


static bool b_quit;

class matrix_t {
public:
  matrix_t();
  matrix_t(const matrix_t& src);
  void load(const std::string& fname);
  inline void set(uint32_t t,uint32_t ch, float v){ m[ch*16+t] = v;};
  inline float get(uint32_t t,uint32_t ch){ return m[ch*16+t];};
  float m[96];
};

matrix_t::matrix_t()
{
  memset(m,0,96*sizeof(float));
}

matrix_t::matrix_t(const matrix_t& src)
{
  for(uint32_t k=0;k<96;k++)
    m[k] = src.m[k];
}

void matrix_t::load(const std::string& fname)
{
  std::ifstream fh(fname.c_str());
  uint32_t t(0);
  while(!fh.eof() ){
    char ctmp[1024];
    memset(ctmp,0,1024);
    fh.getline(ctmp,1023);
    float d[6];
    memset(d,0,6*sizeof(float));
    if( sscanf(ctmp,"%f %f %f %f %f %f",&(d[0]),&(d[1]),&(d[2]),&(d[3]),&(d[4]),&(d[5]))==6 ){
      if( t < 16 ){
        for(uint32_t ch=0;ch<6;ch++)
          set(t,ch,d[ch]);
        t++;
      }
    }
  }
  for(uint32_t k=t;k<16;k++)
    for(uint32_t ch=0;ch<6;ch++)
      set(k,ch,get(k-t,ch));
}

class osc_house_t : public jackc_t, public TASCAR::osc_server_t {
public:
  osc_house_t(const std::vector<std::string>& names,const std::string& multicast,const std::string& serverport,const std::string& jackname);
  ~osc_house_t();
  void run();
  int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer);
  static int osc_reload(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int osc_select(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int osc_set_gain0(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int osc_set_gain1(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int osc_set_gain2(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int osc_set_gain3(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int osc_set_gain4(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int osc_set_gain5(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  void set_gain(float newgain,double duration,uint32_t k) { dgain[k] = (newgain-gain[k])/(duration*srate);tfader[k] = srate*duration;};
  void reload();
  void select(uint32_t p);
private:
  int32_t bar;
  int32_t bar_shifted;
  float current[6];
  std::vector<matrix_t> presets;
  uint32_t preset;
  int32_t offset;
  std::vector<std::string> fnames;
  float v_f1[512];
  float v_q1[512];
  float v_f2[512];
  float v_q2[512];
  double gain[6];
  double dgain[6];
  uint32_t tfader[6];
};

int osc_house_t::osc_set_gain0(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 2) && (types[0]=='f') && (types[1]=='f') ){
    ((osc_house_t*)user_data)->set_gain(argv[0]->f,argv[1]->f,0);
  }
  return 0;
}

int osc_house_t::osc_set_gain1(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 2) && (types[0]=='f') && (types[1]=='f') ){
    ((osc_house_t*)user_data)->set_gain(argv[0]->f,argv[1]->f,1);
  }
  return 0;
}

int osc_house_t::osc_set_gain2(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 2) && (types[0]=='f') && (types[1]=='f') ){
    ((osc_house_t*)user_data)->set_gain(argv[0]->f,argv[1]->f,2);
  }
  return 0;
}

int osc_house_t::osc_set_gain3(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 2) && (types[0]=='f') && (types[1]=='f') ){
    ((osc_house_t*)user_data)->set_gain(argv[0]->f,argv[1]->f,3);
  }
  return 0;
}

int osc_house_t::osc_set_gain4(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 2) && (types[0]=='f') && (types[1]=='f') ){
    ((osc_house_t*)user_data)->set_gain(argv[0]->f,argv[1]->f,4);
  }
  return 0;
}

int osc_house_t::osc_set_gain5(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 2) && (types[0]=='f') && (types[1]=='f') ){
    ((osc_house_t*)user_data)->set_gain(argv[0]->f,argv[1]->f,5);
  }
  return 0;
}


void osc_house_t::select(uint32_t p)
{
  if( p < presets.size() ){
    offset = bar+1;
    preset = p;
  }
}

void osc_house_t::reload()
{
  for(uint32_t k=0;k<presets.size();k++)
    presets[k].load(fnames[k]);
}

osc_house_t::osc_house_t(const std::vector<std::string>& names,const std::string& multicast,const std::string& serverport,const std::string& jackname)
  : jackc_t(jackname),
    TASCAR::osc_server_t(multicast,serverport),
    bar(0),
    bar_shifted(0),
    preset(0),
    offset(0),
    fnames(names)
{
  memset(current,0,6*sizeof(float));
  presets.resize(fnames.size());
  reload();
  for(uint32_t k=0;k<512;k++){
    v_f1[k] = 0.5+0.5*cos(PI2*k/512.0);
    v_q1[k] = 0.5+0.4*sin(PI2*k/512.0);
    v_f2[k] = v_f1[k];
    v_q2[k] = v_q1[k];
  }
  for(uint32_t k=0;k<6;k++){
    gain[k] = 1.0;
    dgain[k] = 0.0;
    tfader[k] = 0;
  }
  std::string prefix("/"+jackname+"/");
  add_input_port("time");
  add_output_port("cv_bass");
  add_output_port("cv_mid");
  add_output_port("cv_high");
  add_output_port("cv_echo");
  add_output_port("cv_detone");
  add_output_port("f1");
  add_output_port("q1");
  add_output_port("f2");
  add_output_port("q2");
  add_method(prefix+"quit","",osc_set_bool_true,&b_quit);
  add_method(prefix+"reload","",osc_reload,this);
  add_method(prefix+"select","i",osc_select,this);
  add_method(prefix+"gain1","f",osc_set_double,&(gain[0]));
  add_method(prefix+"gain1","ff",osc_house_t::osc_set_gain0,this);
  add_method(prefix+"gain2","f",osc_set_double,&(gain[1]));
  add_method(prefix+"gain2","ff",osc_house_t::osc_set_gain1,this);
  add_method(prefix+"gain3","f",osc_set_double,&(gain[2]));
  add_method(prefix+"gain3","ff",osc_house_t::osc_set_gain2,this);
  add_method(prefix+"gain4","f",osc_set_double,&(gain[3]));
  add_method(prefix+"gain4","ff",osc_house_t::osc_set_gain3,this);
  add_method(prefix+"gain5","f",osc_set_double,&(gain[4]));
  add_method(prefix+"gain5","ff",osc_house_t::osc_set_gain4,this);
  add_method(prefix+"gain6","f",osc_set_double,&(gain[5]));
  add_method(prefix+"gain6","ff",osc_house_t::osc_set_gain5,this);
}

int osc_house_t::osc_reload(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data )
    ((osc_house_t*)user_data)->reload();
  return 0;
}

int osc_house_t::osc_select(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc == 1) && (types[0]=='i'))
    ((osc_house_t*)user_data)->select(argv[0]->i);
  return 0;
}

osc_house_t::~osc_house_t()
{
}

void osc_house_t::run()
{
  jackc_t::activate();
  TASCAR::osc_server_t::activate();
  connect_in(0,"mha-house:time",true);
  connect_out(0, "mha-house:cv_bass",true);
  connect_out(1, "mha-house:cv_mid",true);
  connect_out(2, "mha-house:cv_high",true);
  connect_out(3, "mha-sigproc:cv_delay", true);
  connect_out(4, "mha-sigproc:cv_detone", true);
  connect_out(5, "resflt1:f",true);
  connect_out(6, "resflt1:q",true);
  connect_out(7, "resflt2:f",true);
  connect_out(8, "resflt2:q",true);
  while( !b_quit )
    usleep(50000);
  TASCAR::osc_server_t::deactivate();
  jackc_t::deactivate();
}

int osc_house_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer)
{
  for(uint32_t k=0;k<nframes;k++){
    for(uint32_t ch=0;ch<6;ch++)
      if( tfader[ch] ){
        gain[ch] += dgain[ch];
        tfader[ch]--;
      }
    float time(0.25*inBuffer[0][k]);
    int32_t nbar(time);
    float phase(time-nbar);
    uint32_t iphase(std::max(0.0f,std::min(511.0f,phase*512.0f)));
    nbar = std::min(15,std::max(0,nbar));
    if( nbar != bar ){
      bar = nbar;
      bar_shifted = (bar-offset+16) % 16;
      if( preset < presets.size() )
        for(uint32_t ch=0;ch<6;ch++)
          current[ch] = presets[preset].get(bar_shifted,ch);
      std::cerr << current[0] << " " << current[1] << " " << current[2] << " " << current[3] << " " << current[4] << " " << current[5]
                << " bar: " << bar << "/" << bar_shifted << " preset: " << preset << "\n";
    }
    // ("cv_bass");
    // ("cv_mid");
    // ("cv_high");
    // ("cv_echo");
    // ("cv_detone");
    // ("f1");
    // ("q1");
    // ("f2");
    // ("q2");
    for(uint32_t ch=0;ch<5;ch++)
      outBuffer[ch][k] = current[ch]*gain[ch];
    outBuffer[5][k] = v_f1[iphase]*current[5];
    outBuffer[6][k] = v_q1[iphase]*current[5]*gain[5];
    outBuffer[7][k] = v_f2[iphase]*current[5];
    outBuffer[8][k] = v_q2[iphase]*current[5]*gain[5];
  }
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
  std::string serverport("6978");
  std::string serveraddr("239.255.1.7");
  std::vector<std::string> path;
  const char *options = "hn:p:m:";
  struct option long_options[] = { 
    { "help",       0, 0, 'h' },
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
    case 'p':
      serverport = optarg;
      break;
    case 'm':
      serveraddr = optarg;
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
  //if( path.empty() ){
  //  usage(long_options);
  //  return -1;
  //}
  osc_house_t s(path,serveraddr,serverport,jackname);
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

