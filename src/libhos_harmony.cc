#include "libhos_harmony.h"

ambitus_t::ambitus_t()
{
  // treble: d=-10 g=-5 c'=0 e'=3 a'=9 d''=14 (bb''=22)
  for(int32_t p=-10; p<22;p++)
    treble.set(p,gauss(p-12,8));
  treble.update();
  // alto: G=-17 g'=7 (ees''=15)
  for(int32_t p=-17; p<15;p++)
    tenor.set(p,gauss(p-2,8));
  tenor.update();
  // bass: D=-22 d'=2 (bb'=10)
  for(int32_t p=-22; p<10;p++)
    bass.set(p,gauss(p+12,8));
  bass.update();
  //  fezzo: GG=-29 g=-5 (ees'=3)
  for(int32_t p=-29; p<3;p++)
    fezzo.set(p,gauss(p+17,8));
  fezzo.update();
}

scale_t::scale_t()
{
  for(int oct=-10;oct<=10;oct++){
    major.set(0+12*oct,1);
    major.set(2+12*oct,1);
    major.set(4+12*oct,1); // major third
    major.set(5+12*oct,1);
    major.set(7+12*oct,1); // fifth
    major.set(9+12*oct,1);
    major.set(11+12*oct,1); // major seven
    minor.set(0+12*oct,1);
    minor.set(2+12*oct,1);
    minor.set(3+12*oct,1); // minor third
    minor.set(5+12*oct,1);
    minor.set(7+12*oct,1); // fifth
    minor.set(9+12*oct,1);
    minor.set(10+12*oct,1); // minor seven
    //minor.set(11+12*oct,1); // major seven
  }
  major.update();
  minor.update();
}

triad_t::triad_t()
{
  for(int oct=-10;oct<=10;oct++){
    minor.set(0+12*oct,1);
    minor.set(3+12*oct,1); // minor third
    minor.set(7+12*oct,1); // fifth
    major.set(0+12*oct,1);
    major.set(4+12*oct,1); // major third
    major.set(7+12*oct,1); // fifth
  }
  major.update();
  minor.update();
}

const pdf_t& scale_t::operator[](keysig_t::mode_t m) const
{
  switch( m ){
  case keysig_t::major:
    return major;
  case keysig_t::minor:
    return minor;
  }
  return major;
}

const pdf_t& triad_t::operator[](keysig_t::mode_t m) const
{
  switch( m ){
  case keysig_t::major:
    return major;
  case keysig_t::minor:
    return minor;
  }
  return major;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
