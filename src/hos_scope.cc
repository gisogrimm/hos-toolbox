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

#define OSC_ADDR "224.1.2.3"
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
    //Override default signal handler:
    virtual bool on_expose_event(GdkEventExpose* event);
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
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &scope_t::on_timeout), 20 );
#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  //Connect the signal handler if it isn't already a virtual method override:
  signal_expose_event().connect(sigc::mem_fun(*this, &scope_t::on_expose_event), false);
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  for(uint32_t ch=0;ch<channels_;ch++){
    char ctmp[1024];
    sprintf(ctmp,"in_%d",ch+1);
    add_input_port(ctmp);
  }
}

//void scope_t::activate()
//{
//  jackc_t::activate();
//  try{
//    connect_in(0,"phase:phase");
//  }
//  catch( const std::exception& e ){
//    std::cerr << "Warning: " << e.what() << std::endl;
//  };
//}


//void scope_t::deactivate()
//{
//  osc_server_t::deactivate();
//  jackc_t::deactivate();
//}

scope_t::~scope_t()
{
//  for(unsigned int k=0;k<vCycle.size();k++)
//    delete vCycle[k];
}
  
//void scope_t::add_cycle(double x, double y, const std::string& name)
//{
//  std::string fmt(channels_,'f');
//  cycle_t* ctmp(new cycle_t(x,y,channels_,name,fragsize,srate));
//  vCycle.push_back(ctmp);
//  add_method("/"+name,fmt.c_str(),cycle_t::osc_setval,ctmp);
//  add_input_port(name);
//}

bool scope_t::on_expose_event(GdkEventExpose* event)
{
  Glib::RefPtr<Gdk::Window> window = get_window();
  if(window)
    {
      Gtk::Allocation allocation = get_allocation();
      const int width = allocation.get_width();
      const int height = allocation.get_height();
      //double ratio = (double)width/(double)height;
      Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();
      if(event)
        {
          // clip to the area indicated by the expose event so that we only
          // redraw the portion of the window that needs to be redrawn
          cr->rectangle(event->area.x, event->area.y,
                        event->area.width, event->area.height);
          cr->clip();
        }
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
    }
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
  HoSGUI::scope_t c("scope",2,600,300);
  //c.add_cycle(0,1,"1");
  //c.add_cycle(-0.866,-0.5,"2");
  //c.add_cycle(0.866,-0.5,"3");
  //c.add_cycle(0,0,"4");
  //c.set_f0(0,116.46); // B
  //c.set_f0(1,155.45); // e
  //c.set_f0(2,174.49); // f#
  //c.set_f0(3,184.86); // g
  //c.set_f0(4,207.50); // a
  //c.set_f0(5,276.98); // d'
  win.add(c);
  win.set_default_size(1200,400);
  //win.fullscreen();
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
