#include "libhos_music.h"
#include "hos_defs.h"
#include <math.h>
#include <tascar/errorhandling.h>

uint32_t closest_length(double len)
{
  if(len <= 0)
    return MAXLEN;
  uint32_t l2(0);
  while(len < duration(l2))
    l2++;
  if(l2 > MAXLEN)
    l2 = MAXLEN;
  return l2;
}

note_t::note_t() : pitch(0), length(0), time(-1000) {}

note_t::note_t(int32_t p, uint32_t len, double t)
    : pitch(p), length(len), time(t)
{
}

/**
 * @brief Return duration (measured in semibreve)
 *
 * length is:
 * 1: breve (2)
 * 3: semibreve (1)
 * 5: minim (1/2)
 * 7: crotchet (1/4)
 * 9: quaver (1/8)
 * 11: semiquaver (1/16)
 *
 * If the lowest bit is not set, then the duration is dotted value (times 3/2).
 */
double duration(uint32_t length)
{
  return 2.0 * (1.0 + 0.5 * (!(length & 1))) / (double)(1 << (length / 2));
}

double note_t::end_time() const
{
  return time + duration();
}

double note_t::duration() const
{
  return ::duration(length);
}

int32_t major[7] = {0, 2, 4, 5, 7, 9, 11};

int32_t wrapped_pitch(int32_t pitch)
{
  pitch = pitch % 12;
  while(pitch < 0)
    pitch += 12;
  return pitch;
}

bool is_in_scale(int32_t pitch, int32_t key)
{
  pitch = wrapped_pitch(pitch);
  for(uint32_t k = 0; k < 7; k++) {
    if(wrapped_pitch(major[k] + 7 * key) == pitch)
      return true;
  }
  return false;
}

int32_t closest_key(int32_t pitch, int32_t key)
{
  int32_t dkey(0);
  while(true) {
    if(is_in_scale(pitch, key + dkey))
      return key + dkey;
    if(is_in_scale(pitch, key - dkey))
      return key - dkey;
    dkey++;
  }
}

time_signature_t::time_signature_t()
    : numerator(4), denominator(4), starttime(0), addbar(0)
{
}

time_signature_t::time_signature_t(uint32_t hash_)
    : numerator(hash_ / 256), denominator(hash_ & 255), starttime(0), addbar(0)
{
}

time_signature_t::time_signature_t(uint32_t num, uint32_t denom, double startt,
                                   uint32_t addb)
    : numerator(num), denominator(denom), starttime(startt), addbar(addb)
{
}

uint32_t time_signature_t::hash() const
{
  return 256 * numerator + denominator;
}

/**
   \brief Convert a time value into the quantized beat value
 */
double time_signature_t::beat(double time) const
{
  if(numerator == 0)
    return 1.0 * (time - starttime) * denominator;
  return frac(bar(time), numerator * 8192) * numerator;
}

/**
   \brief Convert a time value into a fractional bar number
 */
double time_signature_t::bar(double time) const
{
  if(numerator == 0)
    return 0;
  return ((time - starttime) * (double)denominator) / (double)numerator +
         (double)addbar;
}

double time_signature_t::time(double bar) const
{
  if(denominator == 0)
    return starttime;
  return 1.0 * (bar - (double)addbar) * (double)numerator /
             (double)denominator +
         starttime;
}

keysig_t::keysig_t() : fifths(0), mode(major) {}

keysig_t::keysig_t(const std::string& name)
{
  if(name.size() == 0)
    throw TASCAR::ErrMsg("key signature name is empty");
  uint32_t fc(0);
  if(name[name.size() - 1] == 'm') {
    mode = minor;
    std::string names[] = {"ebm", "bbm", "fm",  "cm",  "gm",  "dm",  "am",
                           "em",  "bm",  "f#m", "c#m", "g#m", "d#m", ""};
    while(names[fc].size() && (names[fc] != name))
      fc++;
    if(!names[fc].size())
      throw TASCAR::ErrMsg("Unknown key signature name \"" + name + "\".");
    fifths = (int32_t)fc - 6;
  } else {
    mode = major;
    std::string names[] = {"Gb", "Db", "Ab", "Eb", "Bb", "F",  "C",
                           "G",  "D",  "A",  "E",  "B",  "F#", ""};
    while(names[fc].size() && (names[fc] != name))
      fc++;
    if(!names[fc].size())
      throw TASCAR::ErrMsg("Unknown key signature name \"" + name + "\".");
    fifths = (int32_t)fc - 6;
  }
}

keysig_t::keysig_t(uint32_t hash)
    : fifths((int32_t)(hash & 15) - 7), mode((hash & 64) ? minor : major)
{
}

uint32_t keysig_t::hash() const
{
  return 64 * (mode == minor) + (fifths + 7);
}

keysig_t::keysig_t(int p, mode_t m)
{
  setpitch(p, m);
}

int32_t keysig_t::pitch() const
{
  int32_t p(fifths * 7);
  if(mode == minor)
    p -= 3;
  return wrapped_pitch(p);
}

keysig_t& keysig_t::operator+=(const keysigchange_t& kc)
{
  fifths += kc.fifths;
  if(kc.parallel) {
    if(mode == major)
      mode = minor;
    else
      mode = major;
  }
  return *this;
}

void keysig_t::setpitch(int32_t p, mode_t m)
{
  if(m == minor)
    p += 3;
  p = wrapped_pitch(p);
  int32_t p0(0);
  while(wrapped_pitch(p0 * 7) != p)
    p0++;
  if(p0 >= 6)
    p0 -= 12;
  fifths = p0;
  mode = m;
}

std::string notename(int32_t pitch)
{
  pitch = wrapped_pitch(pitch);
  switch(pitch) {
  case 0:
    return "c";
  case 1:
    return "c#";
  case 2:
    return "d";
  case 3:
    return "d#";
  case 4:
    return "e";
  case 5:
    return "f";
  case 6:
    return "f#";
  case 7:
    return "g";
  case 8:
    return "g#";
  case 9:
    return "a";
  case 10:
    return "bb";
  case 11:
    return "b";
  }
  return "bug";
}

std::string keysig_t::name() const
{
  std::string major_name[] = {"error (b)", "Gb", "Db", "Ab", "Eb",
                              "Bb",        "F",  "C",  "G",  "D",
                              "A",         "E",  "B",  "F#", "error (#)"};
  std::string minor_name[] = {"error (b)", "ebm", "bbm", "fm",  "cm",
                              "gm",        "dm",  "am",  "em",  "bm",
                              "f#m",       "c#m", "g#m", "d#m", "error (#)"};
  if(mode == major)
    return major_name[std::max(0, std::min(fifths + 7, 14))];
  else
    return minor_name[std::max(0, std::min(fifths + 7, 14))];
}

double frac(double x, double eps)
{
  x = x - floor(x);
  x = round(x * eps) / eps;
  if(x == 1)
    x = 0;
  return x;
}

keysigchange_t::keysigchange_t() : fifths(0), parallel(false) {}

keysigchange_t::keysigchange_t(int32_t f, bool p) : fifths(f), parallel(p) {}

keysigchange_t::keysigchange_t(uint32_t hash)
    : fifths((int32_t)(hash & 15) - 7), parallel(hash & 64)
{
}

uint32_t keysigchange_t::hash() const
{
  return 64 * parallel + (fifths + 7);
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
