#include <lo/lo.h>
#include <stdlib.h>
#include <unistd.h>
#include "libhos_harmony.h"
#include "hos_defs.h"
#include <math.h>
#include "errorhandling.h"
#include "osc_helper.h"
#include <stdio.h>

#define NUM_VOICES 5

static bool  b_quit(false);

class simple_timesig_t {
public:
  uint32_t num;
  uint32_t denom;
};

class voice_t : public melody_model_t {
public:
  voice_t();
  note_t note;
};

voice_t::voice_t()
{
  note.time = 0;
}

class composer_t : public TASCAR::osc_server_t {
public:
  composer_t(const std::string& srv_addr,const std::string& srv_port,const std::string& url,const std::string& fname);
  bool process_timesig();
  int32_t get_key() const;
  int32_t get_mode() const;
  void process_time();
  void read_xml(const std::string& fname);
  static int osc_set_pitch(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  void set_pitch(double c,double w);
private:
  std::vector<voice_t> voice;
  harmony_model_t harmony;
public:
  time_signature_t timesig;
  double time;
private:
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

int32_t composer_t::get_key() const
{
  return harmony.current().pitch();
}

int32_t composer_t::get_mode() const
{
  return harmony.current().mode;
}

composer_t::composer_t(const std::string& srv_addr,const std::string& srv_port,const std::string& url,const std::string& fname)
  : osc_server_t(srv_addr,srv_port), timesig(0,2,0,0), time(0), lo_addr(lo_address_new_from_url(url.c_str())),timesigcnt(0),
    pcenter(NUM_VOICES,0.0),pbandw(NUM_VOICES,48.0),pmodf(NUM_VOICES,1.0),pitchchaos(0.0),beatchaos(0.0),
    timeincrement(1.0/256.0)
{
  lo_address_set_ttl( lo_addr, 1 );
  voice.resize(NUM_VOICES);
  read_xml(fname);
  lo_send(lo_addr,"/clear","");
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
  activate();
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
  //time += 1.0/128.0;
  //time += 1.0/256.0;
  time += timeincrement;
  time = round(256*time)/256;
  //DEBUG(frac(time));
  //DEBUG(time);
  //DEBUG(timesig.bar(time));
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
  //DEBUG(beat);
  if( beat_frac == 0 ){
    //DEBUG(beat);
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

int main(int argc, char** argv)
{
  if( argc < 2 )
    throw TASCAR::ErrMsg("No prob table given.");
  srandom(time(NULL));
  std::string serverport("6978");
  std::string serveraddr("239.255.1.7");
  std::string desturl("osc.udp://239.255.1.7:9877/");
  composer_t c(serveraddr,serverport,desturl,argv[1]);
  while(!b_quit){
    //usleep(15625);
    //usleep(10625);
    usleep(9625);
    c.process_time();
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
