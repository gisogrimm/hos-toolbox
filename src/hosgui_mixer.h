/**
   \file hosgui_mixer.h
   \ingroup apphos
   \brief matrix mixer visualization
   \author Giso Grimm
   \date 2011

   \section license License (GPL)

   Copyright (C) 2011 Giso Grimm

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
/*
 */
#ifndef HOS_MIXERGUI_H
#define HOS_MIXERGUI_H

#include "libhos_gainmatrix.h"
#include <gtkmm.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/drawingarea.h>
#include <cairomm/context.h>
//#include <lo/lo.h>

namespace HoSGUI {

  class mixergui_t : public Gtk::DrawingArea, public MM::observer_t
  {
  public:
    mixergui_t();
    virtual ~mixergui_t();
    void hdspmm_destroy();
    void hdspmm_new(MM::namematrix_t*);

  protected:
    //Override default signal handler:
    virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
    //virtual bool on_expose_event(GdkEventExpose* event);
    bool on_timeout();

    MM::namematrix_t* mm;
    bool modified;
    pthread_mutex_t mutex;
  };

}

#endif


/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
