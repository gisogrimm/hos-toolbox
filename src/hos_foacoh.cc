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

/*
  measure 1:

  if signal is FOA-panned, then X/Y (and thus X*conj(Y) ) must be real-valued

  c = < abs(real(X*conj(Y))) / abs(X*conj(Y)) >

  if signal is FOA-panned, then (X/W)^2 + (Y/W)^2 = 0.5

  c = < 1 - abs(2*((X/W)^2 + (Y/W)^2)-1) >


  measure 2:

  coherence function of X/Y

  c = abs( < X/Y*abs(Y/X) > )

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
#include "defs.h"


#define OSC_ADDR "224.1.2.3"
#define OSC_PORT "6978"
#define HIST_SIZE 256

class az_hist_t : public HoS::wave_t
{
public:
  az_hist_t();
  void update();
  void add(float f, float az,float weight);
  void set_tau(float tau,float fs);
  void draw(const Cairo::RefPtr<Cairo::Context>& cr,float scale);
  void set_frange(float f1,float f2);
private:
  float c1;
  float c2;
  float fmin;
  float fmax;
};

az_hist_t::az_hist_t()
  : HoS::wave_t(36)
{
  set_tau(1.0f,1.0f);
  set_frange(0,22050);
}

void az_hist_t::update()
{
  *this *= c1;
}

void az_hist_t::add(float f, float az, float weight)
{
  if( (f >= fmin) && (f < fmax) ){
    az += M_PI;
    az *= (0.5*size()/M_PI);
    uint32_t iaz(std::max(0.0f,std::min((float)(size()-1),az)));
    operator[](iaz) += c2*weight;
  }
}

void az_hist_t::set_tau(float tau,float fs)
{
  c1 = exp( -1.0/(tau * fs) );
  c2 = 1.0f - c1;
}

void az_hist_t::set_frange(float f1,float f2)
{
  fmin = f1;
  fmax = f2;
}

void az_hist_t::draw(const Cairo::RefPtr<Cairo::Context>& cr,float scale)
{
  float w(0);
  for(uint32_t k=0;k<size();k++)
    w+=operator[](k);
  w /= size();
  if( w > 0.0f )
    scale /= w;
  for(uint32_t k=0;k<size();k++){
    float arg(2.0*M_PI*k/size());
    float r(scale*operator[](k));
    float x(r*cos(arg));
    float y(r*sin(arg));
    if( k==0)
      cr->move_to(x,y);
    else
      cr->line_to(x,y);
  }
  cr->stroke();
}

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
    virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
    bool on_timeout();
    uint32_t chunksize;
    uint32_t fftlen;
    uint32_t wndlen;
    HoS::ola_t ola_w;
    HoS::ola_t ola_x;
    HoS::ola_t ola_y;
    HoS::ola_t ola_inv_w;
    HoS::ola_t ola_inv_x;
    HoS::ola_t ola_inv_y;
    // low pass filter coefficients:
    HoS::wave_t lp_c1;
    HoS::wave_t lp_c2;
    // complex temporary coherence:
    HoS::spec_t ccohXY;
    //HoS::spec_t ccoh2;
    // coherence functions:
    HoS::wave_t cohXY;
    HoS::wave_t cohReXY;
    HoS::wave_t cohXWYW;
    HoS::wave_t az;
    std::vector<az_hist_t> haz;
    //az_hist_t haz2;
    std::string name_;
    //float haz_c1;
    //float haz_c2;
    float fscale;
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
  for(uint32_t kH=0;kH<haz.size();kH++)
    haz[kH].update();
  for(uint32_t k=0;k<ola_x.s.size();k++){
    float freq(k*fscale);
    // for all measures, X*conj(Y) and its absolute value is needed:
    float _Complex cW(ola_w.s[k]);
    float _Complex cX(ola_x.s[k]);
    float _Complex cY(ola_y.s[k]);
    float _Complex cXY(cX * conjf(cY));
    float cXYabs(cabsf(cXY));
    // Measure 1: x-y-coherence:
    ccohXY[k] *= lp_c1[k];
    if( cXYabs > 0 ){
      ccohXY[k] += (lp_c2[k]/cXYabs)*cXY;
    }
    cohXY[k] = cabs(ccohXY[k]);
    // Measure 2: real part of X/Y:
    cohReXY[k] *= lp_c1[k];
    if( cXYabs > 0 ){
      cohReXY[k] += lp_c2[k]*fabsf(crealf(cXY))/cXYabs;
    }
    cohReXY[k] = fabsf(creal(ccohXY[k]));
    // Measure 3: (X/W)^2 + (Y/W)^2 = 2
    cohXWYW[k] = std::min(1.0f,cohXWYW[k]);
    cohXWYW[k] *= lp_c1[k];
    //az[k] = atan2f(creal(cY),creal(cX));
    if( cabsf(cW) > 0 ){
      cX /= cW;
      cY /= cW;
      cohXWYW[k] += lp_c2[k]*0.25*cabsf(cX*conjf(cX) + cY*conjf(cY));
    }
    az[k] = cargf(cX+I*cY);
    ////coh[k] *= coh[k];
    //ola_inv_w.s[k] = ola_w.s[k];
    //ola_inv_x.s[k] = ola_x.s[k];
    //ola_inv_y.s[k] = ola_y.s[k];
    //ola_w.s[k] *= coh[k];
    //ola_x.s[k] *= coh[k];
    //ola_y.s[k] *= coh[k];
    //c = ola_x.s[k] * conjf(ola_y.s[k]);
    //ccoh2[k] *= lp_c1[k];
    //ccoh2[k] += lp_c2[k]*c;
    ////ccoh2[k] = c;
    ////az[k] = cargf(ccoh2[k]);
    //
    ////az[k] *= lp_c1[k];
    ////az[k] += lp_c2[k]*azt;
    //az[k] = azt;
    float w(pow(cohReXY[k],2.0));
    for(uint32_t kH=0;kH<haz.size();kH++)
      haz[kH].add(freq,az[k],w);
    //ola_inv_w.s[k] *= 1.0f-coh[k];
    //ola_inv_x.s[k] *= 1.0f-coh[k];
    //ola_inv_y.s[k] *= 1.0f-coh[k];
  }
  //HoS::wave_t ow(n,vOut[0]);
  //HoS::wave_t ox(n,vOut[1]);
  //HoS::wave_t oy(n,vOut[2]);
  //HoS::wave_t inv_ow(n,vOut[3]);
  //HoS::wave_t inv_ox(n,vOut[4]);
  //HoS::wave_t inv_oy(n,vOut[5]);
  //ola_w.ifft(ow);
  //ola_x.ifft(ox);
  //ola_y.ifft(oy);
  //ola_inv_w.ifft(inv_ow);
  //ola_inv_x.ifft(inv_ox);
  //ola_inv_y.ifft(inv_oy);
  return 0;
}

foacoh_t::foacoh_t(const std::string& name)
  : osc_server_t(OSC_ADDR,OSC_PORT),
    jackc_t("foacoh"),
    chunksize(get_fragsize()),
    fftlen(std::max(512u,4*chunksize)),
    wndlen(std::max(256u,2*chunksize)),
    ola_w(fftlen,wndlen,chunksize,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_HANNING),
    ola_x(fftlen,wndlen,chunksize,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_HANNING),
    ola_y(fftlen,wndlen,chunksize,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_HANNING),
    ola_inv_w(fftlen,wndlen,chunksize,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_HANNING),
    ola_inv_x(fftlen,wndlen,chunksize,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_HANNING),
    ola_inv_y(fftlen,wndlen,chunksize,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_HANNING),
    lp_c1(ola_x.s.size()),
    lp_c2(ola_x.s.size()),
    ccohXY(ola_x.s.size()),
    cohXY(ola_x.s.size()),
    cohReXY(ola_x.s.size()),
    cohXWYW(ola_x.s.size()),
    az(ola_x.s.size()),
    name_(name)
{
  set_prefix("/"+name);
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &foacoh_t::on_timeout), 40 );
#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  //Connect the signal handler if it isn't already a virtual method override:
  signal_draw().connect(sigc::mem_fun(*this, &mixergui_t::on_draw), false);
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  add_input_port("in.0w");
  add_input_port("in.1x");
  add_input_port("in.1y");
  add_output_port("out.0w");
  add_output_port("out.1x");
  add_output_port("out.1y");
  add_output_port("inv_out.0w");
  add_output_port("inv_out.1x");
  add_output_port("inv_out.1y");
  float fs(get_srate()/get_fragsize());
  fscale = 0.5*get_srate()/lp_c1.size();
  for(uint32_t k=0;k<lp_c1.size();k++){
    float f(fscale*std::max(k,1u));
    float tau(std::min(0.5f,std::max(0.05f,150.0f/f)));
    tau = 0.04;
    lp_c1[k] = exp( -1.0/(tau * fs) );
    lp_c2[k] = 1.0f-lp_c1[k];
  }
  for(uint32_t k=0;k<az.size();k++)
    az[k] = 0.0;
  haz.resize(8);
  float fold(0);
  for(uint32_t kH=0;kH<haz.size();kH++){
    float fnext(250*powf(2.0f,(float)kH/1.0f));
    haz[kH].set_tau(std::max(0.125f,std::min(1.0f,250.0f/fnext)),fs);
    haz[kH].set_frange(fold,fnext);
    fold = fnext;
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
  
bool foacoh_t::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  Gtk::Allocation allocation = get_allocation();
  const int width = allocation.get_width();
  const int height = allocation.get_height();
  //double ratio = (double)width/(double)height;
  cr->rectangle(0,0,width, height);
  cr->clip();
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
  // coherence:
  cr->set_line_width(3.0);
  //// Measure 1:
  //cr->set_source_rgb( 1, 0, 0 );
  //cr->move_to(0,height-height*cohXY[0]);
  //for(uint32_t k=1;k<cohXY.size();k++)
  //  cr->line_to(k,height-height*cohXY[k]);
  //cr->stroke();
  //// Measure 2:
  //cr->set_source_rgb( 0, 1, 0 );
  //cr->move_to(0,height-height*cohReXY[0]);
  //for(uint32_t k=1;k<cohReXY.size();k++)
  //  cr->line_to(k,height-height*cohReXY[k]);
  //cr->stroke();
  //// Measure 3:
  //cr->set_source_rgb( 0, 0, 1 );
  //cr->move_to(0,0.5*height-0.5*height/M_PI*az[0]);
  //for(uint32_t k=1;k<az.size();k++)
  //  cr->line_to(k,0.5*height-0.5*height/M_PI*az[k]);
  //cr->stroke();


  cr->save();
  cr->translate(0.5*width,0.5*height);
  cr->scale(0.5*std::min(width,height),0.5*std::min(width,height));
  cr->set_line_width(0.006);
  cr->set_source_rgba( 0, 0, 0, 0.4 );
  cr->move_to(-1,0);
  cr->line_to(1,0);
  cr->move_to(0,-1);
  cr->line_to(0,1);
  cr->stroke();
  for(uint32_t kH=0;kH<haz.size();kH++){
    float arg((float)kH/(float)(haz.size()-1));
    cr->set_source_rgb( pow(1.0f-arg,2.0), pow(0.5-0.5*cos(2.0*M_PI*arg),4.0), pow(arg,2.0) );
    haz[kH].draw(cr,0.3);
  }
  cr->restore();
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
  win.set_title("foacoh");
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
