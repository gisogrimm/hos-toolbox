#include <iostream>
#include <stdlib.h>
#include <string.h>
#include "jackclient.h"
#include <complex.h>
#include <math.h>
#include "hos_defs.h"

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
    resfilt_t flt_pre;
    resfilt_t flt_post;
    double rgain,drgain;
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
  f0 /= fs_;
  f0 *= 2.0;
  double q_pole = pow(q,f0);
  double q_zero = q_pole*q_pole;
  dA1 = (2.0*q_pole*cos(M_PI*f0) - A1)*dt;
  dA2 = (-q_pole*q_pole - A2)*dt;
  dB1 = (-2.0*q_zero - B1)*dt;
  dB2 = (q_zero*q_zero - B2)*dt;
}

cyclephase_t::cyclephase_t(const std::string& name)
  : jackc_t(name),
    b_quit(false),
    flt_pre(srate,fragsize),
    flt_post(srate,fragsize),
    rgain(0.0),drgain(0.0),
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

inline double randd()
{
  return (double)rand()/(double)RAND_MAX;
}

inline double limit(double x,double xmin,double xmax)
{
  return std::min(xmax,std::max(xmin,x));
}

int cyclephase_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer)
{
  float* v_x(inBuffer[0]);
  float* v_f(inBuffer[1]);
  float* v_q(inBuffer[2]);
  float* v_y(outBuffer[0]);
  flt_pre.update(f_min*pow(f_max/f_min,limit(v_f[0],0,1)),limit(0.5*v_q[0],0,0.99));
  flt_post.update(f_min*pow(f_max/f_min,limit(v_f[0],0,1)),limit(v_q[0],0,0.99));
  drgain = (v_q[0]-rgain)/nframes;
  for (jack_nframes_t i = 0; i < nframes; ++i){
    v_y[i] = flt_post.filter(flt_pre.filter(v_x[i])*(1.0-(rgain+=drgain)*randd()));
  }
  return 0;
}

void cyclephase_t::run()
{
  jackc_t::activate();
  //connect_in(0,"japa:pink",true);
  //connect_in(1,"osc2jack:/f",true);
  //connect_in(2,"osc2jack:/q",true);
  //connect_out(0,"japa:in_1",true);
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
