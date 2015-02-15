/**
   \file hos_foacasa.cc
   \ingroup apphos
   \brief First order ambisonics simple CASA algorithm
   \author Giso Grimm
   \date 2014

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
#include "libhos_audiochunks.h"
#include "hos_defs.h"
#include <getopt.h>
#include "lininterp.h"
#include "libhos_random.h"
#include "errorhandling.h"
#include "gammatone.h"

#define OSC_ADDR "239.255.1.7"
#define OSC_PORT "6978"
#define HIST_SIZE 256

float downhill_iterate(float eps,std::vector<float>& param,float (*err)(const std::vector<float>& x,void* data),void* data,const std::vector<float>& unitstep)
{
  std::vector<float> stepparam(param);
  float errv(err(param,data));
  for(uint32_t k=0;k<param.size();k++){
    stepparam[k] += unitstep[k];
    float dv(eps*(errv - err(stepparam,data)));
    stepparam[k] = param[k];
    param[k] += dv;
  }
  return errv;
}

class freqinfo_t {
public:
  freqinfo_t(float bpo,float fmin,float fmax);
  uint32_t bands;
  std::vector<float> fc;
  std::vector<float> fe;
  float band(float f_hz);
private:
  float bpo_;
  float fmin_;
  float fmax_;
};

class mod_analyzer_t {
public:
  mod_analyzer_t(float fs,uint32_t nchannels,float bpo);
  void update(float x);
  float get_modoct() const { return param[0];};
  float get_modbw() const { return param[1];};
  static float errfun(const std::vector<float>&,void* data);
  float errfun(const std::vector<float>&);
private:
  std::vector<gammatone_t> fb;
  std::vector<float> param;
  std::vector<float> val;
  std::vector<float> unitstep;
  float oval;
};

mod_analyzer_t::mod_analyzer_t(float fs,uint32_t nchannels,float bpo)
  : oval(0)
{
  for(uint32_t k=0;k<nchannels;k++){
    
    float f(0.5*fs / pow(2.0,(float)k/bpo));
    float bw(pow(2.0,0.5*bpo));
    bw = f*(bw-1.0/bw);
    fb.push_back(gammatone_t(f,bw,fs,3));
  }
  param.resize(3);
  param[1] = 1;
  param[2] = 1;
  unitstep.resize(3);
  unitstep[0] = 0.1;
  unitstep[1] = 0.1;
  unitstep[2] = 0.1;
  val.resize(nchannels);
}

void mod_analyzer_t::update(float x)
{
  float tmp(x);
  x = x-oval;
  oval = tmp;
  // update filterbank:
  float vsum(0);
  for(uint32_t k=0;k<fb.size();k++)
    vsum += (val[k] = cabs(fb[k].filter(x)));
  if( vsum > 0 )
    for(uint32_t k=0;k<fb.size();k++)
      val[k] /= vsum;
  // parameter fitting:
  downhill_iterate(1,param,&mod_analyzer_t::errfun,this,unitstep);
  param[0] = std::max(0.0f,std::min((float)(fb.size()-1),param[0]));
  param[1] = std::max(0.1f,std::min((float)(fb.size()),param[1]));
  param[2] = std::max(0.1f,std::min(10.0f,param[2]));
  //for(uint32_t k=0;k<val.size();k++)
  //  std::cout << val[k] << " ";
  //std::cout << std::endl;
  //for(uint32_t k=0;k<param.size();k++)
  //  std::cout << param[k] << " ";
  //std::cout << std::endl;
}

float mod_analyzer_t::errfun(const std::vector<float>& p,void* data)
{
  return ((mod_analyzer_t*)(data))->errfun(p);
}

float mod_analyzer_t::errfun(const std::vector<float>& p)
{
  float err(0.0);
  for(uint32_t k=0;k<val.size();k++){
    float lerr(val[k]-p[2]*gauss((double)k-p[0],p[1]));
    err += lerr*lerr;
  }
  return err;
}


class xyfield_t {
public:
  xyfield_t(uint32_t sx,uint32_t sy);
  xyfield_t(const xyfield_t& src);
  ~xyfield_t();
  float& val(uint32_t px,uint32_t py);
  float val(uint32_t px,uint32_t py) const;
  uint32_t sizex() const {return sx_;};
  uint32_t sizey() const {return sy_;};
  uint32_t size() const {return s_;};
  float min() const;
  float max() const;
  void operator+=(float x);
  void operator*=(float x);
private:
  uint32_t sx_;
  uint32_t sy_;
  uint32_t s_;
  float* data;
};

void xyfield_t::operator+=(float x)
{
  for(uint32_t k=0;k<s_;k++)
    data[k] += x;
}

void xyfield_t::operator*=(float x)
{
  for(uint32_t k=0;k<s_;k++)
    data[k] *= x;
}

float xyfield_t::min() const
{
  float r(data[0]);
  for(uint32_t k=1;k<s_;k++)
    r = std::min(data[k],r);
  return r;
}

float xyfield_t::max() const
{
  float r(data[0]);
  for(uint32_t k=1;k<s_;k++)
    r = std::max(data[k],r);
  return r;
}

xyfield_t::xyfield_t(const xyfield_t& src)
  : sx_(src.sx_),sy_(src.sy_),s_(src.s_),data(new float[s_])
{
  for(uint32_t k=0;k<s_;k++)
    data[k] = src.data[k];
}

xyfield_t::xyfield_t(uint32_t sx,uint32_t sy)
  : sx_(sx),sy_(sy),s_(std::max(1u,sx*sy)),data(new float[s_])
{
  for(uint32_t k=0;k<s_;k++)
    data[k] = 0;
}

xyfield_t::~xyfield_t()
{
  delete [] data;
}

float& xyfield_t::val(uint32_t px,uint32_t py)
{
  return data[px+sx_*py];
}

float xyfield_t::val(uint32_t px,uint32_t py) const
{
  return data[px+sx_*py];
}

class objmodel_t : public xyfield_t {
public:
  class param_t {
  public:
    param_t();
    param_t(uint32_t num,const std::vector<float>& vp);
    void setp(uint32_t num,std::vector<float>& v);
    float cx;
    float cy;
    float wx;
    float wy;
    float g;
  };
  void send_osc(const lo_address& lo_addr);
  objmodel_t(uint32_t sx,uint32_t sy,uint32_t numobj,float bpo,float fmin,const std::vector<std::string>& names,bool nosortobjects_);
  float objval(float x,float y,param_t lp);
  float objval(float x,float y,const std::vector<float>&);
  float objval(float x,float y);
  static float errfun(const std::vector<float>&,void* data);
  float errfun(const std::vector<float>&);
  void iterate();
  uint32_t size() const {return nobj;};
  const std::vector<float>& param() const { return obj_param;};
  param_t param(uint32_t k) const { return param_t(k,obj_param);};
  float geterror(){ return error;};
  //void bayes_prob(uint32_t kx,uint32_t ky,std::vector<float>& p);
  float bayes_prob(float x,float y,uint32_t ko);
private:
  float error;
  uint32_t nobj;
  std::vector<float> obj_param;
  std::vector<float> unitstep;
  float xscale;
  //std::vector<xyfield_t> bayes_table;
  uint32_t calls;
  std::vector<std::string> paths_pitch;
  std::vector<std::string> paths_bw;
  std::vector<std::string> paths_az;
  float bpo_;
  float fmin_;
  std::vector<param_t> vpar;
  bool nosortobjects;
};

bool param_less_y(objmodel_t::param_t i,objmodel_t::param_t j){
  return (i.cy>j.cy+1); 
}

bool param_less_x(objmodel_t::param_t i,objmodel_t::param_t j){
  return (i.cx>j.cx); 
}

void objmodel_t::send_osc(const lo_address& lo_addr)
{
  // 0 equals c in low pitch (a=415 Hz):
  //float delta(bpo_*log2f(246.8f/fmin_));
  float delta(bpo_*log2f(0.5*fmin_/246.8f));
  for(uint32_t ko=0;ko<nobj;ko++){
    param_t par(param(ko));
    lo_send(lo_addr,paths_pitch[ko].c_str(),"f",12.0f/bpo_*(par.cy+delta));
    lo_send(lo_addr,paths_bw[ko].c_str(),"f",12.0f/bpo_*par.wy);
    lo_send(lo_addr,paths_az[ko].c_str(),"f",RAD2DEG*(PI2*par.cx/sizex()-M_PI));
  }
}

objmodel_t::param_t::param_t()
  : cx(0),cy(0),wx(3),wy(2),g(1)
{
}

objmodel_t::param_t::param_t(uint32_t num,const std::vector<float>& vp)
{
  cx = vp[5*num];
  cy = vp[5*num+1];
  wx = vp[5*num+2];
  wy = vp[5*num+3];
  g = vp[5*num+4];
  //wx = vp[4*num+2];
  //wy = vp[4*num+3];
}

void objmodel_t::param_t::setp(uint32_t num,std::vector<float>& vp)
{
  vp[5*num] = cx;
  vp[5*num+1] = cy;
  vp[5*num+2] = wx;
  vp[5*num+3] = wy;
  vp[5*num+4] = g;
}

objmodel_t::objmodel_t(uint32_t sx,uint32_t sy,uint32_t numobj,float bpo,float fmin,const std::vector<std::string>& names,bool nosortobjects_)
  : xyfield_t(sx,sy),error(0),nobj(numobj),xscale(PI2/(float)sx),bpo_(bpo),fmin_(fmin),nosortobjects(nosortobjects_)
{
  calls = 0;
  obj_param.resize(5*nobj);
  unitstep.resize(5*nobj);
  for(uint32_t k=0;k<nobj;k++){
    paths_pitch.push_back(std::string("/")+names[k]+std::string("/pitch"));
    paths_bw.push_back(std::string("/")+names[k]+std::string("/bw"));
    paths_az.push_back(std::string("/")+names[k]+std::string("/az"));
    param_t par;
    // center x:
    par.cx = sx*drand();
    // center y:
    par.cy = sy*drand();
    par.setp(k,obj_param);
    // center x:
    unitstep[5*k] = 0.01;
    // center y:
    unitstep[5*k+1] = 0.1;
    unitstep[5*k+2] = 0.01;
    unitstep[5*k+3] = 0.01;
    unitstep[5*k+4] = 10;
    // exponent x:
    //unitstep[4*k+2] = 0.001;
    // bandwidth y:
    //unitstep[4*k+3] = 0.01;
    //bayes_table.push_back(xyfield_t(sx,sy));
  }
  vpar.resize(nobj);
}

float objmodel_t::objval(float x,float y,param_t lp) 
{
  calls++;
  //float g(val(std::max(0.0f,std::min(lp.cx,(float)(sizex()))),
  //            std::max(0.0f,std::min(lp.cy,(float)(sizey())))));
  lp.cy = y-lp.cy;
  lp.cy /= lp.wy*1.4142135623730f;
  lp.cy *= lp.cy;
  //return g*powf(0.5+0.5*cosf((x-lp.cx)*PI2),lp.wx);
  return lp.g*powf(0.5+0.5*cosf((x-lp.cx)*xscale),lp.wx)*expf(-lp.cy);
}

float objmodel_t::objval(float x,float y,const std::vector<float>& p)
{
  float rv(0.0f);
  for(uint32_t k=0;k<nobj;k++)
    rv += objval(x,y,param_t(k,p));
  return rv;
}

float objmodel_t::objval(float x,float y)
{
  return objval(x,y,obj_param);
}

float objmodel_t::bayes_prob(float x,float y,uint32_t ko)
{
  return objval(x,y,param_t(ko,obj_param))/objval(x,y);
}

float objmodel_t::errfun(const std::vector<float>& p,void* data)
{
  return ((objmodel_t*)(data))->errfun(p);
}

float objmodel_t::errfun(const std::vector<float>& p)
{
  float err(0.0);
  for(uint32_t kx=0;kx<sizex();kx++)
    for(uint32_t ky=0;ky<sizey();ky++){
      float lerr(val(kx,ky)-objval(kx,ky,p));
      err += lerr*lerr;
    }
  return err;
}

void objmodel_t::iterate()
{
  calls = 0;
  error = downhill_iterate(0.0002,obj_param,&objmodel_t::errfun,this,unitstep);
  // constraints:
  for(uint32_t k=0;k<nobj;k++){
    param_t par(k,obj_param);
    while( par.cx < 0 )
      par.cx += sizex();
    while( par.cx >= sizex() )
      par.cx -= sizex();
    if( par.cy < 0.0f )
      par.cy = 0.0f;
    if( par.cy > sizey()-1 )
      par.cy = sizey()-1.0f;
    if( par.wx < 3 )
      par.wx = 3;
    if( par.wx > 8 )
      par.wx = 8;
    if( par.wy < 4 )
      par.wy = 4;
    if( par.wy > 32 )
      par.wy = 32;
    if( par.g < 20 )
      par.g = 20;
    //par.wx = 2;
    //par.wy = 32;
    vpar[k] = par;
  }
  // average gains to describe similar sources:
  if( true ){
    double avg_g(vpar[0].g);
    for(uint32_t k=1;k<nobj;k++){
      avg_g += vpar[k].g;
    }
    avg_g /= nobj;
    for(uint32_t k=0;k<nobj;k++){
      vpar[k].g = avg_g;
    }
  }
  // sort:
  if( !nosortobjects ){
    std::sort(vpar.begin(),vpar.end(),param_less_y);
  }else{
    std::sort(vpar.begin(),vpar.end(),param_less_x);
  }
  for(uint32_t k=0;k<nobj;k++)
    vpar[k].setp(k,obj_param);
}

class az_hist_t : public HoS::wave_t
{
public:
  az_hist_t(uint32_t size);
  void update();
  bool add(float f, float az,float weight);
  void add(float f, float _Complex W, float _Complex X, float _Complex Y, float weight);
  void set_tau(float tau,float fs);
  void draw(const Cairo::RefPtr<Cairo::Context>& cr,float scale);
  void set_frange(float f1,float f2);
private:
  float c1;
  float c2;
  float fmin;
  float fmax;
  float dec_w;
  HoS::wave_t dec_x;
  HoS::wave_t dec_y;
};

az_hist_t::az_hist_t(uint32_t size)
  : HoS::wave_t(size),
    dec_x(size),
    dec_y(size)
{
  set_tau(1.0f,1.0f);
  set_frange(0,22050);
  // 1st order decoder type:
  // basic = 1
  // max-rE(2D) = 0.707
  float dec_gain(1.0);
  dec_gain = 0.707;
  dec_w = 1.0/sqrt(2.0);
  for(uint32_t k=0;k<size;k++){
    float az((float)k*2.0*M_PI/(float)size);
    dec_x[k] = dec_gain*cos(az);
    dec_y[k] = dec_gain*sin(az);
  }
}

void az_hist_t::update()
{
  *this *= c1;
}

bool az_hist_t::add(float f, float az, float weight)
{
  if( (f >= fmin) && (f < fmax) ){
    az += M_PI;
    az *= (0.5*size()/M_PI);
    uint32_t iaz(std::max(0.0f,std::min((float)(size()-1),az)));
    operator[](iaz) += c2*weight;
    return true;
  }
  return false;
}

void az_hist_t::add(float f, float _Complex W, float _Complex X, float _Complex Y, float weight)
{
  if( (f >= fmin) && (f < fmax) ){
    for(uint32_t k=0;k<size();k++){
      float t(crealf(dec_w*W+dec_x[k]*X+dec_y[k]*Y));
      t *= t;
      operator[](k) += c2*t*weight;
    }
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
  for(uint32_t k=0;k<=size();k++){
    float arg(2.0*M_PI*k/size());
    float r(scale*operator[](k % size()));
    float x(r*cos(arg));
    float y(r*sin(arg));
    if( k==0)
      cr->move_to(x,y);
    else
      cr->line_to(x,y);
  }
  cr->stroke();
}

freqinfo_t::freqinfo_t(float bpo,float fmin,float fmax)
  : bands(bpo*log2(fmax/fmin)+1.0),bpo_(bpo),fmin_(fmin),fmax_(fmax)
{
  DEBUG(bands);
  float f_ratio(pow(fmax/fmin,0.5/(double)(bands-1)));
  for(uint32_t k=0;k<bands;k++){
    fc.push_back(fmin*pow(fmax/fmin,(double)k/(double)(bands-1)));
    fe.push_back(fc.back()/f_ratio);
  }
  fe.push_back(fc.back()*f_ratio);
}

float freqinfo_t::band(float f_hz)
{
  return bpo_*log2(std::max(40.0f,f_hz)/fmin_);
}

namespace HoSGUI {

  class foacoh_t : public freqinfo_t, public Gtk::DrawingArea, public TASCAR::osc_server_t, public jackc_db_t
  {
  public:
    foacoh_t(const std::string& name,uint32_t channels,float bpo,float fmin,float fmax,const std::vector<std::string>& objnames,uint32_t periodsize,const std::string& url,bool nosortobjects);
    virtual ~foacoh_t();
    virtual int inner_process(jack_nframes_t, const std::vector<float*>&, const std::vector<float*>&);
    void activate();
    void deactivate();
  protected:
    //Override default signal handler:
    virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
    bool on_timeout();
    uint32_t periodsize;
    uint32_t fftlen;
    uint32_t wndlen;
    HoS::ola_t ola_w;
    HoS::ola_t ola_x;
    HoS::ola_t ola_y;
    std::vector<HoS::ola_t*> ola_obj;
    // low pass filter coefficients:
    HoS::wave_t lp_c1;
    HoS::wave_t lp_c2;
    // complex temporary coherence:
    HoS::spec_t ccohXY;
    //HoS::spec_t ccoh2;
    // coherence functions:
    HoS::wave_t cohXY;
    //HoS::wave_t cohReXY;
    HoS::wave_t cohXWYW;
    HoS::wave_t az;
    std::vector<az_hist_t> haz;
    //az_hist_t haz2;
    std::string name_;
    //float haz_c1;
    //float haz_c2;
    float fscale;
    Glib::RefPtr<Gdk::Pixbuf> image;
    Glib::RefPtr<Gdk::Pixbuf> image_mod;
    bool draw_image;
    colormap_t col;
    uint32_t azchannels;
    objmodel_t obj;
    std::vector<mod_analyzer_t> modflt;
    float vmin;
    float vmax;
    float objlp_c1;
    float objlp_c2;
    //std::vector<float> bayes_prob;
    std::vector<float> f2band;
    lo_address lo_addr;
    std::vector<std::string> names;
    std::vector<std::string> paths_modf;
    std::vector<std::string> paths_modbw;
    uint32_t send_cnt;
  };

}

using namespace HoSGUI;

int foacoh_t::inner_process(jack_nframes_t n, const std::vector<float*>& vIn, const std::vector<float*>& vOut)
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
    //cohReXY[k] *= lp_c1[k];
    //if( cXYabs > 0 ){
    //  cohReXY[k] += lp_c2[k]*fabsf(crealf(cXY))/cXYabs;
    //}
    //cohReXY[k] = fabsf(creal(ccohXY[k]));
    //// Measure 3: (X/W)^2 + (Y/W)^2 = 2
    cohXWYW[k] = std::min(1.0f,cohXWYW[k]);
    cohXWYW[k] *= lp_c1[k];
    if( cabsf(cW) > 0 ){
      cX /= cW;
      cY /= cW;
      cohXWYW[k] += lp_c2[k]*0.25*cabsf(cX*conjf(cX) + cY*conjf(cY));
    }
    az[k] = cargf(cX+I*cY);
    float w(powf(cabsf(cW)*cohXY[k],2.0));
    float l_az(az[k]+M_PI);
    l_az *= (0.5*haz.size()/M_PI);
    //uint32_t iaz(std::max(0.0f,std::min((float)(haz.size()-1),l_az)));
    //for(uint32_t kobj=0;kobj<obj.size();kobj++)
    //  bayes_prob[kobj] = 0;
    for(uint32_t kH=0;kH<haz.size();kH++)
      haz[kH].add(freq,az[k],w);
    //    obj.bayes_prob(iaz,kH,bayes_prob);
    //  }
    //// bin-wise Bayes classification:
    //for(uint32_t kobj=0;kobj<obj.size();kobj++)
      //ola_obj[kobj]->s[k] = cohXY[k]*cW*bayes_prob[kobj];
      //ola_obj[kobj]->s[k] = cW*bayes_prob[kobj];
  }
  if( send_cnt == 0 ){
    obj.send_osc(lo_addr);
    send_cnt = 10;
  }else
    send_cnt--;
  for(uint32_t kobj=0;kobj<obj.size();kobj++){
    HoS::wave_t outW(n,vOut[kobj]);
    objmodel_t::param_t par(obj.param(kobj));
    float az(PI2*par.cx/(float)azchannels-M_PI);
    float wx(cos(az));
    float wy(sin(az));
    float env(0);
    for(uint32_t k=0;k<ola_w.s.size();k++){
      // by not using W channel gain, this is max-rE FOA decoder:
      ola_obj[kobj]->s[k] = (ola_w.s[k]+wx*ola_x.s[k]+wy*ola_y.s[k])*obj.bayes_prob(par.cx,f2band[k],kobj);
      float aspec(cabs(ola_obj[kobj]->s[k]));
      env += aspec*aspec;
    }
    env = sqrt(env);
    modflt[kobj].update(env);
    //lo_send(lo_addr,paths_modf[kobj].c_str(),"f",modflt[kobj].get_modoct());
    //lo_send(lo_addr,paths_modbw[kobj].c_str(),"f",modflt[kobj].get_modbw());
    //std::cout << " " << modflt[kobj].get_modoct();
    //ola_obj[kobj]->s[k] = ola_w.s[k];
    ola_obj[kobj]->s[0] = creal(ola_obj[kobj]->s[0]);
    ola_obj[kobj]->ifft(outW);
  }
  //std::cout << "\n";
  for(uint32_t kb=0;kb<bands;kb++){
    for(uint32_t kc=0;kc<azchannels;kc++){
      obj.val(kc,kb) = 10.0f*log10f(std::max(1.0e-10f,haz[kb][kc]));
    }
    //if( sum > 0 ){
    //  sum = (float)azchannels/sum;
    //  for(uint32_t kc=0;kc<azchannels;kc++){
    //    obj.val(kc,kb) *= sum;
    //  }
    //}
  }
  vmin = objlp_c1*vmin+objlp_c2*obj.min();
  vmax = std::max(vmin+0.1f,objlp_c1*vmax+objlp_c2*obj.max());
  obj += -vmin;
  obj *= 100.0/(vmax-vmin);
  obj.iterate();
  return 0;
}

foacoh_t::foacoh_t(const std::string& name,uint32_t channels,float bpo,float fmin,float fmax,const std::vector<std::string>& objnames,uint32_t periodsize_,const std::string& url,bool nosortobjects)
  : freqinfo_t(bpo,fmin,fmax),
    osc_server_t(OSC_ADDR,OSC_PORT),
    jackc_db_t("foacoh",periodsize_),
    periodsize(periodsize_),
    fftlen(std::max(512u,4*periodsize)),
    wndlen(std::max(256u,2*periodsize)),
    ola_w(fftlen,wndlen,periodsize,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_HANNING),
    ola_x(fftlen,wndlen,periodsize,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_HANNING),
    ola_y(fftlen,wndlen,periodsize,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_HANNING),
    lp_c1(ola_x.s.size()),
    lp_c2(ola_x.s.size()),
    ccohXY(ola_x.s.size()),
    cohXY(ola_x.s.size()),
    cohXWYW(ola_x.s.size()),
    az(ola_x.s.size()),
    name_(name),
    fscale(get_srate()/(float)fftlen),
    draw_image(true),
    col(0),
    azchannels(channels),
  obj(channels,bands,objnames.size(),bpo,fmin,objnames,nosortobjects),
    vmin(0),vmax(1),
    lo_addr(lo_address_new_from_url(url.c_str())),
    names(objnames),
    send_cnt(2)
{
  lo_address_set_ttl( lo_addr, 1 );
  image = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB,false,8,channels,bands);
  image_mod = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB,false,8,channels,bands);
  add_input_port("in.0w");
  add_input_port("in.1x");
  add_input_port("in.1y");
  //add_output_port("diffuse.0w");
  //add_output_port("diffuse.1x");
  //add_output_port("diffuse.1y");
  for(uint32_t k=0;k<ola_w.s.size();k++)
    f2band.push_back(band((float)k*fscale));
  float frame_rate(get_srate()/(float)periodsize);
  for(uint32_t ko=0;ko<objnames.size();ko++){
    add_output_port(names[ko].c_str());
    ola_obj.push_back(new HoS::ola_t(fftlen,wndlen,periodsize,HoS::stft_t::WND_HANNING,HoS::stft_t::WND_HANNING));
    //bayes_prob.push_back(0.0f);
    modflt.push_back(mod_analyzer_t(frame_rate,4,1));
    paths_modf.push_back(std::string("/")+names[ko]+std::string("/modf"));
    paths_modbw.push_back(std::string("/")+names[ko]+std::string("/modbw"));
  }
  //fscale = 0.5*get_srate()/lp_c1.size();
  // coherence/azimuth estimation smoothing: 40 ms
  for(uint32_t k=0;k<lp_c1.size();k++){
    float f(fscale*std::max(k,1u));
    float tau(std::min(0.5f,std::max(0.05f,150.0f/f)));
    tau = 0.04;
    lp_c1[k] = exp( -1.0/(tau * frame_rate) );
    lp_c2[k] = 1.0f-lp_c1[k];
  }
  for(uint32_t k=0;k<az.size();k++)
    az[k] = 0.0;
  // intensity accumulators:
  // tau = 250/fc, max 1s, min 125ms
  for(uint32_t kH=0;kH<bands;kH++){
    haz.push_back(az_hist_t(channels));
    haz.back().set_tau(std::max(0.125f,std::min(1.0f,500.0f/fc[kH])),frame_rate);
    haz.back().set_frange(fe[kH],fe[kH+1]);
  }
  objlp_c1 = exp( -1.0/(0.5 * frame_rate) );
  objlp_c2 = 1.0f-objlp_c1;
  col.clim(-1,100);
  set_prefix("/"+name);
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &foacoh_t::on_timeout), 40 );
#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  //Connect the signal handler if it isn't already a virtual method override:
  signal_draw().connect(sigc::mem_fun(*this, &mixergui_t::on_draw), false);
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
}

void foacoh_t::activate()
{
  jackc_db_t::activate();
  osc_server_t::activate();
  try{
    connect_in(0,"mhwe:out1");
    connect_in(1,"mhwe:out2");
    connect_in(2,"mhwe:out3");
    //
    //connect_in(0,"ardour:amb_1st/out 1");
    //connect_in(1,"ardour:amb_1st/out 2");
    //connect_in(2,"ardour:amb_1st/out 3");
    //connect_out(0,"ambdec:in.0w");
    //connect_out(1,"ambdec:in.1x");
    //connect_out(2,"ambdec:in.1y");
  }
  catch( const std::exception& e ){
    std::cerr << "Warning: " << e.what() << std::endl;
  };
  try{
    connect_in(0,"tetraproc:B-form.W");
    connect_in(1,"tetraproc:B-form.X");
    connect_in(2,"tetraproc:B-form.Y");
    //
    //connect_in(0,"ardour:amb_1st/out 1");
    //connect_in(1,"ardour:amb_1st/out 2");
    //connect_in(2,"ardour:amb_1st/out 3");
    //connect_out(0,"ambdec:in.0w");
    //connect_out(1,"ambdec:in.1x");
    //connect_out(2,"ambdec:in.1y");
  }
  catch( const std::exception& e ){
    std::cerr << "Warning: " << e.what() << std::endl;
  };
}


void foacoh_t::deactivate()
{
  osc_server_t::deactivate();
  jackc_db_t::deactivate();
}

foacoh_t::~foacoh_t()
{
  image.clear();
  image_mod.clear();
  for(uint32_t k=0;k<ola_obj.size();k++)
    delete ola_obj[k];
}
  
bool foacoh_t::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  Gtk::Allocation allocation = get_allocation();
  const int width = allocation.get_width();
  const int height = allocation.get_height();
  float vmax(-1e8);
  float vmin(1e8);
  if( draw_image ){
    guint8* pixels = image->get_pixels();
    for(uint32_t k=0;k<azchannels;k++)
      for(uint32_t b=0;b<bands;b++){
        uint32_t pix(k+(bands-b-1)*azchannels);
        //uint32_t pix(bands*k+b);
        //float val(10.0f*log10f(haz[b][k]));
        float val(obj.val(k,b));
        if( val > vmax ){
          vmax = val;
        }
        if( val < vmin ){
          vmin = val;
        }
        pixels[3*pix] = col.r(val)*255;
        pixels[3*pix+1] = col.g(val)*255;
        pixels[3*pix+2] = col.b(val)*255;
      }
    pixels = image_mod->get_pixels();
    for(uint32_t k=0;k<azchannels;k++)
      for(uint32_t b=0;b<bands;b++){
        uint32_t pix(k+(bands-b-1)*azchannels);
        //uint32_t pix(bands*k+b);
        float val(obj.objval(k,b));
        pixels[3*pix] = col.r(val)*255;
        pixels[3*pix+1] = col.g(val)*255;
        pixels[3*pix+2] = col.b(val)*255;
      }
    //std::cout << vmin << " " << vmax << std::endl;
    //std::cout << obj.geterror();
    cr->save();
    cr->scale(0.5*(double)width/(double)azchannels,(double)height/(double)bands);
    Gdk::Cairo::set_source_pixbuf(cr, image, 0, 0);
    //(width - image->get_width())/2, (height - image->get_height())/2);
    cr->paint();
    Gdk::Cairo::set_source_pixbuf(cr, image_mod, azchannels,0);
    cr->paint();
    cr->set_font_size(azchannels*0.1);
    for(uint32_t ko=0;ko<obj.size();ko++){
      objmodel_t::param_t par(obj.param(ko));
      //std::cout << " " << par.cx << " " << par.cy << " " << par.wx << " " << par.wy << " " << par.g;
      cr->set_line_width(0.2);
      cr->set_source_rgb( 1, 1, 1 );
      cr->move_to(par.cx+0.5,bands-(par.cy-0.5*par.wy)-1);
      cr->line_to(par.cx+0.5,bands-(par.cy+0.5*par.wy)-1);
      cr->stroke();
      cr->move_to(par.cx-0.5/par.wx+0.5,bands-par.cy-1);
      cr->line_to(par.cx+0.5/par.wx+0.5,bands-par.cy-1);
      cr->stroke();
      cr->move_to(par.cx+0.3+0.5,bands-par.cy-2 );
      char ctmp[16];
      sprintf(ctmp,"%d",ko+1);
      cr->show_text(ctmp);
    }
    //std::cout << std::endl;
    cr->restore();
  }else{
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

void usage(struct option * opt)
{
  std::cout << "Usage:\n\nhos_foacasa [options]\n\nOptions:\n\n";
  while( opt->name ){
    std::cout << "  -" << (char)(opt->val) << " " << (opt->has_arg?"#":"") <<
      "\n  --" << opt->name << (opt->has_arg?"=#":"") << "\n\n";
    opt++;
  }
}

int main(int argc, char** argv)
{
  Gtk::Main kit(argc, argv);
  Gtk::Window win;
  std::string jackname("casa");
  std::string desturl("osc.udp://239.255.1.7:6978/");
  uint32_t channels(8);
  float bpoctave(3);
  float fmin(125);
  float fmax(4000);
  uint32_t periodsize(1024);
  bool nosortobjects(false);
  std::vector<std::string> objnames;
  const char *options = "hj:c:b:l:u:p:d:o";
  struct option long_options[] = { 
    { "help",      0, 0, 'h' },
    { "jackname",  1, 0, 'j' },
    { "desturl",   1, 0, 'd' },
    { "channels",  1, 0, 'c' },
    { "bpoctave",  1, 0, 'b' },
    { "fmin",      1, 0, 'l' },
    { "fmax",      1, 0, 'u' },
    { "periodsize",1, 0, 'p'},
    { "nosortobjects",0, 0, 'o'},
    { 0, 0, 0, 0 }
  };
  int opt(0);
  int option_index(0);
  while( (opt = getopt_long(argc, argv, options,
                            long_options, &option_index)) != -1){
    switch(opt){
    case 'h':
      usage(long_options);
      return -1;
    case 'j':
      jackname = optarg;
      break;
    case 'd':
      desturl = optarg;
      break;
    case 'c':
      channels = atoi(optarg);
      break;
    case 'b':
      bpoctave = atof(optarg);
      break;
    case 'l':
      fmin = atof(optarg);
      break;
    case 'u':
      fmax = atof(optarg);
      break;
    case 'p':
      periodsize = atoi(optarg);
      break;
    case 'o':
      nosortobjects = true;
      break;
    }
  }
  while( optind < argc )
    objnames.push_back(argv[optind++]);
  if( objnames.empty() ){
    for(uint32_t k=0;k<5;k++){
      char ctmp[1024];
      sprintf(ctmp,"obj%d",k+1);
      objnames.push_back(ctmp);
    }
  }
  win.set_title(jackname);
  HoSGUI::foacoh_t c(jackname,channels,bpoctave,fmin,fmax,objnames,periodsize,desturl,nosortobjects);
  win.add(c);
  win.set_default_size(640,480);
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
