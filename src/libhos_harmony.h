#ifndef LIBHOS_HARMONY_H
#define LIBHOS_HARMONY_H

#include "libhos_music.h"
#include "libhos_random.h"

class ambitus_t {
public:
  ambitus_t();
  pdf_t treble;
  pdf_t tenor;
  pdf_t bass;
  pdf_t fezzo;
};

class scale_t {
public:
  scale_t();
  const pdf_t& operator[](keysig_t::mode_t m) const;
  pdf_t major;
  pdf_t minor;
};

class triad_t {
public:
  triad_t();
  const pdf_t& operator[](keysig_t::mode_t m) const;
  pdf_t major;
  pdf_t minor;
};

const ambitus_t Ambitus;
const scale_t Scale;
const triad_t Triad;

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
