#ifndef LIBHOS_MUSIC_H
#define LIBHOS_MUSIC_H

#include <iostream>
#include <string>
#include <vector>
#include <stdint.h>

int32_t wrapped_pitch(int32_t pitch);
bool is_in_scale(int32_t pitch, int32_t key);
int32_t closest_key(int32_t pitch, int32_t key);
std::string notename(int32_t pitch);
uint32_t closest_length(double len);

#define PITCH_REST 1000

class note_t {
public:
  note_t();
  int32_t pitch;
  uint32_t length;
  double time;
  double end_time() const;
  double duration() const;
  friend std::ostream& operator<<(std::ostream& o, const note_t& p){  o << "[" << p.pitch << ":"<<p.length << "@" << p.time << "]"; return o;};
};

class keysig_t {
public:
  enum mode_t {
    major, minor
  };
  keysig_t();
  int32_t pitch() const;
  void setpitch(int p,mode_t m);
  int32_t fifths;
  mode_t mode;
  std::string name() const;
  friend std::ostream& operator<<(std::ostream& o, const keysig_t& p){  o << p.name(); return o;};
};

class time_signature_t {
public:
  time_signature_t();
  time_signature_t(double nom,double denom,double startt,uint32_t addb);
  double nominator;
  double denominator;
  double starttime;
  uint32_t addbar;
  double bar(double time);
  double time(double bar);
  friend std::ostream& operator<<(std::ostream& o, const time_signature_t& p){  o << "[time " << p.nominator << "/"<<p.denominator << "@" << p.starttime << "+" << p.addbar << "]"; return o;};
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
