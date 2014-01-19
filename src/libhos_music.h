#ifndef LIBHOS_MUSIC_H
#define LIBHOS_MUSIC_H

#include <iostream>

class note_t {
public:
  note_t();
  int pitch;
  unsigned int length;
  double time;
  double end_time();
  double duration();
  friend std::ostream& operator<<(std::ostream& o, const note_t& p){  o << "[" << p.pitch << ":"<<p.length << "@" << p.time << "]"; return o;};
};

int wrapped_pitch(int pitch);
bool is_in_scale(int pitch, int key);
int closest_key(int pitch, int key);

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
