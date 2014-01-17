/**
   \file hos_visualize_sphere.cc
   \ingroup apphos
   \brief Trajectory visualization
   \author Giso Grimm
   \date 2011

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include "hosgui_sphere.h"
#include <stdlib.h>
#include <iostream>

using namespace HoSGUI;

void lo_err_handler_cb(int num, const char *msg, const char *where) 
{
  std::cerr << "lo error " << num << ": " << msg << " (" << where << ")\n";
}

extern double brightness;

int main(int argc, char** argv)
{
  Gtk::Main kit(argc, argv);

  Gtk::Window win;
  win.set_title("HoS visualize");
  std::string osc_server("224.1.2.3");
  std::string osc_port("6978");
  double r(0);
  std::vector<std::string> pAddr;
  if( argc > 1 )
    osc_server = argv[1];
  if( argc > 2 )
    osc_port = argv[2];
  if( argc > 3 )
    r = atof(argv[3]);

  if (getenv("BRIGHTNESS"))
    brightness = atof(getenv("BRIGHTNESS"));

  lo_server_thread lost;
  if( osc_server.size() )
    lost = lo_server_thread_new_multicast(osc_server.c_str(),osc_port.c_str(),lo_err_handler_cb);
  else
    lost = lo_server_thread_new(osc_port.c_str(),lo_err_handler_cb);
  lo_address addr;
  addr = lo_address_new_from_url("osc.udp://localhost:9999/");
  lo_address_set_ttl( addr, 1 );
  pAddr.push_back("/pos/hille");
  pAddr.push_back("/pos/marthe");
  pAddr.push_back("/pos/claas");
  pAddr.push_back("/pos/julia");
  pAddr.push_back("/pos/giso");
  //pAddr.push_back("/pos/extra");
  visualize_t c(pAddr,lost);
  c.set_rotate( r*M_PI/180.0 );
  win.add(c);
  //hbox.add(m_hille);
  //hbox.add(m_marthe);
  //hbox.add(m_claas);
  //hbox.add(m_julia);
  //hbox.add(m_giso);
  //hbox.add(m_gisofezzo);
  //hbox.add(m_gisobass);
  //win.set_default_size(1024,768);
  // HDTV 720p:
  win.set_default_size(640,360);
  win.fullscreen();
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
