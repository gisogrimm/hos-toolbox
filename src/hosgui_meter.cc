/**
   \file hosgui_meter.cc
   \ingroup apphos
   \brief Visualization of level meters
   \author Giso Grimm
   \date 2011

   \section license License (GPL)
   
   Copyright (C) 2011 Giso Grimm
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
/**
 */
#include "hosgui_meter.h"

using namespace HoSGUI;


int meter_t::setlevel(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (argc == 3) && (types[0] == 'f') && (types[1] == 'f') && (types[2] == 'f') ){
    ((meter_t*)user_data)->peak = argv[0]->f;
    ((meter_t*)user_data)->rms_fast = argv[1]->f;
    ((meter_t*)user_data)->rms_slow = argv[2]->f;
    return 0;
  }
  return 1;
};

int meter_t::settarget(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (argc == 1) && (types[0] == 'f') ){
    ((meter_t*)user_data)->target = argv[0]->f;
    return 0;
  }
  return 1;
};

meter_t::meter_t(const std::string& paddr, lo_server_thread& l, double lmin_, double lmax_)
  : peak(0),
    rms_fast(0),
    rms_slow(0),
    lmin(lmin_),
    lmax(lmax_),
    target(80),
    lost(l)
{
  std::string baseaddr("/rmsmeter");
  std::string tmp;
  tmp = baseaddr + paddr;
  lo_server_thread_add_method(lost,tmp.c_str(),"fff",meter_t::setlevel,this);
  tmp = baseaddr + "/target" + paddr;
  lo_server_thread_add_method(lost,tmp.c_str(),"f",meter_t::settarget,this);
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &meter_t::on_timeout), 50 );
#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  //Connect the signal handler if it isn't already a virtual method override:
  signal_expose_event().connect(sigc::mem_fun(*this, &meter_t::on_expose_event), false);
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
}

meter_t::~meter_t()
{
}

bool meter_t::on_expose_event(GdkEventExpose* event)
{
  // This is where we draw on the window
  Glib::RefPtr<Gdk::Window> window = get_window();
  if(window)
    {
      Gtk::Allocation allocation = get_allocation();
      const int width = allocation.get_width();
      const int height = allocation.get_height();

      Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();

      if(event)
        {
          // clip to the area indicated by the expose event so that we only
          // redraw the portion of the window that needs to be redrawn
          cr->rectangle(event->area.x, event->area.y,
                        event->area.width, event->area.height);
          cr->clip();
        }

      cr->scale(width, -height/(lmax-lmin));
      cr->translate(0, -lmax);
      cr->set_line_width(1);
      cr->save();
      cr->set_source_rgb( 0, 0.07, 0.06 );
      cr->paint();
      cr->restore();
      // fast rms:
      cr->save();
      cr->set_source_rgb(0.302, 0.824, 0.176);
      cr->move_to(0, lmin );
      cr->line_to( 1, lmin );
      cr->line_to( 1, rms_fast );
      cr->line_to( 0, rms_fast );
      cr->fill();
      cr->restore();
      // peak level:
      cr->save();
      cr->set_source_rgba(1, 1, 0.27, 0.8);
      cr->set_line_width(1);
      cr->move_to(0, peak );
      cr->line_to( 1, peak );
      cr->stroke();
      cr->restore();
      // slow rms:
      cr->save();
      cr->set_source_rgba(0.624, 0.125, 0.149, 0.8);
      cr->set_line_width(1);
      cr->move_to(0, rms_slow );
      cr->line_to( 1, rms_slow );
      cr->stroke();
      cr->restore();
      // target:
      cr->save();
      cr->set_source_rgba(0.624, 0.125, 0.149, 0.4);
      cr->set_line_width(2);
      cr->move_to(0, target );
      cr->line_to( 1, target );
      cr->stroke();
      cr->restore();

    }

  return true;
}

meter_scale_t::meter_scale_t(double lmin_, double lmax_)
  : lmin(lmin_),
    lmax(lmax_)
{
#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  //Connect the signal handler if it isn't already a virtual method override:
  signal_expose_event().connect(sigc::mem_fun(*this, &meter_t::on_expose_event), false);
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
}

meter_scale_t::~meter_scale_t()
{
}

bool meter_scale_t::on_expose_event(GdkEventExpose* event)
{
  // This is where we draw on the window
  Glib::RefPtr<Gdk::Window> window = get_window();
  if(window)
    {
      Gtk::Allocation allocation = get_allocation();
      const int width = allocation.get_width();
      const int height = allocation.get_height();

      Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();

      if(event)
        {
          // clip to the area indicated by the expose event so that we only
          // redraw the portion of the window that needs to be redrawn
          cr->rectangle(event->area.x, event->area.y,
                        event->area.width, event->area.height);
          cr->clip();
        }

      cr->scale(width, -height/(lmax-lmin));
      cr->translate(0, -lmax);
      cr->set_line_width(1);
      cr->save();
      cr->set_source_rgb( 0.6, 0.6, 0.6 );
      cr->paint();
      cr->restore();
      // ticks:
      cr->save();
      cr->set_source_rgb(0, 0, 0);
      cr->set_line_width(0.3);
      for( double l=floor(lmin);l<=floor(lmax);l+=1){
        if( (int)l % 10 == 0 ){
          cr->move_to(0.2, l );
        }else{
          if( (int)l % 2 == 0 ){
            cr->move_to(0.6, l );
          }else{
            cr->move_to(0.8, l );
          }
        }
        cr->line_to( 1, l );
        cr->stroke();
      }
      cr->restore();
      // text:
      cr->save();
      cr->scale(height/(width*(lmax-lmin)), -1);
      //cr->translate(0, 0);
      cr->set_font_size(2.0);
      cr->set_source_rgb(0, 0, 0);
      char ctmp[128];
      for( double l=10*floor(0.1*lmin);l<=10*floor(0.1*lmax);l+=10){
        cr->move_to( 0, -l-0.5 );
        sprintf(ctmp,"%g",l);
        cr->show_text(ctmp);
      }
      cr->restore();

    }

  return true;
}


bool meter_t::on_timeout()
{
  // force our program to redraw the entire clock.
  Glib::RefPtr<Gdk::Window> win = get_window();
  if (win)
    {
      Gdk::Rectangle r(0, 0, get_allocation().get_width(),
                       get_allocation().get_height());
      win->invalidate_rect(r, false);
    }
  return true;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
