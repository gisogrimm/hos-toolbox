/**
   \defgroup rtm Real-time music composition tool set

*/

/**
   \file hos_composer.cc
   \ingroup rtm
*/

#include "hos_defs.h"
#include "libhos_harmony.h"
#include <lo/lo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <tascar/cli.h>
#include <tascar/errorhandling.h>
//#include <tascar/jackclient.h>
#include <sys/time.h>
#include <tascar/osc_helper.h>
#include <thread>
#include <unistd.h>

#define NUM_VOICES 5

static bool b_quit(false);

class tictoc_t {
public:
  tictoc_t();
  double toc();
  void tic();

private:
  struct timeval tv1;
  struct timeval tv2;
  struct timezone tz;
  double t;
};

tictoc_t::tictoc_t()
{
  memset(&tv1, 0, sizeof(timeval));
  memset(&tv2, 0, sizeof(timeval));
  memset(&tz, 0, sizeof(timezone));
  t = 0;
  tic();
}

void tictoc_t::tic()
{
  gettimeofday(&tv1, &tz);
}

double tictoc_t::toc()
{
  gettimeofday(&tv2, &tz);
  tv2.tv_sec -= tv1.tv_sec;
  if(tv2.tv_usec >= tv1.tv_usec)
    tv2.tv_usec -= tv1.tv_usec;
  else {
    tv2.tv_sec--;
    tv2.tv_usec += 1000000;
    tv2.tv_usec -= tv1.tv_usec;
  }
  t = (float)(tv2.tv_sec) + 0.000001 * (float)(tv2.tv_usec);
  return t;
}

/**
   \brief Definition of one voice
   \ingroup rtm

 */
class voice_t : public melody_model_t {
public:
  voice_t();
  note_t note;
};

voice_t::voice_t()
{
  note.time = 0;
}

/**
   \brief Composition class
   \ingroup rtm
 */
class composer_t : public TASCAR::osc_server_t {
public:
  composer_t(const std::string& srv_addr, const std::string& srv_port,
             const std::string& url, const std::string& fname);
  ~composer_t();

private:
  bool process_timesig();
  int32_t get_key() const;
  int32_t get_mode() const;
  void process_time();
  void read_xml(const std::string& fname);
  static int osc_set_pitch(const char* path, const char* types, lo_arg** argv,
                           int argc, lo_message msg, void* user_data);
  void set_pitch(double c, double w);
  void comp_thread();
  std::vector<voice_t> voice; ///< A vector of voices
  harmony_model_t harmony;    ///< The harmony model of the current piece
  time_signature_t timesig;   ///< The current time signature
  uint64_t time = 0;          ///< Time, measured in 1/64
  pmf_t ptimesig;
  pmf_t ptimesigbars;
  lo_address lo_addr;
  uint32_t timesigcnt;
  std::vector<float> pcenter;
  std::vector<float> pbandw;
  std::vector<float> pmodf;
  float pitchchaos;
  float beatchaos;
  float bpm = 60;
  float duration = 0;
  std::thread cthread;
  bool endthread;
};

/**
   \brief Return current key pitch
 */
int32_t composer_t::get_key() const
{
  return harmony.current().pitch();
}

/**
   \brief Return current key mode
 */
int32_t composer_t::get_mode() const
{
  return harmony.current().mode;
}

/**
   \param srv_addr Address of OSC server
   \param srv_port Port to listen on for incoming OSC messages
   \param url Destination of OSC messages
   \param fname Configuration file name
 */
composer_t::composer_t(const std::string& srv_addr, const std::string& srv_port,
                       const std::string& url, const std::string& fname)
    : osc_server_t(srv_addr, srv_port, "UDP"), timesig(0, 2, 0, 0), time(0),
      lo_addr(lo_address_new_from_url(url.c_str())), timesigcnt(0),
      pcenter(NUM_VOICES, 0.0), pbandw(NUM_VOICES, 48.0),
      pmodf(NUM_VOICES, 1.0), pitchchaos(0.0), beatchaos(0.0), bpm(60),
      endthread(false)
{
  lo_address_set_ttl(lo_addr, 1);
  voice.resize(NUM_VOICES);
  read_xml(fname);
  lo_send(lo_addr, "/clear", "");
  lo_send(lo_addr, "/numvoices", "i", NUM_VOICES);
  lo_send(lo_addr, "/key", "fii", 0.0f, get_key(), get_mode());
  timesig = time_signature_t(ptimesig.rand());
  timesigcnt = ptimesigbars.rand();
  lo_send(lo_addr, "/timesig", "fii", 0.0f, timesig.numerator,
          timesig.denominator);
  for(uint32_t k = 0; k < voice.size(); k++) {
    add_float(std::string("/") + voice[k].get_name() + std::string("/pitch"),
              &(pcenter[k]));
    add_float(std::string("/") + voice[k].get_name() + std::string("/bw"),
              &(pbandw[k]));
    add_float(std::string("/") + voice[k].get_name() + std::string("/modf"),
              &(pmodf[k]));
  }
  add_float("/pitchchaos", &pitchchaos);
  add_float("/beatchaos", &beatchaos);
  add_float("/bpm", &bpm);
  // add_double("/abstime", &dtime);
  add_bool_true("/composer/quit", &b_quit);
  osc_server_t::activate();
  cthread = std::thread(&composer_t::comp_thread, this);
}

composer_t::~composer_t()
{
  endthread = true;
  if(cthread.joinable())
    cthread.join();
}

void composer_t::read_xml(const std::string& fname)
{
  xmlpp::DomParser parser(fname);
  xmlpp::Element* root(parser.get_document()->get_root_node());
  if(root) {
    harmony.read_xml(root);
    duration = get_attribute_double(root, "duration", 0);
    bpm = get_attribute_double(root, "bpm", 60);
    // process time signatures:
    ptimesig.clear();
    xmlpp::Node::NodeList nTimesig(root->get_children("timesig"));
    for(xmlpp::Node::NodeList::iterator nTimesigIt = nTimesig.begin();
        nTimesigIt != nTimesig.end(); ++nTimesigIt) {
      xmlpp::Element* eTimesig(dynamic_cast<xmlpp::Element*>(*nTimesigIt));
      if(eTimesig) {
        std::string s_val(eTimesig->get_attribute_value("v"));
        size_t cp(s_val.find("/"));
        if(cp == std::string::npos)
          throw TASCAR::ErrMsg("Invalid time signature: " + s_val);
        time_signature_t ts(atoi(s_val.substr(0, cp).c_str()),
                            atoi(s_val.substr(cp + 1).c_str()), 0, 0);
        ptimesig.set(ts.hash(), get_attribute_double(eTimesig, "p"));
      }
    }
    ptimesig.update();
    ptimesigbars.clear();
    xmlpp::Node::NodeList nTimesigbars(root->get_children("timesigbars"));
    for(xmlpp::Node::NodeList::iterator nTimesigbarsIt = nTimesigbars.begin();
        nTimesigbarsIt != nTimesigbars.end(); ++nTimesigbarsIt) {
      xmlpp::Element* eTimesigbars(
          dynamic_cast<xmlpp::Element*>(*nTimesigbarsIt));
      if(eTimesigbars) {
        ptimesigbars.set(get_attribute_double(eTimesigbars, "v"),
                         get_attribute_double(eTimesigbars, "p"));
      }
    }
    ptimesigbars.update();
    // read voices:
    xmlpp::Node::NodeList nVoice(root->get_children("voice"));
    for(xmlpp::Node::NodeList::iterator nVoiceIt = nVoice.begin();
        nVoiceIt != nVoice.end(); ++nVoiceIt) {
      xmlpp::Element* eVoice(dynamic_cast<xmlpp::Element*>(*nVoiceIt));
      if(eVoice) {
        int32_t voiceID(get_attribute_double(eVoice, "id"));
        voice[voiceID].read_xml(eVoice);
      }
    }
  }
}

bool composer_t::process_timesig()
{
  if(timesigcnt)
    timesigcnt--;
  if(!timesigcnt) {
    time_signature_t old_timesig(timesig);
    try {
      timesig = time_signature_t(ptimesig.rand());
    }
    catch(const std::exception& e) {
      DEBUG(e.what());
    }
    timesig.starttime = time;
    // DEBUG(time);
    // DEBUG(frac(time));
    try {
      timesigcnt = ptimesigbars.rand();
    }
    catch(const std::exception& e) {
      DEBUG(e.what());
    }
    return !(old_timesig == timesig);
  }
  return false;
}

/**
 * @brief Main composition callback, called every 1/64 note
 */
void composer_t::process_time()
{
  ++time;
  double dtime(time / 64.0);
  if((duration > 0) && (dtime > duration + 5.5))
    b_quit = true;
  else
    lo_send(lo_addr, "/time", "f", dtime);
  if((duration > 0) && (dtime > duration))
    return;
  double beat(timesig.beat(dtime));
  double beat_frac(frac(beat));
  if((beat == 0) || ((timesig.numerator == 0) && (beat_frac == 0))) {
    // new bar, optionally update time signature:
    if(process_timesig()) {
      lo_send(lo_addr, "/timesig", "fii", dtime, timesig.numerator,
              timesig.denominator);
    }
  }
  beat = timesig.beat(dtime);
  beat_frac = frac(beat);
  if(beat_frac == 0) {
    lo_send(lo_addr, "/beat", "f", beat);
  }
  if(harmony.process(beat))
    lo_send(lo_addr, "/key", "fii", dtime, get_key(), get_mode());
  for(unsigned int k = 0; k < voice.size(); k++) {
    if(voice[k].note.end_time() <= dtime) {
      voice[k].note = voice[k].process(beat, harmony, timesig, pcenter[k],
                                       pbandw[k], 1.0 - pow(pitchchaos, 2.0),
                                       1.0 - pow(beatchaos, 1.0), pmodf[k]);
      voice[k].note.time = dtime;
      lo_send(lo_addr, "/note", "iiif", k, voice[k].note.pitch,
              voice[k].note.length, voice[k].note.time);
    }
  }
}

void composer_t::comp_thread()
{
  tictoc_t tictoc;
  while(!endthread) {
    tictoc.tic();
    process_time();
    double periodtime(60.0 / (64.0 * bpm / timesig.denominator));
    double t(tictoc.toc());
    periodtime -= t;
    usleep(1.0e6 * std::max(0.0, periodtime));
  }
}

int main(int argc, char** argv)
{
  std::string serverport("6978");
  std::string serveraddr("239.255.1.7");
  std::string desturl("osc.udp://239.255.1.7:9877/");
  std::string configfile;
  const char* options = "hu:p:a:";
  struct option long_options[] = {{"help", 0, 0, 'h'},
                                  {"desturl", 1, 0, 'u'},
                                  {"port", 1, 0, 'p'},
                                  {"address", 1, 0, 'a'},
                                  {0, 0, 0, 0}};
  int opt(0);
  int option_index(0);
  while((opt = getopt_long(argc, argv, options, long_options, &option_index)) !=
        -1) {
    switch(opt) {
    case 'h':
      TASCAR::app_usage("hos_composer", long_options, "configfile");
      return -1;
    case 'u':
      desturl = optarg;
      break;
    case 'p':
      serverport = optarg;
      break;
    case 'a':
      serveraddr = optarg;
      break;
    }
  }
  if(optind < argc)
    configfile = argv[optind++];
  if(configfile.size() == 0) {
    TASCAR::app_usage("hos_composer", long_options, "configfile");
    return -1;
  }
  srandom(time(NULL));
  composer_t c(serveraddr, serverport, desturl, configfile);
  while(!b_quit) {
    usleep(99625);
  }
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
