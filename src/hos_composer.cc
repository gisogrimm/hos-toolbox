#include <lo/lo.h>
#include <stdlib.h>
#include <unistd.h>
#include "libhos_harmony.h"
#include "defs.h"
#include <math.h>
#include "errorhandling.h"

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

class composer_t {
public:
  composer_t(const std::string& url,const std::string& fname);
  bool process_timesig();
  int32_t get_key() const;
  int32_t get_mode() const;
  void process_time();
  void read_xml(const std::string& fname);
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
};

int32_t composer_t::get_key() const
{
  return harmony.current().pitch();
}

int32_t composer_t::get_mode() const
{
  return harmony.current().mode;
}

composer_t::composer_t(const std::string& url,const std::string& fname)
  : timesig(0,2,0,0), time(0), lo_addr(lo_address_new_from_url(url.c_str())),timesigcnt(0)
{
  lo_address_set_ttl( lo_addr, 1 );
  voice.resize(5);
  read_xml(fname);
  lo_send(lo_addr,"/clear","");
  lo_send(lo_addr,"/key","fii",time,get_key(),get_mode());
  timesig = time_signature_t(ptimesig.rand());
  timesigcnt = ptimesigbars.rand();
  lo_send(lo_addr,"/timesig","fii",time,timesig.numerator,timesig.denominator);
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
    timesig = time_signature_t(ptimesig.rand());
    timesig.starttime = time;
    timesigcnt = ptimesigbars.rand();
    return !(old_timesig == timesig);
  }
  return false;
}

void composer_t::process_time()
{
  //time += 1.0/128.0;
  time += 1.0/256.0;
  lo_send(lo_addr,"/time","f",time);
  if( harmony.process(timesig.beat(time)) )
    lo_send(lo_addr,"/key","fii",time,get_key(),get_mode());
  if( (timesig.beat(time) == 0) || ((timesig.numerator==0)&&(frac(timesig.beat(time))==0)) ){
    // new bar, optionally update time signature:
    if( process_timesig() ){
      lo_send(lo_addr,"/timesig","fii",time,timesig.numerator,timesig.denominator);
    }
  }
  for(unsigned int k=0;k<5;k++){
    if( voice[k].note.end_time() <= time ){
      voice[k].note = voice[k].process(timesig.beat(time),harmony,timesig);
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
  composer_t c("osc.udp://239.255.1.7:9877/",argv[1]);
  while(true){
    usleep(15625);
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
