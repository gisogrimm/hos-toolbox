#include "libhos_music.h"
#include <math.h>

uint32_t closest_length(double len)
{
  if( len <= 0 )
    return MAXLEN;
  uint32_t l2(0);
  while( len < (2.0/(double)(1<<l2)) )
    l2++;
  if( l2 > MAXLEN )
    l2 = MAXLEN;
  return l2;
}

note_t::note_t()
 :pitch(0),length(0),time(-1000)
{
}


double duration(uint32_t length)
{
  return 2.0/(double)(1<<length);
}

double note_t::end_time() const
{
  return time+duration();
}

double note_t::duration() const
{
  return ::duration(length);
}

int32_t major[7] = {0,2,4,5,7,9,11};

int32_t wrapped_pitch(int32_t pitch)
{
  pitch = pitch % 12;
  while( pitch < 0 )
    pitch += 12;
  return pitch;
}

bool is_in_scale(int32_t pitch, int32_t key)
{
  pitch = wrapped_pitch(pitch);
  for(uint32_t k=0;k<7;k++){
    if( wrapped_pitch(major[k]+7*key) == pitch )
      return true;
  }
  return false;
}

int32_t closest_key(int32_t pitch, int32_t key)
{
  int32_t dkey(0);
  while( true ){
    if( is_in_scale( pitch, key+dkey ) )
      return key+dkey;
    if( is_in_scale( pitch, key-dkey ) )
      return key-dkey;
    dkey++;
  }
}

time_signature_t::time_signature_t()
  : numerator(4),denominator(4),starttime(0),addbar(0)
{
}

time_signature_t::time_signature_t(uint32_t num,uint32_t denom,double startt,uint32_t addb)
  : numerator(num),denominator(denom),starttime(startt),addbar(addb)
{
}


double time_signature_t::beat(double time) const
{
  if( numerator == 0 )
    return 1.0*(time-starttime)*denominator;
  return frac(bar(time))*numerator;
}

double time_signature_t::bar(double time) const
{
  if( numerator == 0 )
    return 0;
  return 1.0*(time-starttime)*(double)denominator/(double)numerator + (double)addbar;
}


double time_signature_t::time(double bar) const
{
  if( denominator == 0 )
    return starttime;
  return 1.0*(bar-(double)addbar)*(double)numerator/(double)denominator+starttime;
}

keysig_t::keysig_t()
  : fifths(0),
    mode(major)
{
}

keysig_t::keysig_t(int p, mode_t m)
{
  setpitch(p,m);
}

int32_t keysig_t::pitch() const
{
  int32_t p(fifths*7);
  if( mode == minor )
    p -= 3;
  return wrapped_pitch(p);
}

void keysig_t::setpitch(int32_t p,mode_t m)
{
  if( m == minor )
    p += 3;
  p = wrapped_pitch(p);
  int32_t p0(0);
  while(wrapped_pitch(p0*7) != p)
    p0++;
  if( p0 >= 6 )
    p0 -= 12;
  fifths = p0;
  mode = m;
}

std::string notename(int32_t pitch)
{
  pitch = wrapped_pitch(pitch);
  switch( pitch ){
  case 0 : 
    return "c";
  case 1 :
    return "c#";
  case 2 :
    return "d";
  case 3 : 
    return  "d#"; 
  case 4 : 
    return  "e"; 
  case 5 : 
    return  "f"; 
  case 6 : 
    return  "f#"; 
  case 7 : 
    return  "g"; 
  case 8 : 
    return  "g#"; 
  case 9 : 
    return  "a"; 
  case 10 : 
    return  "bb"; 
  case 11 : 
    return  "b";
  }
  return "bug";
}

std::string keysig_t::name() const
{
  std::string major_name[] = {"error (b)","Gb","Db","Ab","Eb","Bb","F","C","G","D","A","E","B","F#","error (#)"};
  std::string minor_name[] = {"error (b)","ebm","bbm","fm","cm","gm","dm","am","em","bm","f#m","c#m","g#m","d#m","error (#)"};
  if( mode == major )
    return major_name[std::max(0,std::min(fifths+7,14))];
  else
    return minor_name[std::max(0,std::min(fifths+7,14))];
}

double frac(double x)
{
  return x-floor(x);
}


/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
