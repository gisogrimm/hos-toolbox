#include "jackclient.h"
#include <iostream>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>

static bool b_quit;

class dc_t : public jackc_t {
public:
  dc_t(const std::string& jackname,const std::vector<std::string>& names,float c);
  int process(jack_nframes_t n,const std::vector<float*>& inBuf,const std::vector<float*>& outBuf);
private:
  float c_;
};

dc_t::dc_t(const std::string& jackname,const std::vector<std::string>& names,float c)
  : jackc_t(jackname), c_(c)
{
  for(uint32_t k=0;k<names.size();k++){
    add_input_port("in."+names[k]);
    add_output_port("out."+names[k]);
  }
}

int dc_t::process(jack_nframes_t n,const std::vector<float*>& inBuf,const std::vector<float*>& outBuf)
{
  for(uint32_t ch=0;ch<inBuf.size();ch++){
    for(uint32_t k=0;k<n;k++){
      float v(inBuf[ch][k]);
      if( v > 1.0f )
        v = 1.0f;
      if( v > -1.0f )
        v = -1.0f;
      outBuf[ch][k] = v*c_/(c_+v*v);
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
  std::string jackname("instdc");
  float c(0.707);
  std::vector<std::string> objnames;
  const char *options = "hn:c:";
  struct option long_options[] = { 
    { "help",       0, 0, 'h' },
    { "scale",      1, 0, 'c' },
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
  dc_t dc(jackname,objnames,c);
  dc.activate();
  while( !b_quit )
    sleep(1);
  dc.deactivate();
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
