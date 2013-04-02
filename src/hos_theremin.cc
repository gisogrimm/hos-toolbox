#include "jackclient.h"
#include <string.h>
#include <pthread.h>
#include <iostream>
#include <complex.h>
#include <fftw3.h>
#include <gtkmm.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/drawingarea.h>
#include <cairomm/context.h>

#define DEBUG(x) std::cerr << __FILE__ << ":" << __LINE__ << " " << #x << "=" << x << std::endl

class wave_t {
public:
  wave_t(uint32_t n) : n_(n),b(new float[n_]){
    memset(b,0,n_*sizeof(float));
  };
  wave_t(const wave_t& src) : n_(src.n_),b(new float[n_]){};
  ~wave_t(){
    //DEBUG(b);
    //DEBUG(this);
    //delete [] b;
  };
  void operator/=(const float& d){
    if(fabsf(d)>0)
      for(unsigned int k=0;k<n_;k++)
        b[k]/=d;
  };
  float maxabs(){
    float r(0);
    for(unsigned int k=0;k<n_;k++)
      r = std::max(r,fabsf(b[k]));
    return r;
  };
  void copy(const wave_t& src){memcpy(b,src.b,std::min(n_,src.n_)*sizeof(float));};
  uint32_t n_;
  float* b;
};

class spec_t {
public:
  spec_t(uint32_t n) : n_(n),b(new float _Complex [n_]){};
  spec_t(const spec_t& src) : n_(src.n_),b(new float _Complex [n_]){};
  ~spec_t(){delete [] b;};
  void copy(const spec_t& src){memcpy(b,src.b,std::min(n_,src.n_)*sizeof(float _Complex));};
  void operator/=(const spec_t& o);
  uint32_t n_;
  float _Complex * b;
};

void spec_t::operator/=(const spec_t& o)
{
  for(unsigned int k=0;k<std::min(o.n_,n_);k++){
    if( cabs(o.b[k]) > 0 )
      b[k] /= o.b[k];
  }
}


class fft_t {
public:
  fft_t(uint32_t fftlen);
  void execute(const wave_t& src){w.copy(src);fftwf_execute(fftwp);};
  void execute(const spec_t& src){s.copy(src);fftwf_execute(fftwp_s2w);};
  ~fft_t();
  wave_t w;
  spec_t s;
private:
  fftwf_plan fftwp;
  fftwf_plan fftwp_s2w;
};

fft_t::fft_t(uint32_t fftlen)
  : w(fftlen),
    s(fftlen/2+1),
    fftwp(fftwf_plan_dft_r2c_1d(fftlen,w.b,s.b,0)),
    fftwp_s2w(fftwf_plan_dft_c2r_1d(fftlen,s.b,w.b,0))
{
}

fft_t::~fft_t()
{
  fftwf_destroy_plan(fftwp);
  fftwf_destroy_plan(fftwp_s2w);
}

class buffer_t {
public:
  buffer_t(uint32_t len_);
  wave_t w1;
  wave_t w2;
  uint32_t len;
};

buffer_t::buffer_t(uint32_t len_)
  : w1(len_),w2(len_),len(len_)
{
}

class pos_t {
public:
  pos_t(uint32_t buflen):r(0),w(1),l(buflen){};
  uint32_t rspace();
  uint32_t wspace();
  uint32_t r;
  uint32_t w;
  uint32_t l;
};

uint32_t pos_t::rspace()
{
  if( w >= r ) return w-r;
  return w+l-r;
}

uint32_t pos_t::wspace()
{
  if( r > w ) return r-w-1;
  return r+l-w-1;
}

class fifo_t  {
public:
  fifo_t(uint32_t len, const buffer_t& prototype);
  buffer_t& get_read_elem() { return fifo[pos.r]; };
  buffer_t& get_write_elem() { return fifo[pos.w]; };
  void read_advance();
  void write_advance();
  bool read_space(){return pos.rspace();};
private:
  pos_t pos;
  std::vector<buffer_t> fifo;
};

fifo_t::fifo_t(uint32_t len, const buffer_t& prototype)
  : pos(len),
    fifo(len,prototype)
{
}

void fifo_t::read_advance()
{
  uint32_t nr(pos.r+1);
  if( nr>=pos.l) 
    nr=0;
  pos.r = nr;
}

void fifo_t::write_advance()
{
  uint32_t nr(pos.w+1);
  if( nr>=pos.l) 
    nr=0;
  pos.w = nr;
}

class lp_t : public wave_t {
public:
  lp_t(uint32_t n, float c);
  void filter(const wave_t& src);
private:
  float c1;
  float c2;
};

lp_t::lp_t(uint32_t n, float c)
  : wave_t(n),c1(c),c2(1.0f-c)
{
}

void lp_t::filter(const wave_t& src)
{
  for(unsigned int k=0;k<std::min(n_,src.n_);k++)
    b[k] = c1*b[k]+c2*src.b[k];
}

class irs_recorder_t : public jackc_t {
public:
  irs_recorder_t(const std::string& clientname,uint32_t len, uint32_t fifolen);
  ~irs_recorder_t(){
    pthread_mutex_trylock( &mtx );
    pthread_mutex_unlock(  &mtx );
    pthread_mutex_destroy( &mtx );
  };
  int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer);
  void start_service();
  void stop_service();
  void run();
  spec_t& get_lock_z(){
    pthread_mutex_lock( &mtx );
    return z;
  };
  void unlock_z(){
    pthread_mutex_unlock( &mtx );
  };
private:
  static void * service(void *);
protected:
  void service();
  void process_buffer(const buffer_t&);
private:
  buffer_t prototype;
  fifo_t rb;
  bool b_run_service;
  pthread_t srv_thread;
  uint32_t wp;
  uint32_t fftlen;
  fft_t fft1;
  fft_t fft2;
  wave_t testsig;
  lp_t lp1;
  lp_t lp2;
  pthread_mutex_t mtx;
  spec_t z;
};

irs_recorder_t::irs_recorder_t(const std::string& clientname,uint32_t len, uint32_t fifolen)
  : jackc_t(clientname),
    prototype(len),
    rb(fifolen,prototype),
    b_run_service(false),
    wp(0),
    fftlen(len),
    fft1(fftlen),
    fft2(fftlen),
    testsig(fftlen),
    lp1(fftlen,0.9),
    lp2(fftlen,0.9),
    z(fftlen/2+1)
{
  add_input_port("U");
  add_input_port("I");
  add_output_port("sweep");
  spec_t tc(fftlen/2+1);
  for(unsigned int k=0;k<tc.n_;k++)
    tc.b[k] = cexpf(I*2*M_PI*rand()/RAND_MAX);
  fft1.execute(tc);
  testsig.copy(fft1.w);
  testsig /= 2.0*testsig.maxabs();
  pthread_mutex_init( &mtx, NULL );
}

int irs_recorder_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer)
{
  //std::cerr << "*";
  buffer_t& b(rb.get_write_elem());
  for(unsigned int k=0;k<nframes;k++){
    if( wp >= b.len ){
      wp = 0;
      rb.write_advance();
      b = rb.get_write_elem();
    }
    b.w1.b[wp] = inBuffer[0][k];
    b.w2.b[wp] = inBuffer[1][k];
    outBuffer[0][k] = testsig.b[wp];
    wp++;
  }
  return 0;
}


void * irs_recorder_t::service(void* h)
{
  //DEBUG(h);
  ((irs_recorder_t*)h)->service();
  //DEBUG(h);
  return NULL;
}

void irs_recorder_t::start_service()
{
  //DEBUG(b_run_service);
  if( b_run_service )
    return;
  b_run_service = true;
  int err = pthread_create( &srv_thread, NULL, &irs_recorder_t::service, this);
  if( err < 0 )
    throw "pthread_create failed";
  //DEBUG(b_run_service);
}

void irs_recorder_t::stop_service()
{
  //DEBUG(b_run_service);
  if( !b_run_service )
    return;
  b_run_service = false;
  //DEBUG(b_run_service);
  pthread_join( srv_thread, NULL );
  //DEBUG(b_run_service);
}

void irs_recorder_t::service()
{
  //DEBUG(2);
  while(b_run_service ){
    while( rb.read_space() ){
      rb.read_advance();
      process_buffer( rb.get_read_elem() );
    }
    usleep(100);
  }
  //DEBUG(2);
  //DEBUG(b_run_service);
}

void irs_recorder_t::process_buffer(const buffer_t& b)
{
  lp1.filter(b.w1);
  lp2.filter(b.w2);
  fft1.execute(lp1);
  fft2.execute(lp2);
  z.copy(fft1.s);
  z /= fft2.s;
}

void irs_recorder_t::run()
{
  sleep(10);
  deactivate();
  stop_service();
  DEBUG(1);
  sleep(1);
}


class theremin_win_t
{
public:
  theremin_win_t();
  void draw(spec_t* s,Cairo::RefPtr<Cairo::Context> cr, double msize);
  void draw_phase(spec_t* s,Cairo::RefPtr<Cairo::Context> cr, double msize);
  void draw_abs(spec_t& s,Cairo::RefPtr<Cairo::Context> cr, double msize);
protected:
  virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
  virtual bool on_expose_event(GdkEventExpose* event);
  bool on_timeout();
  double scale;
public:
  Gtk::DrawingArea da;
  irs_recorder_t* irs;
};

theremin_win_t::theremin_win_t()
 : scale(10)
{
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &theremin_win_t::on_timeout), 60 );
  da.signal_expose_event().connect(sigc::mem_fun(*this, &theremin_win_t::on_expose_event), false);
}

bool theremin_win_t::on_expose_event(GdkEventExpose* event)
{
  if( event ){
    // This is where we draw on the window
    Glib::RefPtr<Gdk::Window> window = da.get_window();
    if(window){
      Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();
      return on_draw(cr);
    }
  }
  return true;
}

void theremin_win_t::draw(spec_t* s,Cairo::RefPtr<Cairo::Context> cr, double msize)
{
  cr->save();
  cr->set_source_rgb(1, 0, 0 );
  cr->set_line_width( 0.2*msize );
  for(unsigned int k=4;k<std::min(s->n_,1000u);k++){
    if( k==4 )
      cr->move_to( creal(s->b[k]), cimag(s->b[k]) );
    else
      cr->line_to( creal(s->b[k]), cimag(s->b[k]) );
  }
  cr->stroke();
  cr->restore();
}

void theremin_win_t::draw_phase(spec_t* s,Cairo::RefPtr<Cairo::Context> cr, double msize)
{
  cr->save();
  cr->set_source_rgb(1, 0, 0 );
  cr->set_line_width( 0.2*msize );
  for(unsigned int k=0;k<std::min(s->n_,1000u);k++){
    if( k==0 )
      cr->move_to( 0.003*k, cargf(s->b[k]) );
    else
      cr->line_to( 0.003*k, cargf(s->b[k]) );
  }
  cr->stroke();
  cr->restore();
}

void theremin_win_t::draw_abs(spec_t& s,Cairo::RefPtr<Cairo::Context> cr, double msize)
{
  cr->save();
  cr->set_source_rgb(1, 0, 0 );
  cr->set_line_width( 0.2*msize );
  for(unsigned int k=0;k<std::min(s.n_,1000u);k++){
    if( k==0 )
      cr->move_to( 0.007*k-2, cabsf(s.b[k]) );
    else
      cr->line_to( 0.007*k-2, cabsf(s.b[k]) );
  }
  cr->stroke();
  cr->restore();
}

bool theremin_win_t::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  Glib::RefPtr<Gdk::Window> window = da.get_window();
  if(window){
    Gtk::Allocation allocation = da.get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();
    cr->rectangle(0,0,width,height);
    cr->clip();
    cr->translate(0.5*width, 0.5*height);
    double wscale(0.5*std::min(height,width)/scale);
    double markersize(0.02*scale);
    cr->scale( wscale, wscale );
    cr->set_line_width( 0.3*markersize );
    cr->set_font_size( 2*markersize );
    cr->save();
    cr->set_source_rgb( 1, 1, 1 );
    cr->paint();
    cr->restore();
    if( irs ){
      spec_t& spec(irs->get_lock_z());
      //draw( spec, cr, markersize );
      //draw_phase( spec, cr, markersize );
      draw_abs( spec, cr, markersize );
      irs->unlock_z();
    }
    cr->save();
    cr->set_source_rgba(0.2, 0.2, 0.2, 0.8);
    cr->move_to(-markersize, 0 );
    cr->line_to( markersize, 0 );
    cr->move_to( 0, -markersize );
    cr->line_to( 0,  markersize );
    cr->stroke();
    cr->restore();
  }
  return true;
}

bool theremin_win_t::on_timeout()
{
  Glib::RefPtr<Gdk::Window> win = da.get_window();
  if (win){
    Gdk::Rectangle r(0,0, 
		     da.get_allocation().get_width(),
		     da.get_allocation().get_height() );
    win->invalidate_rect(r, true);
  }
  return true;
}

int main(int argc, char** argv)
{
  Gtk::Main kit(argc, argv);
  Gtk::Window win;
  win.set_title("z");
  theremin_win_t c;
  win.add(c.da);
  win.set_default_size(1024,768);
  //c.set_scale(200);
  //c.set_scale(c.guiscale);
  //win.fullscreen();
  win.show_all();
  //DEBUG(1);
  irs_recorder_t irs("impedance",4096,20);
  //DEBUG(1);
  irs.start_service();
  irs.activate();
  //irs.run();
  c.irs = &irs;
  irs.connect_in(0,"system:capture_3");
  irs.connect_in(1,"system:capture_4");
  irs.connect_out(0,"system:playback_1");
  Gtk::Main::run(win);
  irs.deactivate();
  irs.stop_service();
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
