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

simple_timesig_t good_timesig[GOODTIMESIG] = { {4,2}, {11,4}, {5,2}, {3,4} };

class voice_t {
public:
  voice_t();
  pdf_t ambitus;
  int32_t pitch;
  note_t note;
};

voice_t::voice_t()
  : ambitus(Ambitus.treble),pitch(PITCH_REST)
{
  note.time = 0;
}

class composer_t {
public:
  composer_t(const std::string& url);
  bool process_key();
  bool process_timesig();
  int emit_pitch(uint32_t voice,double triad_w);
  int32_t get_key() const;
  int32_t get_mode() const;
  void process_time();
private:
  std::vector<voice_t> voice;
  keysig_t key;
  keysig_t next_key;
public:
  time_signature_t timesig;
  double time;
private:
  pdf_t ptimesig;
  pdf_t ptimesigchange;
  lo_address lo_addr;
};

int32_t composer_t::get_key() const
{
  return key.pitch();
}

int32_t composer_t::get_mode() const
{
  return key.mode;
}

composer_t::composer_t(const std::string& url)
  : timesig(0,2,0,0), time(0), lo_addr(lo_address_new_from_url(url.c_str()))
{
  lo_address_set_ttl( lo_addr, 1 );
  voice.resize(5);
  voice[0].ambitus = Ambitus.treble;
  voice[1].ambitus = Ambitus.treble;
  voice[2].ambitus = Ambitus.tenor;
  voice[3].ambitus = Ambitus.bass;
  voice[4].ambitus = Ambitus.fezzo;
  for(uint32_t t=0;t<GOODTIMESIG;t++)
    ptimesig.set(t,gauss(t,4));
  ptimesig.update();
  ptimesigchange.set(0,4);
  ptimesigchange.set(1,1);
  ptimesigchange.update();
  lo_send(lo_addr,"/clear","");
}

int composer_t::emit_pitch(uint32_t v,double triad_w)
{
  pdf_t scale;
  //triad_w = 0.8;
  scale = (Triad[key.mode]*triad_w) + (Scale[key.mode]*(1.0-triad_w));
  //scale = Triad[key.mode];
  //DEBUG(key.pitch());
  scale = scale.vadd(key.pitch());
  scale.update();
  //DEBUG(key.name());
  pdf_t note_change;
  if( voice[v].pitch != PITCH_REST ){
    for(int32_t n=-80;n<81;n++)
      note_change.set(n,gauss(n,4));
    note_change = note_change.vadd(voice[v].pitch);
  }else{
    for(int32_t n=-100;n<100;n++)
      note_change.set(n,1);
  }
  note_change.update();
  scale = scale * voice[v].ambitus * note_change;
  if( !scale.empty() )
    voice[v].pitch = scale.rand();
  pdf_t rest;
  rest.set(0,8);
  rest.set(1,1);
  rest.update();
  if( rest.rand() )
    voice[v].pitch = PITCH_REST;
  return voice[v].pitch;
}

bool composer_t::process_key()
{
  keysig_t old_key(key);
  key = next_key;
  pdf_t keychange;
  for(int k=1;k<=6;k++){
    keychange.set(k,1.0/(k*k));
    keychange.set(-k,1.0/(k*k));
  }
  keychange.set(0,0.5);
  keychange = keychange.vadd(old_key.fifths);
  pdf_t keysig_taste;
  for(int k=-5;k<6;k++)
    keysig_taste.set(k,gauss(k+1,6));
  keysig_taste.update();
  keychange = keychange * keysig_taste;
  pdf_t major;
  major.set(0,3);
  major.set(1,2);
  major.update();
  next_key.fifths = keychange.rand();
  if( major.rand() == 0 )
    next_key.mode = keysig_t::major;
  else
    next_key.mode = keysig_t::minor;
  //next_key.fifths = 1;
  //next_key.mode = keysig_t::major;
  return !(key == next_key);
}

bool composer_t::process_timesig()
{
  time_signature_t old_timesig(timesig);
  if( ptimesigchange.rand() ){
    uint32_t tsidx(ptimesig.rand());
    timesig = time_signature_t(good_timesig[tsidx].num,good_timesig[tsidx].denom,time,0);
    DEBUG(timesig.numerator);
    DEBUG(timesig.denominator);
  }
  return !(old_timesig == timesig);
}

void composer_t::process_time()
{
  time += 1.0/256.0;
  lo_send(lo_addr,"/time","f",time);
  if( (timesig.beat(time) == 0) || ((timesig.numerator==0)&&(frac(timesig.beat(time))==0)) ){
    // new bar, optionally update key or time signature:
    if( process_key() )
      lo_send(lo_addr,"/key","fii",time+1,get_key(),get_mode());
    if( process_timesig() ){
      lo_send(lo_addr,"/timesig","fii",time+1,timesig.numerator,timesig.denominator);
    }
  }
  for(unsigned int k=0;k<5;k++){
    if( voice[k].note.end_time() <= time+1.0 ){
      voice[k].note.time = voice[k].note.end_time();
      voice[k].note.length = 3;
      voice[k].note.pitch = emit_pitch(k,0.9);
      lo_send(lo_addr,"/note","iiif",k,voice[k].note.pitch,voice[k].note.length,voice[k].note.time);
    }
  }
}

int main(int argc, char** argv)
{
  srandom(time(NULL));
  composer_t c("osc.udp://239.255.1.7:9877/");
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
