#include "jackclient.h"
#include "osc_helper.h"
#include "errorhandling.h"

static int osc_set_bool_true(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data )
    *(bool*)(user_data) = true;
  return 0;
}

static int osc_set_float(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc == 1) && (types[0] == 'f') )
    *(float*)(user_data) = argv[0]->f;
  return 0;
}

class osc2jack_t : public jackc_t, public TASCAR::osc_server_t {
public:
  osc2jack_t(const std::vector<std::string>& names);
  ~osc2jack_t();
  void run();
  int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer);
private:
  bool b_quit;
  float* v;
};

osc2jack_t::osc2jack_t(const std::vector<std::string>& names)
  : jackc_t("osc2jack"),TASCAR::osc_server_t("224.1.2.3","6978"),b_quit(false),
    v(new float[std::max(1u,(uint32_t)names.size())])
{
  add_method("/osc2jack/quit","",osc_set_bool_true,&b_quit);
  for(uint32_t k=0;k<names.size();k++){
    add_method(names[k],"f",osc_set_float,&(v[k]));
    add_output_port(names[k]);
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
  while( !b_quit )
    usleep(50000);
  TASCAR::osc_server_t::deactivate();
  jackc_t::deactivate();
}

int osc2jack_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer)
{
  for(uint32_t ch=0;ch<outBuffer.size();ch++)
    for(uint32_t k=0;k<nframes;k++)
      outBuffer[ch][k] = v[ch];
  return 0;
}


int main(int argc,char** argv)
{
  if( argc < 2 )
    throw TASCAR::ErrMsg(std::string("Usage: ")+argv[0]+" path [ path ... ]");
  std::vector<std::string> path;
  for(int k=1;k<argc;k++)
    path.push_back(argv[k]);
  osc2jack_t s(path);
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

