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
#include "osc_helper.h"
#include <stdlib.h>
#include <iostream>
#include "audiochunks.h"

#define OSC_ADDR "224.1.2.3"
#define OSC_PORT "6978"
#define HIST_SIZE 256

namespace HoSGUI {

  class foacoh_t : public Gtk::DrawingArea, public TASCAR::osc_server_t, public jackc_t
  {
  public:
    foacoh_t(const std::string& name);
    virtual ~foacoh_t();
    virtual int process(jack_nframes_t, const std::vector<float*>&, const std::vector<float*>&);
    void activate();
    void deactivate();
  protected:
    //Override default signal handler:
    virtual bool on_expose_event(GdkEventExpose* event);
    bool on_timeout();
    uint32_t chunksize;
    HoS::ola_t ola_w;
    HoS::ola_t ola_x;
    HoS::ola_t ola_y;
    HoS::spec_t ccoh;
    HoS::wave_t lp_c1;
    HoS::wave_t lp_c2;
    HoS::wave_t coh;
    std::string name_;
  };

}

using namespace HoSGUI;


int foacoh_t::process(jack_nframes_t n, const std::vector<float*>& vIn, const std::vector<float*>& vOut)
{
  HoS::wave_t inW(n,vIn[0]);
  HoS::wave_t inX(n,vIn[1]);
  HoS::wave_t inY(n,vIn[2]);
  ola_w.process(inW);
  ola_x.process(inX);
  ola_y.process(inY);
  for(uint32_t k=0;k<ola_x.s.size();k++){
    float _Complex c(ola_x.s[k] * conjf(ola_y.s[k]));
    float ca(cabs(c));
    if( ca > 0 )
      c/=ca;
    ccoh[k] *= lp_c1[k];
    ccoh[k] += lp_c2[k]*c;
    coh[k] = cabs(ccoh[k]);
    //coh[k] *= coh[k];
    ola_w.s[k] *= coh[k];
    ola_x.s[k] *= coh[k];
    ola_y.s[k] *= coh[k];
  }
  HoS::wave_t ow(n,vOut[0]);
  HoS::wave_t ox(n,vOut[1]);
  HoS::wave_t oy(n,vOut[2]);
  ola_w.ifft(ow);
  ola_x.ifft(ox);
  ola_y.ifft(oy);
  return 0;
}

foacoh_t::foacoh_t(const std::string& name)
  : osc_server_t(OSC_ADDR,OSC_PORT),
    jackc_t("cyclephase"),
    chunksize(get_fragsize()),
    ola_w(std::max(2048u,2*chunksize),std::max(2048u,2*chunksize),chunksize,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_HANNING),
    ola_x(std::max(2048u,2*chunksize),std::max(2048u,2*chunksize),chunksize,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_HANNING),
    ola_y(std::max(2048u,2*chunksize),std::max(2048u,2*chunksize),chunksize,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_HANNING),
    ccoh(ola_x.s.size()),
    lp_c1(ola_x.s.size()),
    lp_c2(ola_x.s.size()),
    coh(ola_x.s.size()),
    name_(name)
{
  set_prefix("/"+name);
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &foacoh_t::on_timeout), 40 );
#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  //Connect the signal handler if it isn't already a virtual method override:
  signal_expose_event().connect(sigc::mem_fun(*this, &foacoh_t::on_expose_event), false);
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  add_input_port("in.0w");
  add_input_port("in.1x");
  add_input_port("in.1y");
  add_output_port("out.0w");
  add_output_port("out.1x");
  add_output_port("out.1y");
  float fs(get_srate());
  float fscale(0.5*fs/lp_c1.size());
  for(uint32_t k=0;k<lp_c1.size();k++){
    float f(fscale*std::max(k,1u));
    float tau(std::min(0.1f,std::max(0.0005f,0.1f/f)));
    //tau = 0.004;
    lp_c1[k] = exp( -1.0/(tau * fs) );
    lp_c2[k] = 1.0f-lp_c1[k];
  }
}

void foacoh_t::activate()
{
  jackc_t::activate();
  osc_server_t::activate();
  try{
    connect_in(0,"ardour:amb_1st/out 1");
    connect_in(1,"ardour:amb_1st/out 2");
    connect_in(2,"ardour:amb_1st/out 3");
    connect_out(0,"ambdec:in.0w");
    connect_out(1,"ambdec:in.1x");
    connect_out(2,"ambdec:in.1y");
  }
  catch( const std::exception& e ){
    std::cerr << "Warning: " << e.what() << std::endl;
  };
}


void foacoh_t::deactivate()
{
  osc_server_t::deactivate();
  jackc_t::deactivate();
}

foacoh_t::~foacoh_t()
{
}
  
bool foacoh_t::on_expose_event(GdkEventExpose* event)
{
  Glib::RefPtr<Gdk::Window> window = get_window();
  if(window)
    {
      Gtk::Allocation allocation = get_allocation();
      //const int width = allocation.get_width();
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
      //cr->scale(1.0,1.0);
      cr->set_line_width(2.0);
      cr->set_source_rgb( 1, 1, 1 );
      cr->set_line_cap(Cairo::LINE_CAP_ROUND);
      cr->set_line_join(Cairo::LINE_JOIN_ROUND);
      // bg:
      cr->save();
      cr->set_source_rgb( 1.0, 1.0, 1.0 );
      cr->paint();
      cr->restore();
      // end bg
      cr->save();
      cr->set_source_rgb( 0, 0, 0 );
      cr->move_to(0,height-height*coh[0]);
      for(uint32_t k=1;k<coh.size();k++)
        cr->line_to(k,height-height*coh[k]);
      cr->stroke();
      cr->restore();
    }
  return true;
}

bool foacoh_t::on_timeout()
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
  win.set_title("HoS cycle phase");
  HoSGUI::foacoh_t c("foacoh");
  win.add(c);
  win.set_default_size(1024,768);
  //win.fullscreen();
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
