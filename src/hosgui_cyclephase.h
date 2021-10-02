/**
   \file hosgui_sphere.h
   \ingroup apphos
   \brief Trajectory visualization
   \author Giso Grimm
   \date 2011

   \section license License (GPL)

   Copyright (C) 2011 Giso Grimm

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
   USA.

*/
/**
 */
#ifndef HOSGUI_CYCLEPHASE_H
#define HOSGUI_CYCLEPHASE_H

#include "jackclient.h"
#include "osc_helper.h"
#include <cairomm/context.h>
#include <gtkmm.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>

namespace HoSGUI {

  class cycle_t {
  public:
    cycle_t(double x, double y, uint32_t channels, const std::string& name);
    void set_values(const std::vector<double>& v);
    static int osc_setval(const char* path, const char* types, lo_arg** argv,
                          int argc, lo_message msg, void* user_data);
    void draw(Cairo::RefPtr<Cairo::Context> cr, double phase);
    void update_history(double phase);

  private:
    double x_;
    double y_;
    uint32_t channels_;
    std::string name_;
    std::vector<double> current;
    std::vector<std::vector<double>> history;
    std::vector<double> col_r;
    std::vector<double> col_g;
    std::vector<double> col_b;
  };

  class cyclephase_t : public Gtk::DrawingArea,
                       public TASCAR::osc_server_t,
                       public jackc_t {
  public:
    cyclephase_t(const std::string& name, uint32_t channels);
    virtual ~cyclephase_t();
    void add_cycle(double x, double y, const std::string& name);
    virtual int process(jack_nframes_t, const std::vector<float*>&,
                        const std::vector<float*>&);
    void activate();
    void deactivate();

  protected:
    // Override default signal handler:
    virtual bool on_expose_event(GdkEventExpose* event);
    bool on_timeout();

    std::vector<cycle_t*> vCycle;
    double phase;
    uint32_t channels_;
    std::string name_;
  };

} // namespace HoSGUI

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
