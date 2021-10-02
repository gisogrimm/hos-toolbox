/**
  \file mm_gui.cc
  \brief Matrix mixer visualization
  \ingroup apphos
  \author Giso Grimm
  \date 2011

  "mm_{hdsp,file,gui,midicc}" is a set of programs to control the matrix
  mixer of an RME hdsp compatible sound card using XML formatted files
  or a MIDI controller, and to visualize the mixing matrix.

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
#include "hosgui_mixer.h"

using namespace HoSGUI;

void lo_err_handler_cb(int num, const char* msg, const char* where)
{
  std::cerr << "lo error " << num << ": " << msg << " (" << where << ")\n";
}

int main(int argc, char** argv)
{
  Gtk::Main kit(argc, argv);

  Gtk::Window win;
  win.set_title("RME hdsp mixer GUI");
  std::string osc_server("");
  std::string osc_port("7777");
  std::string osc_addr("osc.udp://localhost:7778/");
  if(argc < 4) {
    std::cerr << "Usage: oscmixergui <osc_multicast> <osc_port> <destaddr>\n";
    return 1;
  }
  // setup OSC server:
  osc_server = argv[1];
  osc_port = argv[2];
  osc_addr = argv[3];
  lo_server_thread lost;
  lo_address addr;
  if(osc_server.size())
    lost = lo_server_thread_new_multicast(osc_server.c_str(), osc_port.c_str(),
                                          lo_err_handler_cb);
  else
    lost = lo_server_thread_new(osc_port.c_str(), lo_err_handler_cb);
  addr = lo_address_new_from_url(osc_addr.c_str());
  lo_address_set_ttl(addr, 1);
  mixergui_t m(lost, addr);
  win.add(m);
  win.set_default_size(800, 600);
  win.show_all();
  lo_server_thread_start(lost);
  Gtk::Main::run(win);
  lo_server_thread_stop(lost);
  lo_server_thread_free(lost);
  lo_address_free(addr);
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
