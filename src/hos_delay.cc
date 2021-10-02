/**
   \file hos_delay.cc
   \ingroup apphos
   \brief Time varying delay

   \author Giso Grimm
   \date 2013

   \section license License (GPL)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

*/

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <tascar/jackclient.h>
#include <tascar/osc_helper.h>
#include <unistd.h>

#define MAXDELAY 480000
#define OSC_ADDR "239.255.1.7"
#define OSC_PORT "6978"

namespace HoS {

  class rvalue_t {
  public:
    rvalue_t();
    void copy(rvalue_t& src);
    void set(double v, double t);
    inline void step()
    {
      if(time > 0) {
        current += (val - current) / time;
        time -= 1.0;
      } else {
        current = val;
      }
    };
    double val;
    double time;
    bool applied;
    double current;
  };

  /**
     \ingroup apphos
  */
  class delay_t : public jackc_t, public TASCAR::osc_server_t {
  public:
    delay_t(const std::string& name);
    ~delay_t();
    void set_delay(double delay, double ttime);
    void set_gain(double g, double t);
    void run();
    void quit() { b_quit = true; };
    static int osc_set_gain(const char* path, const char* types, lo_arg** argv,
                            int argc, lo_message msg, void* user_data);
    static int osc_set_delay(const char* path, const char* types, lo_arg** argv,
                             int argc, lo_message msg, void* user_data);
    static int osc_quit(const char* path, const char* types, lo_arg** argv,
                        int argc, lo_message msg, void* user_data);

  private:
    int process(jack_nframes_t nframes, const std::vector<float*>& inBuffer,
                const std::vector<float*>& outBuffer);
    double dt;
    float delayline[MAXDELAY];
    rvalue_t osc_delay;
    rvalue_t delay;
    rvalue_t osc_gain;
    rvalue_t gain;
    // double target_delay;
    // double target_time;
    // double osc_delay;
    // double osc_targettime;
    // double current_delay;
    uint32_t in_pos;
    uint32_t out_pos;
    bool b_quit;
    pthread_mutex_t mutex;
  };

} // namespace HoS

using namespace HoS;

rvalue_t::rvalue_t() : val(0), time(0), applied(false), current(0) {}

void rvalue_t::copy(rvalue_t& src)
{
  val = src.val;
  time = src.time;
  src.applied = true;
}

void rvalue_t::set(double v, double t)
{
  val = v;
  time = t;
  applied = false;
}

delay_t::delay_t(const std::string& name)
    : jackc_t(name), TASCAR::osc_server_t(OSC_ADDR, OSC_PORT, "UDP"),
      dt(1.0 / srate),
      // target_delay(0.0),
      // target_time(0.0),
      // osc_delay(0.0),
      // osc_targettime(0.0),
      // current_delay(0.0),
      in_pos(0), out_pos(0), b_quit(false)
{
  osc_gain.val = 1.0;
  osc_gain.time = 1.0;
  pthread_mutex_init(&mutex, NULL);
  memset(delayline, 0, sizeof(float) * MAXDELAY);
  add_input_port("in");
  add_output_port("out");
  set_prefix("/" + name);
  add_method("/delay", "ff", delay_t::osc_set_delay, this);
  add_method("/gain", "ff", delay_t::osc_set_gain, this);
  add_method("/quit", "", delay_t::osc_quit, this);
}

delay_t::~delay_t()
{
  pthread_mutex_trylock(&mutex);
  pthread_mutex_unlock(&mutex);
  pthread_mutex_destroy(&mutex);
}

int delay_t::osc_set_delay(const char* path, const char* types, lo_arg** argv,
                           int argc, lo_message msg, void* user_data)
{
  if((user_data) && (argc == 2) && (types[0] == 'f') && (types[1] == 'f')) {
    ((delay_t*)user_data)->set_delay(argv[0]->f, argv[1]->f);
  }
  return 0;
}

int delay_t::osc_set_gain(const char* path, const char* types, lo_arg** argv,
                          int argc, lo_message msg, void* user_data)
{
  if((user_data) && (argc == 2) && (types[0] == 'f') && (types[1] == 'f')) {
    ((delay_t*)user_data)->set_gain(argv[0]->f, argv[1]->f);
  }
  return 0;
}

int delay_t::osc_quit(const char* path, const char* types, lo_arg** argv,
                      int argc, lo_message msg, void* user_data)
{
  if(user_data)
    ((delay_t*)user_data)->quit();
  return 0;
}

void delay_t::set_delay(double delay, double ttime)
{
  pthread_mutex_lock(&mutex);
  osc_delay.set(delay * srate, ttime * srate);
  pthread_mutex_unlock(&mutex);
}

void delay_t::set_gain(double g, double t)
{
  pthread_mutex_lock(&mutex);
  osc_gain.set(g, t * srate);
  pthread_mutex_unlock(&mutex);
}

int delay_t::process(jack_nframes_t nframes,
                     const std::vector<float*>& inBuffer,
                     const std::vector<float*>& outBuffer)
{
  // set_preset();
  float* v_in(inBuffer[0]);
  float* v_out(outBuffer[0]);
  if(pthread_mutex_trylock(&mutex) == 0) {
    if(!osc_delay.applied)
      delay.copy(osc_delay);
    if(!osc_gain.applied)
      gain.copy(osc_gain);
    pthread_mutex_unlock(&mutex);
  }
  // main loop:
  for(jack_nframes_t i = 0; i < nframes; ++i) {
    gain.step();
    delay.step();
    delay.current = std::min(std::max(delay.current, 0.0), MAXDELAY - 1.0);
    out_pos = in_pos + delay.current;
    while(out_pos >= MAXDELAY)
      out_pos -= MAXDELAY;
    delayline[in_pos] = v_in[i];
    v_out[i] = gain.current * delayline[out_pos];
    if(!in_pos)
      in_pos = MAXDELAY;
    in_pos--;
  }
  return 0;
}

void delay_t::run()
{
  TASCAR::osc_server_t::activate();
  jackc_t::activate();
  while(!b_quit) {
    sleep(1);
  }
  jackc_t::deactivate();
  TASCAR::osc_server_t::deactivate();
}

int main(int argc, char** argv)
{
  std::string name("delay");
  if(argc > 1)
    name = argv[1];
  delay_t S(name);
  S.run();
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
