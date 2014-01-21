#include "libhos_music.h"

note_t::note_t()
 :pitch(0),length(0),time(-1000)
{
}

double note_t::end_time()
{
  return time+duration();
}

double note_t::duration()
{
  return 1.0/(double)(1<<length);
}

int major[7] = {0,2,4,5,7,9,11};

int wrapped_pitch(int pitch)
{
  pitch = pitch % 12;
  while( pitch < 0 )
    pitch += 12;
  return pitch;
}

bool is_in_scale(int pitch, int key)
{
  pitch = wrapped_pitch(pitch);
  for(unsigned int k=0;k<7;k++){
    if( wrapped_pitch(major[k]+7*key) == pitch )
      return true;
  }
  return false;
}

int closest_key(int pitch, int key)
{
  int dkey(0);
  while( true ){
    if( is_in_scale( pitch, key-dkey ) )
      return key-dkey;
    if( is_in_scale( pitch, key+dkey ) )
      return key+dkey;
    dkey++;
  }
}

time_signature_t::time_signature_t()
  : nominator(4),denominator(4)
{
}

double time_signature_t::bar(double time)
{
  if( nominator == 0 )
    return 0;
  return 2.0*time*denominator/nominator;
}


double time_signature_t::time(double bar)
{
  if( denominator == 0 )
    return 0;
  return 0.5*bar*nominator/denominator;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
