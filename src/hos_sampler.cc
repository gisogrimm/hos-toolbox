#include "hos_defs.h"
#include "libhos_audiochunks.h"
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <math.h>
#include <string.h>
#include <tascar/errorhandling.h>
#include <tascar/jackclient.h>
#include <tascar/osc_helper.h>
#include <unistd.h>

using namespace HoS;

// loop event
// /sampler/sound/add loopcnt gain
// /sampler/sound/clear
// /sampler/sound/stop

class loop_event_t {
public:
  loop_event_t() : tsample(0), tloop(1), loopgain(1.0){};
  loop_event_t(int32_t cnt, float gain)
      : tsample(0), tloop(cnt), loopgain(gain){};
  bool valid() const { return tsample || tloop; };
  inline void process(TASCAR::wave_t& out_chunk, const TASCAR::wave_t& in_chunk)
  {
    uint32_t n_(in_chunk.size());
    for(uint32_t k = 0; k < out_chunk.size(); k++) {
      if(tloop == -2) {
        tsample = 0;
        tloop = 0;
      }
      if(tsample)
        tsample--;
      else {
        if(tloop) {
          if(tloop > 0)
            tloop--;
          tsample = n_ - 1;
        }
      }
      if(tsample || tloop)
        out_chunk[k] += loopgain * in_chunk[n_ - 1 - tsample];
    }
  };
  uint32_t tsample;
  int32_t tloop;
  float loopgain;
};

class looped_sndfile_t : public sndfile_t {
public:
  looped_sndfile_t(const std::string& fname, uint32_t channel);
  ~looped_sndfile_t();
  void add(loop_event_t);
  void clear();
  void stop();
  void loop(wave_t& chunk);

private:
  pthread_mutex_t mutex;
  std::vector<loop_event_t> loop_event;
};

looped_sndfile_t::looped_sndfile_t(const std::string& fname, uint32_t channel)
    : sndfile_t(fname, channel)
{
  loop_event.reserve(1024);
  pthread_mutex_init(&mutex, NULL);
}

looped_sndfile_t::~looped_sndfile_t()
{
  pthread_mutex_trylock(&mutex);
  pthread_mutex_unlock(&mutex);
  pthread_mutex_destroy(&mutex);
}

void looped_sndfile_t::loop(wave_t& chunk)
{
  pthread_mutex_lock(&mutex);
  uint32_t kle(loop_event.size());
  while(kle) {
    kle--;
    if(loop_event[kle].valid()) {
      loop_event[kle].process(chunk, *this);
    } else {
      loop_event.erase(loop_event.begin() + kle);
    }
  }
  pthread_mutex_unlock(&mutex);
}

void looped_sndfile_t::add(loop_event_t le)
{
  pthread_mutex_lock(&mutex);
  loop_event.push_back(le);
  pthread_mutex_unlock(&mutex);
}

void looped_sndfile_t::clear()
{
  pthread_mutex_lock(&mutex);
  loop_event.clear();
  pthread_mutex_unlock(&mutex);
}

void looped_sndfile_t::stop()
{
  pthread_mutex_lock(&mutex);
  for(uint32_t kle = 0; kle < loop_event.size(); kle++)
    loop_event[kle].tloop = 0;
  pthread_mutex_unlock(&mutex);
}

class note_event_t {
public:
  note_event_t(uint32_t note, double time, float gain, double duration);
  note_event_t();
  inline void process_time(double time)
  {
    if((old_time <= time_) && (time > time_)) {
      t = 0;
      fade_gain = 1.0;
    } else {
      t++;
    }
    if(time - time_ > duration_)
      fade_gain *= fade_rate;
    if(fade_gain < 1e-10)
      fade_gain = 0.0;
    old_time = time;
  };
  uint32_t note_;
  double time_;
  double old_time;
  float gain_;
  double duration_;
  int32_t t;
  double fade_gain;
  double fade_rate;
};

note_event_t::note_event_t(uint32_t note, double time, float gain,
                           double duration)
    : note_(note), time_(time), old_time(0), gain_(gain), duration_(duration),
      t(-1), fade_gain(1.0), fade_rate(exp(-1.0 / (0.25 * 44100)))
{
}

note_event_t::note_event_t()
    : note_(0), time_(0), old_time(0), gain_(1), duration_(1.0), t(-1),
      fade_gain(8.0), fade_rate(exp(-1.0 / (0.25 * 44100)))
{
}

class sampler_t : public jackc_t, public TASCAR::osc_server_t {
public:
  sampler_t(const std::string& jname, const std::string& announce);
  ~sampler_t();
  void run();
  int process(jack_nframes_t n, const std::vector<float*>& sIn,
              const std::vector<float*>& sOut);
  void open_sounds(const std::string& fname);
  void open_notes(const std::string& fname);
  void set_t0(double t0);
  void set_loop_time(double tloop) { loop_time = tloop; };
  void set_gain(float newgain, double duration)
  {
    dgain = (newgain - mastergain) / (duration * srate);
    tfader = srate * duration;
  };
  void set_auxgain(float newgain, double duration)
  {
    dauxgain = (newgain - auxgain) / (duration * srate);
    tauxfader = srate * duration;
  };
  void quit() { b_quit = true; };
  static int osc_set_t0(const char* path, const char* types, lo_arg** argv,
                        int argc, lo_message msg, void* user_data);
  static int osc_set_loop_time(const char* path, const char* types,
                               lo_arg** argv, int argc, lo_message msg,
                               void* user_data);
  static int osc_quit(const char* path, const char* types, lo_arg** argv,
                      int argc, lo_message msg, void* user_data);
  static int osc_addloop(const char* path, const char* types, lo_arg** argv,
                         int argc, lo_message msg, void* user_data);
  static int osc_stoploop(const char* path, const char* types, lo_arg** argv,
                          int argc, lo_message msg, void* user_data);
  static int osc_clearloop(const char* path, const char* types, lo_arg** argv,
                           int argc, lo_message msg, void* user_data);
  static int osc_set_gain(const char* path, const char* types, lo_arg** argv,
                          int argc, lo_message msg, void* user_data);
  static int osc_set_auxgain(const char* path, const char* types, lo_arg** argv,
                             int argc, lo_message msg, void* user_data);

private:
  std::vector<looped_sndfile_t*> sounds;
  std::vector<std::string> soundnames;
  std::vector<note_event_t> notes;
  double timescale;
  double current_time;
  double last_phase;
  bool b_quit;
  double* vtime;
  float* vgain;
  float* vauxgain;
  double loop_time;
  double mastergain;
  uint32_t tfader;
  double dgain;
  double auxgain;
  uint32_t tauxfader;
  double dauxgain;
  lo_address lo_addr;
  bool b_announce;
  int32_t barno;
};

sampler_t::sampler_t(const std::string& jname, const std::string& announce)
    : jackc_t(jname), osc_server_t("239.255.1.7", "6978", "UDP"),
      timescale(1.0), current_time(-1), last_phase(0), b_quit(false),
      vtime(new double[fragsize]), vgain(new float[fragsize]),
      vauxgain(new float[fragsize]), loop_time(0), mastergain(0.0), tfader(0),
      dgain(0.0), auxgain(0.0), tauxfader(0), dauxgain(0.0),
      b_announce(!announce.empty()), barno(-100)
{
  add_input_port("phase");
  add_output_port("out");
  add_output_port("auxout");
  set_prefix("/" + jname);
  add_method("/t0", "f", sampler_t::osc_set_t0, this);
  add_method("/loop", "f", sampler_t::osc_set_loop_time, this);
  add_double("/gain", &mastergain);
  add_method("/gain", "ff", osc_set_gain, this);
  add_double("/auxgain", &auxgain);
  add_method("/auxgain", "ff", osc_set_auxgain, this);
  add_double("/timescale", &timescale);
  add_method("/quit", "", sampler_t::osc_quit, this);
  if(b_announce)
    lo_addr = lo_address_new_from_url(announce.c_str());
}

int sampler_t::osc_set_t0(const char* path, const char* types, lo_arg** argv,
                          int argc, lo_message msg, void* user_data)
{
  if((user_data) && (argc == 1) && (types[0] == 'f')) {
    ((sampler_t*)user_data)->set_t0(argv[0]->f);
  }
  return 0;
}

int sampler_t::osc_set_loop_time(const char* path, const char* types,
                                 lo_arg** argv, int argc, lo_message msg,
                                 void* user_data)
{
  if((user_data) && (argc == 1) && (types[0] == 'f')) {
    ((sampler_t*)user_data)->set_loop_time(argv[0]->f);
  }
  return 0;
}

int sampler_t::osc_addloop(const char* path, const char* types, lo_arg** argv,
                           int argc, lo_message msg, void* user_data)
{
  if((user_data) && (argc == 2) && (types[0] == 'i') && (types[1] == 'f')) {
    ((looped_sndfile_t*)user_data)->add(loop_event_t(argv[0]->i, argv[1]->f));
  }
  return 0;
}

int sampler_t::osc_set_gain(const char* path, const char* types, lo_arg** argv,
                            int argc, lo_message msg, void* user_data)
{
  if((user_data) && (argc == 2) && (types[0] == 'f') && (types[1] == 'f')) {
    ((sampler_t*)user_data)->set_gain(argv[0]->f, argv[1]->f);
  }
  return 0;
}

int sampler_t::osc_set_auxgain(const char* path, const char* types,
                               lo_arg** argv, int argc, lo_message msg,
                               void* user_data)
{
  if((user_data) && (argc == 2) && (types[0] == 'f') && (types[1] == 'f')) {
    ((sampler_t*)user_data)->set_auxgain(argv[0]->f, argv[1]->f);
  }
  return 0;
}

int sampler_t::osc_stoploop(const char* path, const char* types, lo_arg** argv,
                            int argc, lo_message msg, void* user_data)
{
  if((user_data) && (argc == 0)) {
    ((looped_sndfile_t*)user_data)->stop();
  }
  return 0;
}

int sampler_t::osc_clearloop(const char* path, const char* types, lo_arg** argv,
                             int argc, lo_message msg, void* user_data)
{
  if((user_data) && (argc == 0)) {
    ((looped_sndfile_t*)user_data)->clear();
  }
  return 0;
}

int sampler_t::osc_quit(const char* path, const char* types, lo_arg** argv,
                        int argc, lo_message msg, void* user_data)
{
  if(user_data)
    ((sampler_t*)user_data)->quit();
  return 0;
}

void sampler_t::set_t0(double t0)
{
  current_time = t0;
}

sampler_t::~sampler_t()
{
  for(unsigned int k = 0; k < sounds.size(); k++)
    delete sounds[k];
  delete[] vtime;
  delete[] vgain;
  delete[] vauxgain;
}

int sampler_t::process(jack_nframes_t n, const std::vector<float*>& sIn,
                       const std::vector<float*>& sOut)
{
  for(uint32_t k = 0; k < sOut.size(); k++)
    memset(sOut[k], 0, n * sizeof(float));
  // for(uint32_t k=0;k<sounds.size();k++){
  //  wave_t wout(n,sOut[k+1]);
  //  sounds[k]->loop(wout);
  //}
  float* vPhase(sIn[0]);
  for(uint32_t k = 0; k < n; k++) {
    double dphase(vPhase[k] - last_phase);
    if(dphase < -0.5)
      dphase += 1.0;
    if(dphase > 0.5)
      dphase -= 1.0;
    current_time += timescale * dphase;
    if((loop_time > 0) && (current_time >= loop_time))
      current_time = 0.0;
    last_phase = vPhase[k];
    vtime[k] = current_time;
    int32_t newbarno(floor(current_time));
    if(newbarno != barno) {
      if(b_announce)
        lo_send(lo_addr, "/bar", "i", newbarno);
      barno = newbarno;
    }
    if(tfader) {
      mastergain += dgain;
      tfader--;
    }
    vgain[k] = mastergain;
    if(tauxfader) {
      auxgain += dauxgain;
      tauxfader--;
    }
    vauxgain[k] = auxgain;
  }
  // std::cerr << chunk_time << ",..." << std::endl;
  //  DEBUG(chunk_time);
  for(unsigned int kn = 0; kn < notes.size(); kn++) {
    if(notes[kn].note_ < sounds.size()) {
      for(uint32_t k = 0; k < n; k++) {
        notes[kn].process_time(vtime[k]);
        if((notes[kn].t >= 0) &&
           ((uint32_t)notes[kn].t < sounds[notes[kn].note_]->size())) {
          float val((*sounds[notes[kn].note_])[notes[kn].t] * notes[kn].gain_ *
                    notes[kn].fade_gain);
          sOut[0][k] += val * vgain[k];
          sOut[1][k] += val * vauxgain[k];
        }
      }
      // sounds[notes[kn].note_]->add_chunk(chunk_time,notes[kn].time_*samples_per_period,notes[k].gain_,out);
    }
  }
  return 0;
}

void sampler_t::open_sounds(const std::string& fname)
{
  std::ifstream fh(fname.c_str());
  if(!fh.good())
    throw TASCAR::ErrMsg("Unable to open soundfont file \"" + fname + "\".");
  while(!fh.eof()) {
    char ctmp[1024];
    memset(ctmp, 0, 1024);
    fh.getline(ctmp, 1023);
    std::string fname(ctmp);
    if(fname.size()) {
      looped_sndfile_t* sf(new looped_sndfile_t(fname, 0));
      sounds.push_back(sf);
      soundnames.push_back(fname);
      // add_output_port(fname);
    }
  }
}

void sampler_t::open_notes(const std::string& fname)
{
  std::ifstream fh(fname.c_str());
  if(!fh.good())
    throw TASCAR::ErrMsg("Unable to open pitch file \"" + fname + "\".");
  while(!fh.eof()) {
    char ctmp[1024];
    memset(ctmp, 0, 1024);
    fh.getline(ctmp, 1023);
    note_event_t n;
    if(sscanf(ctmp, "%lf %d %f %lf", &(n.time_), &(n.note_), &(n.gain_),
              &(n.duration_)) >= 2)
      notes.push_back(n);
  }
}

void sampler_t::run()
{
  // DEBUG(1);
  // for(uint32_t k=0;k<sounds.size();k++){
  //  add_method("/"+soundnames[k]+"/add","if",sampler_t::osc_addloop,sounds[k]);
  //  add_method("/"+soundnames[k]+"/stop","",sampler_t::osc_stoploop,sounds[k]);
  //  add_method("/"+soundnames[k]+"/clear","",sampler_t::osc_clearloop,sounds[k]);
  //}
  // DEBUG(1);
  jackc_t::activate();
  // DEBUG(1);
  TASCAR::osc_server_t::activate();
  // DEBUG(1);
  try {
    connect_in(0, "phase:phase");
  }
  catch(const std::exception& e) {
    std::cerr << "Warning: " << e.what() << std::endl;
  };
  while(!b_quit) {
    sleep(1);
  }
  TASCAR::osc_server_t::deactivate();
  jackc_t::deactivate();
}

void usage(struct option* opt)
{
  std::cout << "Usage:\n\nhos_sampler [options] soundfont notesfile [ jackname "
               "]\n\nOptions:\n\n";
  while(opt->name) {
    std::cout << "  -" << (char)(opt->val) << " " << (opt->has_arg ? "#" : "")
              << "\n  --" << opt->name << (opt->has_arg ? "=#" : "") << "\n\n";
    opt++;
  }
}

int main(int argc, char** argv)
{
  std::string jname("sampler");
  std::string soundfont("");
  std::string notefile("");
  std::string announce("");
  const char* options = "a:h";
  struct option long_options[] = {
      {"announce", 1, 0, 'a'}, {"help", 0, 0, 'h'}, {0, 0, 0, 0}};
  int opt(0);
  int option_index(0);
  while((opt = getopt_long(argc, argv, options, long_options, &option_index)) !=
        -1) {
    switch(opt) {
    case 'a':
      announce = optarg;
      break;
    case 'h':
      usage(long_options);
      return -1;
    }
  }
  if(optind < argc)
    soundfont = argv[optind++];
  if(optind < argc)
    notefile = argv[optind++];
  if(optind < argc)
    jname = argv[optind++];
  if(notefile.empty())
    throw TASCAR::ErrMsg("notes filename is empty.");
  if(soundfont.empty())
    throw TASCAR::ErrMsg("soundfont filename is empty.");
  sampler_t s(jname, announce);
  // DEBUG(1);
  s.open_sounds(soundfont);
  // DEBUG(1);
  s.open_notes(notefile);
  // DEBUG(1);
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
