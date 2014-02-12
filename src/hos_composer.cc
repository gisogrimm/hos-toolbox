#include <lo/lo.h>
#include <stdlib.h>
#include <unistd.h>
#include "libhos_harmony.h"
#include "defs.h"
#include <math.h>

class simple_timesig_t {
public:
  uint32_t num;
  uint32_t denom;
};

#define GOODTIMESIG 4

simple_timesig_t good_timesig[GOODTIMESIG] = { {4,2}, {7,4}, {5,2}, {3,4} };

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
  //bool process_key();
  bool process_timesig();
  //int emit_pitch(uint32_t voice,double triad_w);
  int32_t get_key() const;
  int32_t get_mode() const;
  void process_time();
  void read_xml(const std::string& fname);
private:
  std::vector<voice_t> voice;
  harmony_model_t harmony;
  //keysig_t key;
  //keysig_t next_key;
public:
  time_signature_t timesig;
  double time;
private:
  pmf_t ptimesig;
  pmf_t ptimesigchange;
  lo_address lo_addr;
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
  : timesig(0,2,0,0), time(0), lo_addr(lo_address_new_from_url(url.c_str()))
{
  lo_address_set_ttl( lo_addr, 1 );
  voice.resize(5);
  //voice[0].ambitus = Ambitus.treble;
  //voice[1].ambitus = Ambitus.treble;
  //voice[2].ambitus = Ambitus.tenor;
  //voice[3].ambitus = Ambitus.bass;
  //voice[4].ambitus = Ambitus.fezzo;
  for(uint32_t t=0;t<GOODTIMESIG;t++)
    ptimesig.set(t,gauss(t,4));
  ptimesig.update();
  ptimesigchange.set(0,4);
  ptimesigchange.set(1,1);
  ptimesigchange.update();
  read_xml(fname);
  lo_send(lo_addr,"/clear","");
  lo_send(lo_addr,"/key","fii",time,get_key(),get_mode());
}

//int composer_t::emit_pitch(uint32_t v,double triad_w)
//{
//  pmf_t scale;
//  //triad_w = 0.8;
//  scale = (Triad[harmony.current().mode]*triad_w) + (Scale[harmony.current().mode]*(1.0-triad_w));
//  //scale = Triad[key.mode];
//  //DEBUG(key.pitch());
//  scale = scale.vadd(harmony.current().pitch());
//  scale.update();
//  //DEBUG(key.name());
//  pmf_t note_change;
//  if( voice[v].pitch != PITCH_REST ){
//    for(int32_t n=-80;n<81;n++)
//      note_change.set(n,gauss(n,4));
//    note_change = note_change.vadd(voice[v].pitch);
//  }else{
//    for(int32_t n=-100;n<100;n++)
//      note_change.set(n,1);
//  }
//  note_change.update();
//  scale = scale * voice[v].ambitus * note_change;
//  if( !scale.empty() )
//    voice[v].pitch = scale.rand();
//  pmf_t rest;
//  rest.set(0,8);
//  rest.set(1,1);
//  rest.update();
//  if( rest.rand() )
//    voice[v].pitch = PITCH_REST;
//  return voice[v].pitch;
//}

void composer_t::read_xml(const std::string& fname)
{
  xmlpp::DomParser parser(fname);
  xmlpp::Element* root(parser.get_document()->get_root_node());
  if( root ){
    harmony.read_xml(root);
  }
  xmlpp::Node::NodeList nVoice(root->get_children("voice"));
  for(xmlpp::Node::NodeList::iterator nVoiceIt=nVoice.begin(); nVoiceIt != nVoice.end(); ++nVoiceIt){
    xmlpp::Element* eVoice(dynamic_cast<xmlpp::Element*>(*nVoiceIt));
    if( eVoice ){
      int32_t voiceID(get_attribute_double(eVoice,"id"));
      voice[voiceID].read_xml(eVoice);
    }
  }
}

bool composer_t::process_timesig()
{
  time_signature_t old_timesig(timesig);
  if( ptimesigchange.rand() ){
    uint32_t tsidx(ptimesig.rand());
    timesig = time_signature_t(good_timesig[tsidx].num,good_timesig[tsidx].denom,time,0);
  }
  return !(old_timesig == timesig);
}

void composer_t::process_time()
{
  time += 1.0/128.0;
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
  srandom(time(NULL));
  composer_t c("osc.udp://239.255.1.7:9877/","picforth.prob");
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
