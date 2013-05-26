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
#include "hosgui_cyclephase.h"
#include <stdlib.h>
#include <iostream>

void lo_err_handler_cb(int num, const char *msg, const char *where) 
{
  std::cerr << "lo error " << num << ": " << msg << " (" << where << ")\n";
}

int main(int argc, char** argv)
{
  Gtk::Main kit(argc, argv);
  Gtk::Window win;
  win.set_title("HoS cycle phase");
  HoSGUI::cyclephase_t c("cyclephase",6);
  c.add_cycle(0,1,"1");
  c.add_cycle(-0.866,-0.5,"2");
  c.add_cycle(0.866,-0.5,"3");
  c.add_cycle(0,0,"4");
  win.add(c);
  win.set_default_size(1024,768);
  win.fullscreen();
  win.show_all();
  c.activate();
  Gtk::Main::run(win);
  c.deactivate();
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
