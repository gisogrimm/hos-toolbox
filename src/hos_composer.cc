#include <lo/lo.h>
#include <stdlib.h>
#include <unistd.h>
#include "libhos_music.h"
#include "libhos_random.h"
#include "defs.h"
#include <math.h>

class simple_timesig_t {
public:
  uint32_t nom;
  uint32_t denom;
};

#define GOODTIMESIG 4

simple_timesig_t good_timesig[GOODTIMESIG] = { {4,2}, {11,4}, {5,2}, {3,4} };

double gauss(double x, double sigma )
{
  return 1.0/sqrt(2.0*M_PI*sigma*sigma)*exp(-(x*x)/(2*sigma*sigma));
}

class ambitus_treble_t : public pdf_t {
public:
  ambitus_treble_t(){
    // usable ambitus ranges from g to a'':
    // preferred range around c''
    for(int32_t p=-5; p<22;p++)
      set(p,gauss(p-12,8));
    //// prefer empty strings from c on:
    //operator[](0) += 1.0;
    //operator[](4) += 1.0;
    //operator[](9) += 1.0;
    //operator[](14) += 1.0;
    update();
  };
};

class ambitus_bass_t : public pdf_t {
public:
  ambitus_bass_t(){
    // usable ambitus ranges from D to a':
    // preferred range around c
    for(int32_t p=-22; p<10;p++)
      set(p,gauss(p+17,8));
    //// prefer empty strings:
    //operator[](-22) += 1.0;//D
    //operator[](-17) += 1.0;//G
    //operator[](-12) += 1.0;//c
    //operator[](-8) += 1.0; //e
    //operator[](-3) += 1.0; //a
    //operator[](2) += 1.0;  //d'
    update();
  };
};

class ambitus_tenor_t : public pdf_t {
public:
  ambitus_tenor_t(){
    // usable ambitus ranges from D to c'':
    // preferred range around d'
    for(int32_t p=-17; p<13;p++)
      set(p,gauss(p-2,8));
    //// prefer empty strings:
    //operator[](-22) += 1.0;//D
    //operator[](-17) += 1.0;//G
    //operator[](-12) += 1.0;//c
    //operator[](-8) += 1.0; //e
    //operator[](-3) += 1.0; //a
    //operator[](2) += 1.0;  //d'
    update();
  };
};

namespace Ambitus {
  
  const ambitus_treble_t treble;
  const ambitus_tenor_t tenor;
  const ambitus_bass_t bass;

}

class major_scale_t : public pdf_t {
public:
  major_scale_t();
};

class major_triad_t : public pdf_t {
public:
  major_triad_t();
};

class minor_scale_t : public pdf_t {
public:
  minor_scale_t();
};

class minor_triad_t : public pdf_t {
public:
  minor_triad_t();
};

major_scale_t::major_scale_t()
{
  for(int oct=-10;oct<=10;oct++){
    set(0+12*oct,1);
    set(2+12*oct,1);
    set(4+12*oct,1); // major third
    set(5+12*oct,1);
    set(7+12*oct,1); // fifth
    set(9+12*oct,1);
    set(11+12*oct,1); // major seven
  }
  update();
}

major_triad_t::major_triad_t()
{
  for(int oct=-10;oct<=10;oct++){
    set(0+12*oct,1);
    set(4+12*oct,1); // major third
    set(7+12*oct,1); // fifth
  }
  update();
}


minor_scale_t::minor_scale_t()
{
  for(int oct=-10;oct<=10;oct++){
    set(0+12*oct,1);
    set(2+12*oct,1);
    set(3+12*oct,1); // minor third
    set(5+12*oct,1);
    set(7+12*oct,1); // fifth
    set(9+12*oct,1);
    set(10+12*oct,1); // minor seven
    set(11+12*oct,1); // major seven
  }
  update();
}

minor_triad_t::minor_triad_t()
{
  for(int oct=-10;oct<=10;oct++){
    set(0+12*oct,1);
    set(3+12*oct,1); // minor third
    set(7+12*oct,1); // fifth
  }
  update();
}

namespace Scales {
  major_triad_t major_triad;
  major_scale_t major_scale;
  minor_triad_t minor_triad;
  minor_scale_t minor_scale;
}

class composer_t {
public:
  composer_t(uint32_t num_voices=5);
  void select_key();
  int emit_pitch(uint32_t voice);
  int32_t get_key() const;
  int32_t get_mode() const;
private:
  std::vector<pdf_t> ambitus;
  std::vector<int32_t> pitch;
  keysig_t key;
  keysig_t next_key;
};

int32_t composer_t::get_key() const
{
  return key.pitch();
}

int32_t composer_t::get_mode() const
{
  return key.mode;
}

composer_t::composer_t(uint32_t num_voices)
{
  ambitus.push_back(Ambitus::treble);
  ambitus.push_back(Ambitus::treble);
  ambitus.push_back(Ambitus::tenor);
  ambitus.push_back(Ambitus::tenor);
  ambitus.push_back(Ambitus::bass);
  for(uint32_t k=0;k<ambitus.size();k++)
    pitch.push_back(ambitus[k].rand());
}

int composer_t::emit_pitch(uint32_t voice)
{
  pdf_t scale;
  double triad_w((voice==4)*0.8);
  //triad_w = 0.8;
  if( key.mode == keysig_t::major ){
    scale = (Scales::major_triad*triad_w) + (Scales::major_scale*(1.0-triad_w));
  }else{
    scale = (Scales::minor_triad*triad_w) + (Scales::minor_scale*(1.0-triad_w));
  }
  if( key.mode == keysig_t::major ){
    scale = Scales::major_triad;
  }else{
    scale = Scales::minor_triad;
  }
  //DEBUG(key.pitch());
  scale = scale.vadd(key.pitch());
  scale.update();
  //DEBUG(key.name());
  pdf_t note_change;
  for(int32_t n=-80;n<81;n++)
    note_change.set(n,gauss(n,4));
  note_change = note_change.vadd(pitch[voice]);
  note_change.update();
  scale = scale * ambitus[voice] * note_change;
  pitch[voice] = scale.rand();
  pdf_t rest;
  rest.set(0,8);
  rest.set(1,1);
  rest.update();
  if( rest.rand() )
    pitch[voice] = PITCH_REST;
  return pitch[voice];
}

void composer_t::select_key()
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
  keysig_taste.set(-5,1);
  keysig_taste.set(-4,2);
  keysig_taste.set(-3,3);
  keysig_taste.set(-2,4);
  keysig_taste.set(-1,5);
  keysig_taste.set(0,4);
  keysig_taste.set(1,4);
  keysig_taste.set(2,3);
  keysig_taste.set(3,3);
  keysig_taste.set(4,2);
  keysig_taste.set(5,1);
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
  DEBUG(next_key.name());
}

int main(int argc, char** argv)
{
  composer_t c;
  lo_address lo_addr(lo_address_new_from_url( "osc.udp://239.255.1.7:9877/" ));
  lo_address_set_ttl( lo_addr, 1 );
  double time(0);
  note_t n[5];
  for(unsigned int k=0;k<5;k++){
    n[k].time = 0;
  }
  lo_send(lo_addr,"/clear","");
  std::vector<pdf_t> lenpdf;
  lenpdf.resize(5);
  for(uint32_t k=0;k<7;k++){
    lenpdf[0].set(k,gauss(k-2,2));
    lenpdf[1].set(k,gauss(k-2,2));
    lenpdf[2].set(k,gauss(k-1,1));
    lenpdf[3].set(k,gauss(k-1,1));
    lenpdf[4].set(k,gauss(k-1,2));
  }
  lenpdf[4].set(3,1);
  for(uint32_t k=0;k<lenpdf.size();k++)
    lenpdf[k].update();
  pdf_t timesig;
  for(uint32_t t=0;t<GOODTIMESIG;t++)
    timesig.set(t,gauss(t,4));
  timesig.update();
  pdf_t timesigchange;
  timesigchange.set(0,2);
  timesigchange.set(1,1);
  timesigchange.update();
  uint32_t otsidx(-1);
  while(true){
    usleep(15625);
    time += 1.0/256.0;
    lo_send(lo_addr,"/time","f",time);
    if( fabsf(2.0*time - floor(2.0*time)) < 0.001 ){
      c.select_key();
      lo_send(lo_addr,"/key","fii",time+1,c.get_key(),c.get_mode());
      if( timesigchange.rand() ){
        uint32_t tsidx(timesig.rand());
        if( otsidx != tsidx )
          lo_send(lo_addr,"/timesig","fii",time+1,good_timesig[tsidx].nom,good_timesig[tsidx].denom);
        otsidx = tsidx;
      }
    }
    for(unsigned int k=0;k<5;k++){
      if( n[k].end_time() <= time+1.0 ){
        n[k].time = n[k].end_time();
        //DEBUG(n[k].time);
        //DEBUG(n[k].time-floor(n[k].time));
        //n[k].length = 7.0*rand()/RAND_MAX;
        n[k].length = lenpdf[k].rand();
        n[k].length = 3;
        n[k].pitch = c.emit_pitch(k);
        //(12.0*rand()/RAND_MAX-6.0);
        //n[k].pitch += 0.3*(c_pitch[k]-n[k].pitch)*rand()/RAND_MAX;
        lo_send(lo_addr,"/note","iiif",k,n[k].pitch,n[k].length,n[k].time);
        //std::cout << "k=" << k << " " << n[k] << std::endl;
      }
    }
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
