#ifndef LIBHOS_HARMONY_H
#define LIBHOS_HARMONY_H

#include "libhos_music.h"
#include "libhos_random.h"
#include <libxml++/libxml++.h>

double get_attribute_double(xmlpp::Element* e,const std::string& name);

class ambitus_t {
public:
  ambitus_t();
  pmf_t treble;
  pmf_t tenor;
  pmf_t bass;
  pmf_t fezzo;
};

class scale_t {
public:
  scale_t();
  const pmf_t& operator[](keysig_t::mode_t m) const;
  pmf_t major;
  pmf_t minor;
};

class triad_t {
public:
  triad_t();
  const pmf_t& operator[](keysig_t::mode_t m) const;
  pmf_t major;
  pmf_t minor;
};

const ambitus_t Ambitus;
const scale_t Scale;
const triad_t Triad;

class harmony_model_t {
public:
  bool process(double beat);
  const keysig_t& current() const;
  const keysig_t& next() const;
  void read_xml(xmlpp::Element* e);
  pmf_t notes(double triadw) const;
private:
  void update_tables();
  keysig_t key_current;
  keysig_t key_next;
  pmf_t pkey;
  pmf_t pchange;
  pmf_t pbeat;
  std::map<uint32_t,pmf_t> pkeyrel;
};

class melody_model_t {
public:
  note_t process(double beat,const harmony_model_t& harmony, const time_signature_t& timesig);
  void read_xml(xmlpp::Element* e);
private:
  pmf_t pambitus;
  pmf_t pstep;
  pmf_t pduration;
  pmf_t pbeat;
  int32_t last_pitch;
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
