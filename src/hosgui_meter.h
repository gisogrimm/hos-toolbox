/**
   \file hosgui_meter.h
   \ingroup apphos
   \brief level meter visualization
   \author Giso Grimm
   \date 2011

   \section license License (GPL)

   Copyright (C) 2011 Giso Grimm

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

*/
#ifndef HOSGUI_METER_H
#define HOSGUI_METER_H

#include <cairomm/context.h>
#include <gtkmm.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <lo/lo.h>

/**
   \brief Classes for visualization
*/
namespace HoSGUI {

  class meter_t : public Gtk::DrawingArea {
  public:
    meter_t(const std::string& paddr, lo_server_thread& l, double lmin,
            double lmax);
    virtual ~meter_t();

  protected:
    // Override default signal handler:
    virtual bool on_expose_event(GdkEventExpose* event);

    static int setlevel(const char* path, const char* types, lo_arg** argv,
                        int argc, lo_message msg, void* user_data);
    static int settarget(const char* path, const char* types, lo_arg** argv,
                         int argc, lo_message msg, void* user_data);

    bool on_timeout();

    double peak;
    double rms_fast;
    double rms_slow;
    double lmin;
    double lmax;
    double target;

    lo_server_thread& lost;
  };

  class meter_scale_t : public Gtk::DrawingArea {
  public:
    meter_scale_t(double lmin, double lmax);
    virtual ~meter_scale_t();

  protected:
    // Override default signal handler:
    virtual bool on_expose_event(GdkEventExpose* event);

    double lmin;
    double lmax;
  };

  class meter_frame_t : public Gtk::Frame {
  public:
    meter_frame_t(const std::string& paddr, lo_server_thread& l)
        : sc(40, 93), m(paddr, l, 40, 93)
    {
      set_label(paddr);
      add(box);
      box.add(sc);
      box.add(m);
    };

  protected:
    Gtk::HBox box;
    meter_scale_t sc;
    meter_t m;
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
