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
#include <tascar/jackclient.h>
#include <tascar/osc_helper.h>
#include <tascar/stft.h>
#include <stdlib.h>
#include <iostream>
#include "libhos_audiochunks.h"

#define OSC_ADDR "239.255.1.7"
#define OSC_PORT "6978"
#define HIST_SIZE 256

namespace HoSGUI {

  class cycle_t {
  public:
    cycle_t(double x, double y,uint32_t channels, const std::string& name,uint32_t chunksize,double srate);
    void set_values(const std::vector<double>& v);
    static int osc_setval(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    void draw(Cairo::RefPtr<Cairo::Context> cr, double phase);
    void calc_chroma(float* samples, uint32_t n);
    void update_history(double phase);
    void set_f0(uint32_t ch,double f);
  private:
    double mapped_intensity(double f, double t0, double i);
    double x_;
    double y_;
    uint32_t channels_;
    std::string name_;
    std::vector<double> current;
    std::vector<std::vector<double> > history;
    std::vector<double> col_r;
    std::vector<double> col_g;
    std::vector<double> col_b;
    TASCAR::wave_t inBuffer;
    HoS::delay1_t delay;
    TASCAR::stft_t stft0;
    TASCAR::stft_t stft1;
    TASCAR::wave_t instf;
    std::vector<double> t0;
    std::vector<double> chroma;
    double ifscale;
    double last_phase;
  };

  class cyclephase_t : public Gtk::DrawingArea, public TASCAR::osc_server_t, public jackc_t
  {
  public:
    cyclephase_t(const std::string& name,uint32_t channels);
    virtual ~cyclephase_t();
    void add_cycle(double x, double y, const std::string& name);
    virtual int process(jack_nframes_t, const std::vector<float*>&, const std::vector<float*>&);
    void activate();
    void deactivate();
    void set_f0(uint32_t ch,double f);
  protected:
    //Override default signal handler:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
    //virtual bool on_expose_event(GdkEventExpose* event);
    bool on_timeout();

    std::vector<cycle_t*> vCycle;
    double phase;
    uint32_t channels_;
    std::string name_;
  };

}

using namespace HoSGUI;

cycle_t::cycle_t(double x, double y,uint32_t channels, const std::string& name,uint32_t chunksize,double srate)
  : x_(x),y_(y),channels_(channels),name_(name),current(channels,0.0),inBuffer(chunksize),delay(chunksize),
    stft0(std::max(2048u,2*chunksize),std::max(2048u,2*chunksize),chunksize,TASCAR::stft_t::WND_HANNING,0.5),
    stft1(std::max(2048u,2*chunksize),std::max(2048u,2*chunksize),chunksize,TASCAR::stft_t::WND_HANNING,0.5),
    instf(stft0.s.n_),
    ifscale(srate/(2.0*M_PI)),
    last_phase(0.0)
{
  history.resize(HIST_SIZE);
  for(unsigned int k=0;k<history.size();k++)
    history[k].resize(channels);
  col_r.resize(channels);
  col_g.resize(channels);
  col_b.resize(channels);
  for(unsigned int k=0;k<channels;k++){
    col_r[k] = 0.5+0.5*cos(k*M_PI*2.0/channels);
    col_g[k] = 0.5+0.5*cos(k*M_PI*2.0/channels+2.0/3.0*M_PI);
    col_b[k] = 0.5+0.5*cos(k*M_PI*2.0/channels+4.0/3.0*M_PI);
  }
  t0.resize(channels);
  chroma.resize(channels);
}

double cycle_t::mapped_intensity(double f, double t0, double i)
{
  double ft;
  // do not check for more than 5 octaves:
  if( f*t0 > 32 )
    return 0.0;
  // a quarter tone above f0:
  while( (ft = f*t0) > 1.02903 )
    f *= 0.5;
  // a quarter tone below f0:
  if( ft > 0.97153 )
    return i;
  return 0.0;
}

void cycle_t::set_f0(uint32_t ch,double f)
{
  if( f > 0 )
    t0[ch] = 1.0/f;
}

void cycle_t::update_history(double phase)
{
  phase = std::min(std::max(phase,0.0),1.0);
  unsigned int kl(last_phase*(HIST_SIZE-1));
  unsigned int kn(phase*(HIST_SIZE-1));
  if( kl == kn ){
    for(unsigned int ch=0;ch<channels_;ch++)
      history[kl][ch] = current[ch];
    last_phase = phase;
    return;
  }
  if( kl < kn ){
    for(unsigned int k=kl+1;k<=kn;k++)
      for(unsigned int ch=0;ch<channels_;ch++)
        history[k][ch] = current[ch];
  }else{
    for(unsigned int k=kl+1;k<HIST_SIZE;k++)
      for(unsigned int ch=0;ch<channels_;ch++)
        history[k][ch] = current[ch];
    for(unsigned int k=0;k<=kn;k++)
      for(unsigned int ch=0;ch<channels_;ch++)
        history[k][ch] = current[ch];
  }
  last_phase = phase;
}

void cycle_t::set_values(const std::vector<double>& v)
{
  if( v.size() == current.size() )
    for(unsigned int k=0;k<v.size();k++)
      current[k] = sqrt(v[k]);
}

int cycle_t::osc_setval(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  std::vector<double> vtmp;
  for(int k=0;k<argc;k++)
    if(types[k]=='f')
      vtmp.push_back(argv[k]->f);
  if( user_data)
    ((cycle_t*)user_data)->set_values(vtmp);
  return 0;
}

void cycle_t::calc_chroma(float* samples, uint32_t n)
{
  for(uint32_t k=0;k<std::min(n,inBuffer.n);k++){
    inBuffer.d[k] = samples[k];
  }
  delay.process(inBuffer);
  stft0.process(inBuffer);
  stft1.process(delay);
  for(unsigned int k=0;k<stft0.s.n_;k++){
    stft0.s.b[k] *= conj(stft1.s.b[k]);
    instf.d[k] = std::arg(stft0.s.b[k])*ifscale;
  }
  double te(0.0);
  for(unsigned int ch=0;ch<channels_;ch++)
    chroma[ch] = 0.0;
  for(unsigned int k=0;k<instf.n;k++){
    double le(std::abs(stft0.s.b[k]));
    te+=le;
    for(unsigned int ch=0;ch<channels_;ch++)
      chroma[ch] += mapped_intensity(instf.d[k],t0[ch],le);
  }
  if( te > 0 )
    te = 1.0/te;
  for(unsigned int ch=0;ch<channels_;ch++)
    chroma[ch] *= te;
  set_values(chroma);
}

void cycle_t::draw(Cairo::RefPtr<Cairo::Context> cr, double phase)
{
  cr->save();
  cr->translate(-x_,-y_);
  cr->scale(0.46,0.46);
  double hscale(2*M_PI/history.size());
  for(unsigned int ch=0;ch<channels_;ch++){
    double r(0.4+0.6*ch/channels_);
    cr->set_source_rgb( col_r[ch], col_g[ch], col_b[ch] );
    for(unsigned int kh=0;kh<history.size();kh++){
      cr->set_line_width(1.5*history[kh][ch]/(2*channels_));
      cr->arc(0,0,r,((double)kh-1.0)*hscale-0.5*M_PI,kh*hscale-0.5*M_PI);
      cr->stroke();
    }
  }
  cr->begin_new_path();
  double cx(sin(phase*M_PI*2.0));
  double cy(-cos(phase*M_PI*2.0));
  cr->set_source_rgba( 0.905882,0.117647,0, 0.5);
  cr->set_line_width(1.0/80.0);
  cr->move_to( cx, cy );
  cr->line_to( 0, 0 );
  cr->stroke();
  //cr->arc( 1.2*cx, 1.2*cy, 0.15, 0, 2*M_PI);
  //cr->fill();
  cr->restore();
}


int cyclephase_t::process(jack_nframes_t n, const std::vector<float*>& vIn, const std::vector<float*>& vOut)
{
  phase = vIn[0][0];
  for(unsigned int k=0;k<vCycle.size();k++){
    vCycle[k]->calc_chroma(vIn[k+1],n);
    vCycle[k]->update_history(phase);
  }
  return 0;
}

void cyclephase_t::set_f0(uint32_t ch,double f)
{
  for(unsigned int k=0;k<vCycle.size();k++)
    vCycle[k]->set_f0(ch,f);
}

cyclephase_t::cyclephase_t(const std::string& name,uint32_t channels)
  : osc_server_t(OSC_ADDR,OSC_PORT,"UDP"),
    jackc_t("cyclephase"),
    phase(0),
    channels_(channels),
    name_(name)
{
  set_prefix("/"+name);
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &cyclephase_t::on_timeout), 40 );
//#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
//  //Connect the signal handler if it isn't already a virtual method override:
//  signal_expose_event().connect(sigc::mem_fun(*this, &cyclephase_t::on_expose_event), false);
//#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  add_input_port("phase");
}

void cyclephase_t::activate()
{
  jackc_t::activate();
  osc_server_t::activate();
  try{
    connect_in(0,"phase:phase");
  }
  catch( const std::exception& e ){
    std::cerr << "Warning: " << e.what() << std::endl;
  };
}


void cyclephase_t::deactivate()
{
  osc_server_t::deactivate();
  jackc_t::deactivate();
}

cyclephase_t::~cyclephase_t()
{
  for(unsigned int k=0;k<vCycle.size();k++)
    delete vCycle[k];
}
  
void cyclephase_t::add_cycle(double x, double y, const std::string& name)
{
  std::string fmt(channels_,'f');
  cycle_t* ctmp(new cycle_t(x,y,channels_,name,fragsize,srate));
  vCycle.push_back(ctmp);
  add_method("/"+name,fmt.c_str(),cycle_t::osc_setval,ctmp);
  add_input_port(name);
}

bool cyclephase_t::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  Glib::RefPtr<Gdk::Window> window = get_window();
  if(window){
      Gtk::Allocation allocation = get_allocation();
      const int width = allocation.get_width();
      const int height = allocation.get_height();
      cr->translate(0.5*width,0.5*height);
      double scale = std::min(width,height)/3;
      cr->scale(scale,scale);
      cr->set_line_width(1.0/400.0);
      cr->set_source_rgb( 0.7, 0.7, 0.7 );
      cr->set_line_cap(Cairo::LINE_CAP_ROUND);
      cr->set_line_join(Cairo::LINE_JOIN_ROUND);
      // bg:
      cr->save();
      cr->set_source_rgb( 1.0, 1.0, 1.0 );
      cr->paint();
      cr->restore();
      // end bg
      for(unsigned int k=0;k<vCycle.size();k++)
        vCycle[k]->draw(cr,phase);
    }
  return true;
}

bool cyclephase_t::on_timeout()
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
  HoSGUI::cyclephase_t c("cyclephase",6);
  c.add_cycle(0,1,"1");
  c.add_cycle(-0.866,-0.5,"2");
  c.add_cycle(0.866,-0.5,"3");
  c.add_cycle(0,0,"4");
  c.set_f0(0,116.46); // B
  c.set_f0(1,155.45); // e
  c.set_f0(2,174.49); // f#
  c.set_f0(3,184.86); // g
  c.set_f0(4,207.50); // a
  c.set_f0(5,276.98); // d'
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
