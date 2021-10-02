/**
   \file mm_midicc.cc
   \ingroup apphos
   \brief Matrix mixer MIDI control interface
   \author Giso Grimm
   \date 2011

  "mm_{hdsp,file,gui,midicc}" is a set of programs to control the matrix
  mixer of an RME hdsp compatible sound card using XML formatted files
  or a MIDI controller, and to visualize the mixing matrix.

  \section license License (GPL)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; version 2
  of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
  USA.

*/
#include "libhos_gainmatrix.h"
#include "libhos_midi_ctl.h"
#include <map>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// todo: output-link:
// if( (kout == link_dest) -> return pan and gain from link_src
// control: pan-put button for link_dest, button-row for link_src

void osc_err_handler_cb(int num, const char* msg, const char* where)
{
  std::cerr << "lo error " << num << ": " << msg << " (" << where << ")\n";
}

namespace MM {

  /**
  \ingroup apphos
  */
  class mm_midicc_t : public midi_ctl_t {
  public:
    mm_midicc_t(const std::string& osc_server_addr,
                const std::string& osc_server_port,
                const std::string& osc_dest_addr);
    ~mm_midicc_t();
    float midi2gain(unsigned int v);
    unsigned int gain2midi(float v);
    float midi2pan(unsigned int v);
    unsigned int pan2midi(float v);
    float midi2pan2(unsigned int v);
    unsigned int pan22midi(float v);
    // void create_hos_mapping();
    void run();

  private:
    virtual void emit_event(int channel, int param, int value);
    static int hdspmm_new(const char* path, const char* types, lo_arg** argv,
                          int argc, lo_message msg, void* user_data);
    static int hdspmm_destroy(const char* path, const char* types,
                              lo_arg** argv, int argc, lo_message msg,
                              void* user_data);
    void hdspmm_destroy();
    void hdspmm_new(unsigned int kout, unsigned int kin);

    void upload();

    void start_updt_service();
    void stop_updt_service();
    static void* updt_service(void*);
    void updt_service();

    MM::lo_matrix_t* mm;
    bool modified;
    pthread_mutex_t mutex;

    bool b_shift1;
    bool b_shift2;
    unsigned int selected_out;
    bool b_exit;

    lo_server_thread lost;
    lo_address addr;

    bool b_run_service;
    pthread_t srv_thread;
  };

} // namespace MM

using namespace MM;

void mm_midicc_t::run()
{
  while(!b_exit) {
    sleep(1);
  }
}

mm_midicc_t::mm_midicc_t(const std::string& osc_server_addr,
                         const std::string& osc_server_port,
                         const std::string& osc_dest_addr)
    : midi_ctl_t("mm_mididcc"), mm(NULL), modified(true), b_shift1(false),
      b_shift2(false), selected_out(0), b_exit(false), b_run_service(false)
{
  pthread_mutex_init(&mutex, NULL);
  if(osc_server_addr.size())
    lost = lo_server_thread_new_multicast(
        osc_server_addr.c_str(), osc_server_port.c_str(), osc_err_handler_cb);
  else
    lost = lo_server_thread_new(osc_server_port.c_str(), osc_err_handler_cb);
  addr = lo_address_new_from_url(osc_dest_addr.c_str());
  lo_address_set_ttl(addr, 1);
  lo_server_thread_add_method(lost, "/hdspmm/backend_new", "ii",
                              mm_midicc_t::hdspmm_new, this);
  lo_server_thread_add_method(lost, "/hdspmm/backend_quit", "",
                              mm_midicc_t::hdspmm_destroy, this);
  lo_server_thread_start(lost);
  start_service();
  start_updt_service();
}

void* mm_midicc_t::updt_service(void* h)
{
  ((mm_midicc_t*)h)->updt_service();
  return NULL;
}

void mm_midicc_t::start_updt_service()
{
  if(b_run_service)
    return;
  b_run_service = true;
  int err = pthread_create(&srv_thread, NULL, &mm_midicc_t::updt_service, this);
  if(err < 0)
    throw "pthread_create failed";
}

void mm_midicc_t::stop_updt_service()
{
  if(!b_run_service)
    return;
  b_run_service = false;
  pthread_join(srv_thread, NULL);
}

void mm_midicc_t::updt_service()
{
  while(b_run_service) {
    usleep(700000);
    pthread_mutex_lock(&mutex);
    if(mm && mm->ismodified()) {
      pthread_mutex_unlock(&mutex);
      upload();
      pthread_mutex_lock(&mutex);
    }
    pthread_mutex_unlock(&mutex);
  }
}

mm_midicc_t::~mm_midicc_t()
{
  stop_updt_service();
  stop_service();
  if(mm)
    delete mm;
  lo_server_thread_stop(lost);
  lo_server_thread_free(lost);
  lo_address_free(addr);
  pthread_mutex_destroy(&mutex);
}

float mm_midicc_t::midi2gain(unsigned int v)
{
  double db(v / 90.0);
  db *= db;
  return db;
}

unsigned int mm_midicc_t::gain2midi(float v)
{
  double midi(v);
  midi = 90.0 * sqrt(midi);
  return (unsigned int)(std::min(127.0, std::max(0.0, midi + 0.5)));
}

float mm_midicc_t::midi2pan(unsigned int v)
{
  if(v < 63)
    return (float)v / 126.0;
  if(v > 65)
    return 0.5 + ((float)v - 65.0) / 126.0;
  return 0.5;
}

unsigned int mm_midicc_t::pan2midi(float v)
{
  v = std::min(1.0f, std::max(0.0f, v));
  if(v < 0.5)
    return 126.0 * v;
  if(v > 0.5)
    return 65 + (126.0 * (v - 0.5));
  return 64;
}

float mm_midicc_t::midi2pan2(unsigned int v)
{
  if(v < 63)
    return 0.5 * (float)v / 126.0;
  if(v > 65)
    return 0.5 * (0.5 + ((float)v - 65.0) / 126.0);
  return 0.25;
}

unsigned int mm_midicc_t::pan22midi(float v)
{
  v *= 2;
  v = std::min(1.0f, std::max(0.0f, v));
  if(v < 0.5)
    return 126.0 * v;
  if(v > 0.5)
    return 65 + (126.0 * (v - 0.5));
  return 64;
}

void mm_midicc_t::emit_event(int channel, int param, int value)
{
  if(channel != 0)
    return;
  pthread_mutex_lock(&mutex);
  if(mm) {
    unsigned int mod_in = 7 * b_shift1 + 14 * b_shift2;
    switch(param) {
    case 7: // output gain
      mm->set_out_gain(selected_out, midi2gain(value));
      break;
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
      mm->set_gain(selected_out, param + mod_in, midi2gain(value));
      break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
      // DEBUG(param+mod_in-8);
      // DEBUG(value);
      mm->set_mute(param + mod_in - 8, (value == 0));
      break;
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
      if(mm->get_n_out(selected_out) == 2)
        mm->set_pan(selected_out, param - 24 + mod_in, midi2pan2(value));
      else
        mm->set_pan(selected_out, param - 24 + mod_in, midi2pan(value));
      break;
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
      selected_out = param - 16;
      pthread_mutex_unlock(&mutex);
      upload();
      pthread_mutex_lock(&mutex);
      break;
    case 23:
      b_shift2 = (value > 0);
      pthread_mutex_unlock(&mutex);
      upload();
      pthread_mutex_lock(&mutex);
      break;
    case 15:
      b_shift1 = (value > 0);
      pthread_mutex_unlock(&mutex);
      upload();
      pthread_mutex_lock(&mutex);
      break;
    }
  }
  pthread_mutex_unlock(&mutex);
}

void mm_midicc_t::upload()
{
  // DEBUG(mm);
  pthread_mutex_lock(&mutex);
  if(mm) {
    unsigned int mod_in = 7 * b_shift1 + 14 * b_shift2;
    for(unsigned int k = 0; k < mm->get_num_outputs(); k++)
      mm->set_select_out(k, k == selected_out);
    for(unsigned int k = 0; k < mm->get_num_inputs(); k++)
      mm->set_select_in(k, (k >= mod_in) && (k < mod_in + 7));
    // output gain:
    if(selected_out < mm->get_num_outputs())
      send_midi(0, 7, gain2midi(mm->get_out_gain(selected_out)));
    else
      send_midi(0, 7, 0);
    // matrix gains:
    for(unsigned int k = 0; k < 7; k++) {
      unsigned int kin = k + mod_in;
      if(kin < mm->get_num_inputs())
        send_midi(0, k, gain2midi(mm->get_gain(selected_out, kin)));
      else
        send_midi(0, k, 0);
    }
    // matrix pans:
    for(unsigned int k = 24; k < 31; k++) {
      unsigned int kin = k - 24 + mod_in;
      if(kin < mm->get_num_inputs())
        if(mm->get_n_out(selected_out) == 2)
          send_midi(0, k, pan22midi(mm->get_pan(selected_out, kin)));
        else
          send_midi(0, k, pan2midi(mm->get_pan(selected_out, kin)));
      else
        send_midi(0, k, 0);
    }
    // selection:
    for(unsigned int k = 16; k < 23; k++) {
      if(k - 16 == selected_out)
        send_midi(0, k, 127);
      else
        send_midi(0, k, 0);
    }
    for(unsigned int k = 8; k < 15; k++) {
      unsigned int kin = k + mod_in - 8;
      if(kin < mm->get_num_inputs())
        send_midi(0, k, 127 * (mm->get_mute(kin) == 0));
      else
        send_midi(0, k, 0);
    }
    send_midi(0, 23, 127 * b_shift2);
    send_midi(0, 15, 127 * b_shift2);
  } else {
    for(unsigned int k = 0; k < 64; k++) {
      send_midi(0, k, 0);
    }
  }
  pthread_mutex_unlock(&mutex);
}

int mm_midicc_t::hdspmm_new(const char* path, const char* types, lo_arg** argv,
                            int argc, lo_message msg, void* user_data)
{
  // DEBUG(path);
  if((argc == 2) && (types[0] == 'i') && (types[1] == 'i')) {
    ((mm_midicc_t*)user_data)->hdspmm_new(argv[0]->i, argv[1]->i);
    return 0;
  }
  return 1;
};

int mm_midicc_t::hdspmm_destroy(const char* path, const char* types,
                                lo_arg** argv, int argc, lo_message msg,
                                void* user_data)
{
  // DEBUG(path);
  ((mm_midicc_t*)user_data)->hdspmm_destroy();
  return 0;
};

void mm_midicc_t::hdspmm_destroy()
{
  // DEBUG("destroy");
  pthread_mutex_lock(&mutex);
  if(mm)
    delete mm;
  mm = NULL;
  modified = true;
  pthread_mutex_unlock(&mutex);
  // b_exit = true;
}

void mm_midicc_t::hdspmm_new(unsigned int kout, unsigned int kin)
{
  // DEBUG("new");
  // DEBUG(kout);
  pthread_mutex_lock(&mutex);
  if(mm)
    delete mm;
  mm = new MM::lo_matrix_t(kout, kin, lost, addr);
  modified = true;
  pthread_mutex_unlock(&mutex);
  upload();
}

int main(int argc, char** argv)
{
  try {
    // todo: midi device command line, other pars optional, midi mapping
    std::string osc_server_addr("");
    std::string osc_server_port("7777");
    std::string osc_dest_url("osc.udp://localhost:7778/");
    if(argc < 3) {
      std::cerr << "Usage: mm_midicc <midi_dev> <midi_port> [ "
                   "<osc_server_addr> <osc_server_port> <osc_dest_url> ]\n";
      return 1;
    }
    int midi_client = atoi(argv[1]);
    int midi_port = atoi(argv[2]);
    // setup OSC server:
    if(argc > 3)
      osc_server_addr = argv[3];
    if(argc > 4)
      osc_server_port = argv[4];
    if(argc > 5)
      osc_dest_url = argv[5];
    mm_midicc_t m(osc_server_addr, osc_server_port, osc_dest_url);
    m.connect_input(midi_client, midi_port);
    m.connect_output(midi_client, midi_port);
    m.run();
    return 0;
  }
  catch(const std::exception& e) {
    std::cerr << e.what() << std::endl;
    exit(1);
  }
  catch(const char* e) {
    std::cerr << e << std::endl;
    exit(1);
  }
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
