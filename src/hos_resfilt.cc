#include <iostream>
#include <stdlib.h>
#include <string.h>
#include "jackclient.h"
#include <complex.h>
#include <math.h>
#include "defs.h"

#define PI2 6.283185307179586232
#define PI2INV 0.15915494309189534561

namespace HoS {

  class resfilt_t {
  public:
    resfilt_t(double fs, double fragsize);
    void update(double f0, double q);
    inline double filter(double x){
      y0 = (A1+=dA1)*y1 + (A2+=dA2)*y2 + x + (B1+=dB1)*x1 + (B2+=dB2)*x2;
      y2 = y1;
      y1 = y0;
      x2 = x1;
      x1 = x;
      return y0;
    }
  private:
    double y0, y1, y2;
    double x1, x2;
    double A1;
    double A2;
    double dA1;
    double dA2;
    double B1;
    double B2;
    double dB1;
    double dB2;
    double dt;
    double fs_;
  };

  /**
     \ingroup apphos
  */
  class cyclephase_t : public jackc_t {
  public:
    cyclephase_t(const std::string& name);
    ~cyclephase_t();
    void run();
    void quit() { b_quit = true;};
  private:
    int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer);
    bool b_quit;
    resfilt_t flt;
    double f_min, f_max;
  };

}

using namespace HoS;

resfilt_t::resfilt_t(double fs, double fragsize)
  : y0(0), y1(0), y2(0),
    x1(0), x2(0),
    A1(0), A2(0), 
    dA1(0), dA2(0),
    B1(0), B2(0), 
    dB1(0), dB2(0),
    dt(1.0/fragsize), fs_(fs)
{
}

void resfilt_t::update(double f0, double q)
{
  //DEBUG(f0);
  //DEBUG(q*cos(PI2*f0/fs_));
  dA1 = (2.0*q*cos(PI2*f0/fs_) - A1)*dt;
  dA2 = (-q*q - A2)*dt;
  dB1 = (-2.0*q - B1)*dt;
  dB2 = (q*q - B2)*dt;
}

cyclephase_t::cyclephase_t(const std::string& name)
  : jackc_t(name),
    b_quit(false),
    flt(srate,fragsize),
    f_min(100.0), f_max(4000.0)
{
  add_input_port("x");
  add_input_port("f");
  add_input_port("q");
  add_output_port("y");
}

cyclephase_t::~cyclephase_t()
{
}


int cyclephase_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer)
{
  float* v_x(inBuffer[0]);
  float* v_f(inBuffer[1]);
  float* v_q(inBuffer[2]);
  float* v_y(outBuffer[0]);
  flt.update(f_min*pow(f_max/f_min,v_f[0]),v_q[0]);
  for (jack_nframes_t i = 0; i < nframes; ++i)
    v_y[i] = flt.filter(v_x[i]);
  return 0;
}

void cyclephase_t::run()
{
  jackc_t::activate();
  while( !b_quit ){
    sleep( 1 );
  }
  jackc_t::deactivate();
}

int main(int argc, char** argv)
{
  std::string name("resflt");
  if( argc > 1 )
    name = argv[1];
  cyclephase_t S(name);
  S.run();
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
