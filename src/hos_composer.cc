/**
   \defgroup rtm Real-time music composition tool set

*/

/**
   \file hos_composer.cc
   \ingroup rtm
*/

#include <lo/lo.h>
#include <stdlib.h>
#include <unistd.h>
#include "libhos_harmony.h"
#include "hos_defs.h"
#include <math.h>
#include <tascar/errorhandling.h>
#include <tascar/osc_helper.h>
#include <stdio.h>
#include <tascar/jackclient.h>
#include <tascar/cli.h>

#define NUM_VOICES 5

static bool  b_quit(false);

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
class composer_t : public TASCAR::osc_server_t, public jackc_db_t {
public:
  composer_t(const std::string& srv_addr,const std::string& srv_port,const std::string& url,const std::string& fname,const std::string& jackname);
private:
  bool process_timesig();
  int32_t get_key() const;
  int32_t get_mode() const;
  void process_time();
  void read_xml(const std::string& fname);
  static int osc_set_pitch(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  void set_pitch(double c,double w);
  int inner_process(jack_nframes_t n,const std::vector<float*>& inBuff,const std::vector<float*>& outBuff);
  std::vector<voice_t> voice;///< A vector of voices
  harmony_model_t harmony; ///< The harmony model of the current piece
  time_signature_t timesig; ///< The current time signature
  double time;
  pmf_t ptimesig;
  pmf_t ptimesigbars;
  lo_address lo_addr;
  uint32_t timesigcnt;
  std::vector<float> pcenter;
  std::vector<float> pbandw;
  std::vector<float> pmodf;
  float pitchchaos;
  float beatchaos;
  float timeincrement;
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
composer_t::composer_t(const std::string& srv_addr,
                       const std::string& srv_port,
                       const std::string& url,
                       const std::string& fname, 
                       const std::string& jackname)
  : osc_server_t(srv_addr,srv_port), 
    jackc_db_t(jackname,512),
    timesig(0,2,0,0), time(0), lo_addr(lo_address_new_from_url(url.c_str())),timesigcnt(0),
    pcenter(NUM_VOICES,0.0),pbandw(NUM_VOICES,48.0),pmodf(NUM_VOICES,1.0),pitchchaos(0.0),beatchaos(0.0),
    timeincrement(1.0/256.0)
{
  lo_address_set_ttl( lo_addr, 1 );
  voice.resize(NUM_VOICES);
  read_xml(fname);
  lo_send(lo_addr,"/clear","");
  lo_send(lo_addr,"/numvoices","i",NUM_VOICES);
  lo_send(lo_addr,"/key","fii",time,get_key(),get_mode());
  timesig = time_signature_t(ptimesig.rand());
  timesigcnt = ptimesigbars.rand();
  lo_send(lo_addr,"/timesig","fii",time,timesig.numerator,timesig.denominator);
  for(uint32_t k=0;k<voice.size();k++){
    add_float(std::string("/")+voice[k].get_name()+std::string("/pitch"),&(pcenter[k]));
    add_float(std::string("/")+voice[k].get_name()+std::string("/bw"),&(pbandw[k]));
    add_float(std::string("/")+voice[k].get_name()+std::string("/modf"),&(pmodf[k]));
  }
  add_float("/pitchchaos",&pitchchaos);
  add_float("/beatchaos",&beatchaos);
  add_float("/timeincrement",&timeincrement);
  add_double("/abstime",&time);
  add_bool_true("/composer/quit",&b_quit);
  osc_server_t::activate();
}

void composer_t::read_xml(const std::string& fname)
{
  xmlpp::DomParser parser(fname);
  xmlpp::Element* root(parser.get_document()->get_root_node());
  if( root ){
    harmony.read_xml(root);
    // process time signatures:
    ptimesig.clear();
    xmlpp::Node::NodeList nTimesig(root->get_children("timesig"));
    for(xmlpp::Node::NodeList::iterator nTimesigIt=nTimesig.begin(); nTimesigIt != nTimesig.end(); ++nTimesigIt){
      xmlpp::Element* eTimesig(dynamic_cast<xmlpp::Element*>(*nTimesigIt));
      if( eTimesig ){
        std::string s_val(eTimesig->get_attribute_value("v"));
        size_t cp(s_val.find("/"));
        if( cp == std::string::npos )
          throw TASCAR::ErrMsg("Invalid time signature: "+s_val);
        time_signature_t ts(atoi(s_val.substr(0,cp).c_str()),
                            atoi(s_val.substr(cp+1).c_str()),0,0);
        ptimesig.set(ts.hash(),get_attribute_double(eTimesig,"p"));
      }
    }
    ptimesig.update();
    ptimesigbars.clear();
    xmlpp::Node::NodeList nTimesigbars(root->get_children("timesigbars"));
    for(xmlpp::Node::NodeList::iterator nTimesigbarsIt=nTimesigbars.begin(); nTimesigbarsIt != nTimesigbars.end(); ++nTimesigbarsIt){
      xmlpp::Element* eTimesigbars(dynamic_cast<xmlpp::Element*>(*nTimesigbarsIt));
      if( eTimesigbars ){
        ptimesigbars.set(get_attribute_double(eTimesigbars,"v"),get_attribute_double(eTimesigbars,"p"));
      }
    }
    ptimesigbars.update();
    // read voices:
    xmlpp::Node::NodeList nVoice(root->get_children("voice"));
    for(xmlpp::Node::NodeList::iterator nVoiceIt=nVoice.begin(); nVoiceIt != nVoice.end(); ++nVoiceIt){
      xmlpp::Element* eVoice(dynamic_cast<xmlpp::Element*>(*nVoiceIt));
      if( eVoice ){
        int32_t voiceID(get_attribute_double(eVoice,"id"));
        voice[voiceID].read_xml(eVoice);
      }
    }
  }
}

bool composer_t::process_timesig()
{
  if( timesigcnt )
    timesigcnt--;
  if( !timesigcnt ){
    time_signature_t old_timesig(timesig);
    try{
      timesig = time_signature_t(ptimesig.rand());
    }
    catch(const std::exception& e){
      DEBUG(e.what());
    }
    timesig.starttime = time;
    //DEBUG(time);
    //DEBUG(frac(time));
    try{
      timesigcnt = ptimesigbars.rand();
    }
    catch(const std::exception& e){
      DEBUG(e.what());
    }
    return !(old_timesig == timesig);
  }
  return false;
}

void composer_t::process_time()
{
  time += timeincrement;
  time = round(256*time)/256;
  lo_send(lo_addr,"/time","f",time);
  double beat(timesig.beat(time));
  double beat_frac(frac(beat));
  if( (beat == 0) || ((timesig.numerator==0)&&(beat_frac==0)) ){
    // new bar, optionally update time signature:
    if( process_timesig() ){
      lo_send(lo_addr,"/timesig","fii",time,timesig.numerator,timesig.denominator);
    }
  }
  beat = timesig.beat(time);
  beat_frac = frac(beat);
  if( beat_frac == 0 ){
    lo_send(lo_addr,"/beat","f",beat);
  }
  if( harmony.process(beat) )
    lo_send(lo_addr,"/key","fii",time,get_key(),get_mode());
  for(unsigned int k=0;k<voice.size();k++){
    if( voice[k].note.end_time() <= time ){
      voice[k].note = voice[k].process(beat,harmony,timesig,pcenter[k],pbandw[k],1.0-pow(pitchchaos,2.0),1.0-pow(beatchaos,1.0),pmodf[k]);
      voice[k].note.time = time;
      lo_send(lo_addr,"/note","iiif",k,voice[k].note.pitch,voice[k].note.length,voice[k].note.time);
    }
  }
}

int composer_t::inner_process(jack_nframes_t n,const std::vector<float*>& inBuff,const std::vector<float*>& outBuff)
{
  process_time();
  return 0;
}

int main(int argc, char** argv)
{
  std::string serverport("6978");
  std::string serveraddr("239.255.1.7");
  std::string desturl("osc.udp://239.255.1.7:9877/");
  std::string jackname("composer");
  std::string configfile;
  const char *options = "hj:u:p:a:";
  struct option long_options[] = { 
      { "help",     0, 0, 'h' },
      { "jackname", 1, 0, 'j' },
      { "desturl",  1, 0, 'u' },
      { "port",     1, 0, 'p' },
      { "address",  1, 0, 'a' },
      { 0, 0, 0, 0 }
  };
  int opt(0);
  int option_index(0);
  while( (opt = getopt_long(argc, argv, options,
                            long_options, &option_index)) != -1){
    switch(opt){
      case 'h':
        TASCAR::app_usage("hos_composer",long_options,"configfile");
        return -1;
    case 'j':
      jackname = optarg;
      break;
    case 'u':
      desturl = optarg;
      break;
    case 'p' :
      serverport = optarg;
      break;
    case 'a' :
      serveraddr = optarg;
      break;
    }
  }
  if( optind < argc )
    configfile = argv[optind++];
  if( configfile.size() == 0 ){
    TASCAR::app_usage("hos_composer",long_options,"configfile");
    return -1;
  }
  srandom(time(NULL));
  composer_t c(serveraddr,serverport,desturl,configfile,jackname);
  c.jackc_db_t::activate();
  while(!b_quit){
    usleep(99625);
  }
  c.jackc_db_t::deactivate();
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
