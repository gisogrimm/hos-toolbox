#include "osc_helper.h"
#include <alsa/asoundlib.h>
#include <alsa/seq_event.h>
#include <iostream>
#include <libxml++/libxml++.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "libhos_midi_ctl.h"

#define DEBUG(x)                                                               \
  std::cerr << __FILE__ << ":" << __LINE__ << " " << #x << "=" << x << std::endl
#define MIDISC 0.0078740157480314959537182f

class midi_client_t : public midi_ctl_t {
public:
  midi_client_t(const std::string& name);
  ~midi_client_t();

protected:
  void emit_event(int channel, int param, int value);
};

midi_client_t::midi_client_t(const std::string& name) : midi_ctl_t(name) {}

midi_client_t::~midi_client_t() {}

void midi_client_t::emit_event(int channel, int param, int value)
{
  std::cerr << "ch=" << channel << " par=" << param << " val=" << value
            << std::endl;
}

int main(int argc, char** argv)
{
  midi_client_t s("debug");
  s.start_service();
  s.connect_input(20, 32);
  while(true)
    sleep(1);
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
