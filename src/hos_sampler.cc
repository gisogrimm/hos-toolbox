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

// loop event
// /sampler/sound/add loopcnt gain
// /sampler/sound/clear
// /sampler/sound/stop

class loop_event_t {
public:
  loop_event_t() : tsample(0),tloop(1),loopgain(1.0){};
  loop_event_t(int32_t cnt,float gain) : tsample(0),tloop(cnt),loopgain(gain){};
  bool valid() const { return tsample || tloop;};
  inline void process(wave_t& out_chunk,const wave_t& in_chunk){
    uint32_t n_(in_chunk.size());
    for( uint32_t k=0;k<out_chunk.size();k++){
      if( tloop == -2 ){
        tsample = 0;
        tloop = 0;
      }
      if( tsample )
        tsample--;
      else{
        if( tloop ){
          if( tloop > 0 )
            tloop--;
          tsample = n_ - 1;
        }
      }
      if( tsample || tloop )
        out_chunk[k] += loopgain*in_chunk[n_-1 - tsample];
    }
  };
  uint32_t tsample;
  int32_t tloop;
  float loopgain;
};

class looped_sndfile_t : public sndfile_t {
public:
  looped_sndfile_t(const std::string& fname,uint32_t channel);
  ~looped_sndfile_t();
  void add(loop_event_t);
  void clear();
  void stop();
  void loop(wave_t& chunk);
private:
  pthread_mutex_t mutex;
  std::vector<loop_event_t> loop_event;
};

looped_sndfile_t::looped_sndfile_t(const std::string& fname,uint32_t channel)
  : sndfile_t(fname,channel)
{
  loop_event.reserve(1024);
  pthread_mutex_init( &mutex, NULL );
}

looped_sndfile_t::~looped_sndfile_t()
{
  pthread_mutex_trylock( &mutex );
  pthread_mutex_unlock(  &mutex );
  pthread_mutex_destroy( &mutex );
}

void looped_sndfile_t::loop(wave_t& chunk)
{
  pthread_mutex_lock( &mutex );
  uint32_t kle(loop_event.size());
  while(kle){
    kle--;
    if( loop_event[kle].valid() ){
      loop_event[kle].process(chunk,*this);
    }else{
      loop_event.erase(loop_event.begin()+kle);
    }
  }
  pthread_mutex_unlock( &mutex );
}

void looped_sndfile_t::add(loop_event_t le)
{
  pthread_mutex_lock( &mutex );
  loop_event.push_back(le);
  pthread_mutex_unlock( &mutex );
}

void looped_sndfile_t::clear()
{
  pthread_mutex_lock( &mutex );
  loop_event.clear();
  pthread_mutex_unlock( &mutex );
}

void looped_sndfile_t::stop()
{
  pthread_mutex_lock( &mutex );
  for(uint32_t kle=0;kle<loop_event.size();kle++)
    loop_event[kle].tloop = 0;
  pthread_mutex_unlock( &mutex );
}

class note_event_t {
public:
  note_event_t(uint32_t note,double time,float gain);
  note_event_t();
  inline void process_time(double time) {
    if( time < time_ )
      t = -1;
    else
      t++;
  };
  uint32_t note_;
  double time_;
  float gain_;
  int32_t t;
};

note_event_t::note_event_t(uint32_t note,double time,float gain)
  : note_(note),
    time_(time),
    gain_(gain),
    t(-1)
{
}

note_event_t::note_event_t()
  : note_(0),
    time_(0),
    gain_(1),
    t(-1)
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
  static int osc_addloop(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int osc_stoploop(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int osc_clearloop(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
private:
  std::vector<looped_sndfile_t*> sounds;
  std::vector<std::string> soundnames;
  std::vector<note_event_t> notes;
  double current_time;
  double last_phase;
  bool b_quit;
  double* vtime;
};

sampler_t::sampler_t(const std::string& jname)
  : jackc_t(jname),
    osc_server_t("224.1.2.3","6978"),
    current_time(0),
    last_phase(0),
    b_quit(false),
    vtime(new double[fragsize])
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

int sampler_t::osc_addloop(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 2) && (types[0]=='i') && (types[1]=='f') ){
    ((looped_sndfile_t*)user_data)->add(loop_event_t(argv[0]->i,argv[1]->f));
  }
  return 0;
}

int sampler_t::osc_stoploop(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 0) ){
    ((looped_sndfile_t*)user_data)->stop();
  }
  return 0;
}

int sampler_t::osc_clearloop(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 0) ){
    ((looped_sndfile_t*)user_data)->clear();
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
  delete [] vtime;
}

int sampler_t::process(jack_nframes_t n, const std::vector<float*>& sIn, const std::vector<float*>& sOut)
{
  for(uint32_t k=0;k<sOut.size();k++)
    memset(sOut[k],0,n*sizeof(float));
  for(uint32_t k=0;k<sounds.size();k++){
    wave_t wout(n,sOut[k+1]);
    sounds[k]->loop(wout);
  }
  float* vPhase(sIn[0]);
  for( uint32_t k=0;k<n;k++){
    double dphase(vPhase[k]-last_phase);
    if( dphase < -0.5 )
      dphase += 1.0;
    if( dphase > 0.5 )
      dphase -= 1.0;
    current_time += dphase;
    last_phase = vPhase[k];
    vtime[k] = current_time;
  }
  //std::cerr << chunk_time << ",..." << std::endl;
  //  DEBUG(chunk_time);
  for(unsigned int kn=0;kn<notes.size();kn++){
    if( notes[kn].note_ < sounds.size() ){
      for(uint32_t k=0;k<n;k++){
        notes[kn].process_time(vtime[k]);
        if( (notes[kn].t >= 0) && ((uint32_t)notes[kn].t < sounds[notes[kn].note_]->size()))
          sOut[0][k] += (*sounds[notes[kn].note_])[notes[kn].t];
      }
      //sounds[notes[kn].note_]->add_chunk(chunk_time,notes[kn].time_*samples_per_period,notes[k].gain_,out);
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
      looped_sndfile_t* sf(new looped_sndfile_t(fname,0));
      sounds.push_back(sf);
      soundnames.push_back(fname);
      add_output_port(fname);
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
  DEBUG(1);
  for(uint32_t k=0;k<sounds.size();k++){
    add_method("/"+soundnames[k]+"/add","if",sampler_t::osc_addloop,sounds[k]);
    add_method("/"+soundnames[k]+"/stop","",sampler_t::osc_stoploop,sounds[k]);
    add_method("/"+soundnames[k]+"/clear","",sampler_t::osc_clearloop,sounds[k]);
  }
  DEBUG(1);
  jackc_t::activate();
  DEBUG(1);
  TASCAR::osc_server_t::activate();
  DEBUG(1);
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
  std::string jname("sampler");
  if( argc > 3 )
    jname = argv[3];
  sampler_t s(jname);
  DEBUG(1);
  s.open_sounds(argv[1]);
  DEBUG(1);
  s.open_notes(argv[2]);
  DEBUG(1);
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

