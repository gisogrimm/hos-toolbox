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

#include <gtkmm.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/drawingarea.h>
#include <cairomm/context.h>
#include "jackclient.h"
//#include "osc_helper.h"
#include <stdlib.h>
#include <iostream>
//#include "audiochunks.h"

#include "defs.h"

#define OSC_ADDR "239.255.1.7"
#define OSC_PORT "6978"
#define HIST_SIZE 256

namespace HoSGUI {

  class scope_t : public Gtk::DrawingArea, public jackc_t
  {
  public:
    scope_t(const std::string& name,uint32_t channels, uint32_t period, uint32_t downsample);
    virtual ~scope_t();
    virtual int process(jack_nframes_t, const std::vector<float*>&, const std::vector<float*>&);
  protected:
    virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
    //virtual bool on_expose_event(GdkEventExpose* event);
    bool on_timeout();
    uint32_t channels_;
    uint32_t period_;
    uint32_t downsample_;
    std::string name_;
    std::vector<double> col_r;
    std::vector<double> col_g;
    std::vector<double> col_b;
    std::vector<std::vector<double> > data;
    uint32_t t;
    uint32_t dt;
  };

}

using namespace HoSGUI;

int scope_t::process(jack_nframes_t n, const std::vector<float*>& vIn, const std::vector<float*>& vOut)
{
  for(uint32_t k=0;k<n;k++){
    if( dt==0){
      dt = downsample_;
      t++;
      if( t >= period_ )
        t = 0;
      for(uint32_t ch=0;ch<channels_;ch++){
        data[ch][t] = vIn[ch][k];
      }
    }
    dt--;
  }
  return 0;
}

scope_t::scope_t(const std::string& name,uint32_t channels,uint32_t period,uint32_t downsample)
  : jackc_t("hos_scope"),
    channels_(channels),
    period_(period),
    downsample_(downsample),
    name_(name),
    data(channels,std::vector<double>(period,0)),
    t(0),
    dt(0)
{
  col_r.resize(channels);
  col_g.resize(channels);
  col_b.resize(channels);
  for(unsigned int k=0;k<channels;k++){
    col_r[k] = 0.5+0.5*cos(k*M_PI*2.0/channels);
    col_g[k] = 0.5+0.5*cos(k*M_PI*2.0/channels+2.0/3.0*M_PI);
    col_b[k] = 0.5+0.5*cos(k*M_PI*2.0/channels+4.0/3.0*M_PI);
  }
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &scope_t::on_timeout), 40 );
  signal_draw().connect(sigc::mem_fun(*this, &scope_t::on_draw), false);
#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  //Connect the signal handler if it isn't already a virtual method override:
  //signal_expose_event().connect(sigc::mem_fun(*this, &scope_t::on_expose_event), false);
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  for(uint32_t ch=0;ch<channels_;ch++){
    char ctmp[1024];
    sprintf(ctmp,"in_%d",ch+1);
    add_input_port(ctmp);
  }
}

scope_t::~scope_t()
{
//  for(unsigned int k=0;k<vCycle.size();k++)
//    delete vCycle[k];
}

bool scope_t::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  Gtk::Allocation allocation = get_allocation();
  const int width = allocation.get_width();
  const int height = allocation.get_height();
  //double ratio = (double)width/(double)height;
  //cr->clip();
  cr->save();
  cr->set_source_rgb( 1.0, 1.0, 1.0 );
  cr->paint();
  cr->restore();
  cr->save();
  cr->move_to(t*(double)width/(double)period_,0);
  cr->line_to(t*(double)width/(double)period_,height);
  cr->stroke();
  cr->restore();
  for(uint32_t ch=0;ch<channels_;ch++){
    cr->set_source_rgb( col_r[ch], col_g[ch], col_b[ch] );
    cr->save();
    cr->move_to(0,height-data[ch][0]*height);
    for(uint32_t ti=1;ti<period_;ti++){
      cr->line_to(ti*(double)width/(double)period_,height-data[ch][ti]*height);
    }
    cr->stroke();
    cr->restore();
  }
  // end bg
  //for(unsigned int k=0;k<vCycle.size();k++)
  //  vCycle[k]->draw(cr,phase);
  return true;
}

bool scope_t::on_timeout()
{
  Glib::RefPtr<Gdk::Window> win = get_window();
  if(win){
    Gdk::Rectangle r(0, 0, get_allocation().get_width(),
                     get_allocation().get_height());
    win->invalidate_rect(r, false);
  }
  return true;
}


void lo_err_handler_cb(int num, const char *msg, const char *where) 
{
  std::cerr << "lo error " << num << ": " << msg << " (" << where << ")\n";
}

int main(int argc, char** argv)
{
  Gtk::Main kit(argc, argv);
  Gtk::Window win;
  win.set_title("HoS oscilloscope");
  HoSGUI::scope_t c("scope",2,300,600);
  win.add(c);
  win.set_default_size(600,240);
  win.show_all();
  c.jackc_t::activate();
  Gtk::Main::run(win);
  c.jackc_t::deactivate();
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
