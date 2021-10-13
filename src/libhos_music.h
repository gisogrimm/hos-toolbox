#ifndef LIBHOS_MUSIC_H
#define LIBHOS_MUSIC_H

#include <iostream>
#include <stdint.h>
#include <string>
#include <vector>

int32_t wrapped_pitch(int32_t pitch);
bool is_in_scale(int32_t pitch, int32_t key);
int32_t closest_key(int32_t pitch, int32_t key);
std::string notename(int32_t pitch);
uint32_t closest_length(double len);
double duration(uint32_t len);
double frac(double x, double eps = 8192.0);

#define PITCH_REST 1000
#define MAXLEN 11

/**
   \ingroup rtm
 */
class note_t {
public:
  note_t();
  note_t(int32_t p, uint32_t len, double time = 0);
  int32_t pitch;
  uint32_t length;
  double time;
  double end_time() const;
  double duration() const;
  friend std::ostream& operator<<(std::ostream& o, const note_t& p)
  {
    o << "[" << p.pitch << ":" << p.length << "@" << p.time << "]";
    return o;
  };
};

/**
   \ingroup rtm
*/
class keysigchange_t {
public:
  keysigchange_t();
  keysigchange_t(int32_t f, bool p);
  keysigchange_t(uint32_t hash);
  uint32_t hash() const;
  int32_t fifths;
  bool parallel;
};

/**
   \ingroup rtm
 */
class keysig_t {
public:
  enum mode_t { major, minor };
  keysig_t();
  keysig_t(const std::string& name);
  keysig_t(uint32_t hash);
  keysig_t(int p, mode_t m);
  int32_t pitch() const;
  void setpitch(int p, mode_t m);
  int32_t fifths;
  mode_t mode;
  std::string name() const;
  friend std::ostream& operator<<(std::ostream& o, const keysig_t& p)
  {
    o << p.name();
    return o;
  };
  bool operator==(const keysig_t& o) const
  {
    return (fifths == o.fifths) && (mode == o.mode);
  };
  keysig_t& operator+=(const keysigchange_t&);
  uint32_t hash() const;
};

/**
   \brief A class to deal with bars and beats based on a semi-contiuous time
   \ingroup rtm
*/
class time_signature_t {
public:
  time_signature_t();
  time_signature_t(uint32_t num, uint32_t denom, double startt, uint32_t addb);
  time_signature_t(uint32_t hash_);
  uint32_t hash() const;
  uint32_t numerator;
  uint32_t denominator;
  double
      starttime;   ///< Start time from which on the new time signature is value
  uint32_t addbar; ///< Number of bars which were counted before the current
                   ///< time signature
  double bar(double time) const;
  double beat(double time) const;
  double time(double bar) const;
  bool unmeasured() const { return numerator == 0; };
  friend std::ostream& operator<<(std::ostream& o, const time_signature_t& p)
  {
    o << "[time " << p.numerator << "/" << p.denominator << "@" << p.starttime
      << "+" << p.addbar << "]";
    return o;
  };
  bool operator==(const time_signature_t& o) const
  {
    return (numerator == o.numerator) && (denominator == o.denominator);
  };
};

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
