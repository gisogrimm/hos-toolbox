#include "jackclient.h"
#include "osc_helper.h"
#include "errorhandling.h"

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
  osc2jack_t(const std::vector<std::string>& names);
  ~osc2jack_t();
  void run();
  int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer);
private:
  bool b_quit;
  float* v;
  std::vector<looper_t> vloop;
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
  vloop.resize(names.size());
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

