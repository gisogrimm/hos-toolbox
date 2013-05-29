#include "jackclient.h"
#include "audiochunks.h"
#include "osc_helper.h"
#include "errorhandling.h"
#include <string.h>
#include <fstream>
#include <iostream>
#include "defs.h"
#include <math.h>

using namespace HoS;

class note_event_t {
public:
  note_event_t(uint32_t note,double time,float gain);
  note_event_t();
  uint32_t note_;
  double time_;
  float gain_;
};

note_event_t::note_event_t(uint32_t note,double time,float gain)
  : note_(note),
    time_(time),
    gain_(gain)
{
}

note_event_t::note_event_t()
  : note_(0),
    time_(0),
    gain_(1)
{
}

class sampler_t : public jackc_t, public TASCAR::osc_server_t {
public:
  sampler_t(const std::string& jname= "sampler");
  ~sampler_t();
  void run();
  int process(jack_nframes_t n, const std::vector<float*>& sIn, const std::vector<float*>& sOut);
  void open_sounds(const std::string& fname);
  void open_notes(const std::string& fname);
  void set_t0(double t0);
  void quit() { b_quit = true;};
  static int osc_set_t0(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int osc_quit(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
private:
  std::vector<sndfile_t*> sounds;
  std::vector<note_event_t> notes;
  double current_time;
  double last_phase;
  bool b_quit;
};

sampler_t::sampler_t(const std::string& jname)
  : jackc_t(jname),
    osc_server_t("224.1.2.3","6978"),
    current_time(0),
    last_phase(0),
    b_quit(false)
{
  add_input_port("phase");
  add_output_port("out");
  set_prefix("/"+jname);
  add_method("/t0","f",sampler_t::osc_set_t0,this);
  add_method("/quit","",sampler_t::osc_quit,this);
}

int sampler_t::osc_set_t0(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 1) && (types[0]=='f') ){
    ((sampler_t*)user_data)->set_t0(argv[0]->f);
  }
  return 0;
}

int sampler_t::osc_quit(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data )
    ((sampler_t*)user_data)->quit();
  return 0;
}

void sampler_t::set_t0(double t0)
{
  current_time = t0;
}

sampler_t::~sampler_t()
{
  for( unsigned int k=0;k<sounds.size();k++)
    delete sounds[k];
}

int sampler_t::process(jack_nframes_t n, const std::vector<float*>& sIn, const std::vector<float*>& sOut)
{
  memset(sOut[0],0,n*sizeof(float));
  float* vPhase(sIn[0]);
  double dt(0.0);
  double t0(vPhase[0]);
  uint32_t Nphase(0);
  double lct(current_time);
  for( uint32_t k=0;k<n;k++){
    double dphase(vPhase[k]-last_phase);
    if( dphase < 0 )
      current_time += 1.0;
    else{
      dt += dphase;
      Nphase++;
    }
    last_phase = vPhase[k];
  }
  dt /= (double)Nphase;
  if( dt <= 0)
    return 0;
  t0 += lct;
  wave_t out(n,sOut[0]);
  double samples_per_period(1.0/dt);
  int32_t t0block(round(vPhase[0]*samples_per_period));
  int32_t chunk_time(round(lct*samples_per_period));
  chunk_time += t0block;
  //std::cerr << chunk_time << ",..." << std::endl;
  //  DEBUG(chunk_time);
  for(unsigned int k=0;k<notes.size();k++){
    if( notes[k].note_ < sounds.size() ){
      sounds[notes[k].note_]->add_chunk(chunk_time,notes[k].time_*samples_per_period,notes[k].gain_,out);
    }
  }
  return 0;
}

void sampler_t::open_sounds(const std::string& fname)
{
  std::ifstream fh(fname.c_str());
  while(!fh.eof() ){
    char ctmp[1024];
    memset(ctmp,0,1024);
    fh.getline(ctmp,1023);
    std::string fname(ctmp);
    if(fname.size()){
      sndfile_t* sf(new sndfile_t(fname));
      sounds.push_back(sf);
    }
  }
}

void sampler_t::open_notes(const std::string& fname)
{
  std::ifstream fh(fname.c_str());
  while(!fh.eof() ){
    char ctmp[1024];
    memset(ctmp,0,1024);
    fh.getline(ctmp,1023);
    note_event_t n;
    if( sscanf(ctmp,"%lf %d %f",&(n.time_),&(n.note_),&(n.gain_))>=2 )
      notes.push_back(n);
  }
}

void sampler_t::run()
{
  jackc_t::activate();
  TASCAR::osc_server_t::activate();
  try{
    connect_in(0,"phase:phase");
  }
  catch( const std::exception& e ){
    std::cerr << "Warning: " << e.what() << std::endl;
  };
  while( !b_quit ){
    sleep(1);
  }
  TASCAR::osc_server_t::deactivate();
  jackc_t::deactivate();
}

int main(int argc,char** argv)
{
  if( argc < 3 )
    throw TASCAR::ErrMsg(std::string("Usage: ")+argv[0]+" soundfont notesfile");
  sampler_t s;
  s.open_sounds(argv[1]);
  s.open_notes(argv[2]);
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

