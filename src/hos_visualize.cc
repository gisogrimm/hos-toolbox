/**
   \file hos_visualize.cc
   \ingroup apphos
   \brief Trajectory, level meter and matrix mixer visualization
   \author Giso Grimm
   \date 2011

   \section license License (GPL)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2
   of the License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

*/
#include "hosgui_meter.h"
#include "hosgui_mixer.h"
#include "hosgui_sphere.h"
#include <iostream>

using namespace HoSGUI;

void lo_err_handler_cb(int num, const char* msg, const char* where)
{
  std::cerr << "lo error " << num << ": " << msg << " (" << where << ")\n";
}

int main(int argc, char** argv)
{
  Gtk::Main kit(argc, argv);

  Gtk::Window win;
  win.set_title("HoS visualize");
  std::string osc_server("239.255.1.7");
  std::string osc_port("6978");
  std::vector<std::string> pAddr;
  if(argc > 1)
    osc_server = argv[1];
  if(argc > 2)
    osc_port = argv[2];
  lo_server_thread lost;
  if(osc_server.size())
    lost = lo_server_thread_new_multicast(osc_server.c_str(), osc_port.c_str(),
                                          lo_err_handler_cb);
  else
    lost = lo_server_thread_new(osc_port.c_str(), lo_err_handler_cb);
  lo_address addr;
  addr = lo_address_new_from_url("osc.udp://localhost:9999/");
  lo_address_set_ttl(addr, 1);
  pAddr.push_back("/pos/hille");
  pAddr.push_back("/pos/marthe");
  pAddr.push_back("/pos/claas");
  pAddr.push_back("/pos/julia");
  pAddr.push_back("/pos/giso");
  pAddr.push_back("/pos/extra");
  visualize_t c(pAddr, lost);
  meter_frame_t m_hille("/hille", lost);
  meter_frame_t m_marthe("/marthe", lost);
  meter_frame_t m_claas("/claas", lost);
  meter_frame_t m_julia("/julia", lost);
  meter_frame_t m_giso("/giso", lost);
  meter_frame_t m_gisofezzo("/gisofezzo", lost);
  meter_frame_t m_gisobass("/gisobass", lost);
  Gtk::Table box_meter(1, 7, true);
  box_meter.attach(m_hille, 0, 1, 0, 1);
  box_meter.attach(m_marthe, 1, 2, 0, 1);
  box_meter.attach(m_claas, 2, 3, 0, 1);
  box_meter.attach(m_julia, 3, 4, 0, 1);
  box_meter.attach(m_giso, 4, 5, 0, 1);
  box_meter.attach(m_gisofezzo, 5, 6, 0, 1);
  box_meter.attach(m_gisobass, 6, 7, 0, 1);
  // mixergui_t mix(lost,addr);
  Gtk::VPaned vbox;
  // Gtk::HBox hbox;
  Gtk::HPaned hbox2;
  vbox.add1(c);
  vbox.add2(hbox2);
  // hbox2.add1(hbox);
  hbox2.add1(box_meter);
  // hbox2.add2(mix);
  win.add(vbox);
  // hbox.add(m_hille);
  // hbox.add(m_marthe);
  // hbox.add(m_claas);
  // hbox.add(m_julia);
  // hbox.add(m_giso);
  // hbox.add(m_gisofezzo);
  // hbox.add(m_gisobass);
  win.set_default_size(1024, 768);
  win.fullscreen();
  vbox.set_position(600);
  hbox2.set_position(640);
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
