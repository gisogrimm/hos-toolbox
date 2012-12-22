/**
   \file hos_oscrmsmeter.cc
   \ingroup apphos
   \brief visualization of levels via OSC
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
#include <iostream>
#include "hosgui_meter.h"

using namespace HoSGUI;

void lo_err_handler_cb(int num, const char *msg, const char *where) 
{
  std::cerr << "lo error " << num << ": " << msg << " (" << where << ")\n";
}

int main(int argc, char** argv)
{
  Gtk::Main kit(argc, argv);
  
  Gtk::Window win;
  Gtk::HBox box;
  win.set_title("RMS meter");
  std::string osc_server("");
  std::string osc_port("7777");
  std::vector<meter_frame_t*> vm;
  if( argc < 4 ){
    std::cerr << "Usage: oscrmsmeter <osc_multicast> <osc_port> <addr> [ <addr> ... ]\n";
    return 1;
  }
  // setup OSC server:
  osc_server = argv[1];
  osc_port = argv[2];
  lo_server_thread lost;
  if( osc_server.size() )
    lost = lo_server_thread_new_multicast(osc_server.c_str(),osc_port.c_str(),lo_err_handler_cb);
  else
    lost = lo_server_thread_new(osc_port.c_str(),lo_err_handler_cb);
  // create meter widgets:
  for(int k=3;k<argc;k++){
    meter_frame_t* pM(new meter_frame_t(argv[k],lost));
    vm.push_back(pM);
    box.add(*pM);
    pM->show();
  }
  //box.show_all();
  win.add(box);
  win.set_default_size(vm.size()*55,280);
  win.show_all();
  lo_server_thread_start(lost);
  Gtk::Main::run(win);
  lo_server_thread_stop(lost);
  lo_server_thread_free(lost);

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
