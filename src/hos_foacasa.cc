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
#include <tascar/jackclient.h>
#include <tascar/osc_helper.h>
#include <tascar/errorhandling.h>
#include <tascar/ola.h>
#include <stdlib.h>
#include <iostream>
#include "hos_defs.h"
#include <getopt.h>
#include "lininterp.h"
#include "filter.h"

/**
   \brief One iteration of gradient search
   \param eps Stepsize scaling factor
   \retval param Input: start values, Output: new values
   \param err Error function handle
   \param data Data pointer to be handed to error function
   \param unitstep Unit step size
   \return Error measure
 */
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

/**
   \brief Create logarithmic frequency spacing
 */
class freqinfo_t {
public:
  freqinfo_t( float bpo, float fmin, float fmax );
  uint32_t bands;
  std::vector<float> fc; ///< center frequencies
  std::vector<float> fe; ///< edge frequencies
  float band(float f_hz);
private:
  float bpo_;
  float fmin_;
  float fmax_;
};

/**
   \brief Two-dimensional data container for feature map
 */
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

/**
   \brief Model which describes a scene.

   A scene model consists of several objects. Each object is described
   based on a brief parameter set. This specific model is a mixture
   model, using gaussians on the frequency axis, and raised cosines
   along the azimuth axis.
 */
class scene_model_t : public xyfield_t {
public:
  /**
     \brief Parameter set for one object
   */
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
  scene_model_t(uint32_t sx,uint32_t sy,uint32_t numobj,float bpo,float fmin,const std::vector<std::string>& names,uint32_t sortmode_);
  float objval(float x,float y,param_t lp);
  float objval(float x,float y,const std::vector<float>&);
  float objval(float x,float y);
  static float errfun(const std::vector<float>&,void* data);
  float errfun(const std::vector<float>&);
  void iterate();
  void reset();
  uint32_t size() const {return nobj;};
  const std::vector<float>& param() const { return obj_param;};
  param_t param(uint32_t k) const { return param_t(k,obj_param);};
  float geterror(){ return error;};
  float bayes_prob(float x,float y,uint32_t ko);
  void send_osc(const lo_address& lo_addr);
  void add_variables( TASCAR::osc_server_t* srv );
private:
  float error;
  uint32_t nobj;
  std::vector<float> obj_param;
  std::vector<float> unitstep;
  float xscale;
  uint32_t calls;
  std::vector<std::string> objnames;
  std::vector<std::string> paths_pitch;
  std::vector<std::string> paths_bw;
  std::vector<std::string> paths_az;
  float bpo_;
  float fmin_;
  std::vector<param_t> vpar;
  uint32_t sortmode;
};

/**
   \brief Add variables to OSC server to allow resetting of parameters
 */
void scene_model_t::add_variables( TASCAR::osc_server_t* srv )
{
  for(uint32_t ko=0;ko<nobj;ko++){
    char ctmp[1024];
    sprintf(ctmp,"/%s/",objnames[ko].c_str());
    srv->set_prefix(ctmp);
    srv->add_float("mux",&(obj_param[5*ko]));
    srv->add_float("muy",&(obj_param[5*ko+1]));
    srv->add_float("sigmax",&(obj_param[5*ko+2]));
    srv->add_float("sigmay",&(obj_param[5*ko+3]));
  }
}

/**
   \brief Comparison function to sort objects along y axis
 */
bool param_less_y(scene_model_t::param_t i,scene_model_t::param_t j){
  return (i.cy>j.cy+1); 
}

/**
   \brief Comparison function to sort objects along x axis
 */
bool param_less_x(scene_model_t::param_t i,scene_model_t::param_t j){
  return (i.cx>j.cx); 
}

/**
   \brief Send OSC messages describing the scene
 */
void scene_model_t::send_osc(const lo_address& lo_addr)
{
  // 0 equals c in low pitch (a=415 Hz):
  float delta(bpo_*log2f(0.5*fmin_/246.8f));
  for(uint32_t ko=0;ko<nobj;ko++){
    param_t par(param(ko));
    lo_send(lo_addr,paths_pitch[ko].c_str(),"f",12.0f/bpo_*(par.cy+delta));
    lo_send(lo_addr,paths_bw[ko].c_str(),"f",12.0f/bpo_*par.wy);
    lo_send(lo_addr,paths_az[ko].c_str(),"f",RAD2DEG*(PI2*par.cx/sizex()-M_PI));
  }
}

scene_model_t::param_t::param_t()
  : cx(0),cy(0),wx(3),wy(2),g(1)
{
}

scene_model_t::param_t::param_t(uint32_t num,const std::vector<float>& vp)
{
  cx = vp[5*num];
  cy = vp[5*num+1];
  wx = vp[5*num+2];
  wy = vp[5*num+3];
  g = vp[5*num+4];
}

void scene_model_t::param_t::setp(uint32_t num,std::vector<float>& vp)
{
  vp[5*num] = cx;
  vp[5*num+1] = cy;
  vp[5*num+2] = wx;
  vp[5*num+3] = wy;
  vp[5*num+4] = g;
}

scene_model_t::scene_model_t(uint32_t sx,uint32_t sy,uint32_t numobj,float bpo,float fmin,const std::vector<std::string>& names,uint32_t sortmode_)
  : xyfield_t(sx,sy),error(0),nobj(numobj),
    xscale(PI2/(float)sx),
    objnames(names),
    bpo_(bpo),fmin_(fmin),
    sortmode(sortmode_)
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
    par.cx = sx*(double)k/(double)nobj;
    // center y:
    par.cy = sy*0.5;
    par.setp(k,obj_param);
    // center x:
    unitstep[5*k] = 0.01;
    // center y:
    unitstep[5*k+1] = 0.1;
    unitstep[5*k+2] = 0.01;
    unitstep[5*k+3] = 0.01;
    unitstep[5*k+4] = 10;
  }
  vpar.resize(nobj);
}

/**
   \brief Model function for one object
 */
float scene_model_t::objval(float x,float y,param_t lp) 
{
  calls++;
  //float g(val(std::max(0.0f,std::min(lp.cx,(float)(sizex()))),
  //            std::max(0.0f,std::min(lp.cy,(float)(sizey())))));
  lp.cy = y-lp.cy;
  lp.cy /= lp.wy*1.4142135623730f;
  lp.cy *= lp.cy;
  return lp.g*powf(0.5+0.5*cosf((x-lp.cx)*xscale),lp.wx)*expf(-lp.cy);
}

/**
 */
float scene_model_t::objval(float x,float y,const std::vector<float>& p)
{
  float rv(0.0f);
  for(uint32_t k=0;k<nobj;k++)
    rv += objval(x,y,param_t(k,p));
  return rv;
}

float scene_model_t::objval(float x,float y)
{
  return objval(x,y,obj_param);
}

float scene_model_t::bayes_prob(float x,float y,uint32_t ko)
{
  return objval(x,y,param_t(ko,obj_param))/objval(x,y);
}

float scene_model_t::errfun(const std::vector<float>& p,void* data)
{
  return ((scene_model_t*)(data))->errfun(p);
}

float scene_model_t::errfun(const std::vector<float>& p)
{
  float err(0.0);
  for(uint32_t kx=0;kx<sizex();kx++)
    for(uint32_t ky=0;ky<sizey();ky++){
      float lerr(val(kx,ky)-objval(kx,ky,p));
      err += lerr*lerr;
    }
  return err;
}

void scene_model_t::iterate()
{
  calls = 0;
  error = downhill_iterate(0.0002,obj_param,&scene_model_t::errfun,this,unitstep);
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
    if( par.wx < 1 )
      par.wx = 1;
    if( par.wx > 10 )
      par.wx = 10;
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
  switch( sortmode ){
  case 1: // frequency sorting:
    std::sort(vpar.begin(),vpar.end(),param_less_y);
    break;
  case 2: // azimuth sorting:
    std::sort(vpar.begin(),vpar.end(),param_less_x);
    break;
  }
  for(uint32_t k=0;k<nobj;k++)
    vpar[k].setp(k,obj_param);
}

void scene_model_t::reset()
{
  for(uint32_t k=0;k<nobj;k++){
    param_t par(k,obj_param);
    par.cx = k/(double)nobj*sizex();
    par.cy = 0.5*sizey();    
    par.wx = 3;
    par.wy = 4;
    par.g = 20;
    vpar[k] = par;
    vpar[k].setp(k,obj_param);
  }
}

/**
   \brief Feature extractor for one frequency band
   
 */
class az_hist_t : public TASCAR::wave_t
{
public:
  az_hist_t(uint32_t size);
  void update();
  bool add(float f, float az,float weight);
  void set_tau(float tau,float fs);
  void set_frange(float f1,float f2);
private:
  float c1;
  float c2;
  float fmin;
  float fmax;
};

az_hist_t::az_hist_t(uint32_t size)
  : TASCAR::wave_t(size)
{
  set_tau(1.0f,1.0f);
  set_frange(0,22050);
}

/**
   \brief Smoothly forget previous values
 */
void az_hist_t::update()
{
  *this *= c1;
}

/**
   \brief Add intensity at the specified frequency and azimuth
 */
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

freqinfo_t::freqinfo_t(float bpo,float fmin,float fmax)
  : bands(bpo*log2(fmax/fmin)+1.0),bpo_(bpo),fmin_(fmin),fmax_(fmax)
{
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

  class foacoh_t : public freqinfo_t, public Gtk::DrawingArea, public jackc_db_t,
                   public TASCAR::osc_server_t
  {
  public:
    foacoh_t(const std::string& name,uint32_t channels,float bpo,float fmin,float fmax,const std::vector<std::string>& objnames,uint32_t periodsize,const std::string& url,uint32_t sortmode, float levelthreshold_, float lpperiods, float taumax );
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
    TASCAR::ola_t ola_w;
    TASCAR::ola_t ola_x;
    TASCAR::ola_t ola_y;
    std::vector<TASCAR::ola_t*> ola_obj;
    TASCAR::wave_t lp_c1; ///< low pass filter coefficients
    TASCAR::wave_t lp_c2; ///< low pass filter coefficients
    TASCAR::spec_t ccohXY;///< complex temporary coherence
    // coherence functions:
    TASCAR::wave_t cohXY;
    TASCAR::wave_t az;
    std::vector<az_hist_t> haz;
    std::string name_;
    float fscale;
    Glib::RefPtr<Gdk::Pixbuf> image;
    //Glib::RefPtr<Gdk::Pixbuf> image_mod;
    //bool draw_image;
    colormap_t col;
    uint32_t azchannels;
    scene_model_t obj;
    //std::vector<mod_analyzer_t> modflt;
    float vmin;
    float vmax;
    float objlp_c1;
    float objlp_c2;
    //std::vector<float> bayes_prob;
    std::vector<float> f2band;
    lo_address lo_addr;
    std::vector<std::string> names;
    //std::vector<std::string> paths_modf;
    //std::vector<std::string> paths_modbw;
    uint32_t send_cnt;
    HoS::arflt levellp;
    float level;
    float levelthreshold;
  };

}

using namespace HoSGUI;

std::complex<float> If = 1i;

int foacoh_t::inner_process(jack_nframes_t n, const std::vector<float*>& vIn, const std::vector<float*>& vOut)
{
  TASCAR::wave_t inW(n,vIn[0]);
  TASCAR::wave_t inX(n,vIn[1]);
  TASCAR::wave_t inY(n,vIn[2]);
  level = inW.spldb();
  if( !(level > -200) )
    level = -200;
  level = levellp.filter(level);
  ola_w.process(inW);
  ola_x.process(inX);
  ola_y.process(inY);
  // forget previous values:
  for(uint32_t kH=0;kH<haz.size();kH++)
    haz[kH].update();
  // do scene analysis:
  for(uint32_t k=0;k<ola_x.s.size();k++){
    float freq(k*fscale);
    // for all measures, X*conj(Y) and its absolute value is needed:
    std::complex<float> cW(ola_w.s[k]);
    std::complex<float> cX(ola_x.s[k]);
    std::complex<float> cY(ola_y.s[k]);
    std::complex<float> cXY(cX * std::conj(cY));
    float cXYabs(std::abs(cXY));
    // Measure 1: x-y-coherence:
    ccohXY[k] *= lp_c1[k];
    if( cXYabs > 0 ){
      ccohXY[k] += (lp_c2[k]/cXYabs)*cXY;
    }
    cohXY[k] = std::abs(ccohXY[k]);
    if( std::abs(cW) > 0 ){
      cX /= cW;
      cY /= cW;
    }
    az[k] = std::arg(cX+If*cY);
    float w(powf(std::abs(cW)*cohXY[k],2.0));
    float l_az(az[k]+M_PI);
    l_az *= (0.5*haz.size()/M_PI);
    for(uint32_t kH=0;kH<haz.size();kH++)
      haz[kH].add(freq,az[k],w);
  }
  // send model parameters:
  if( send_cnt == 0 ){
    obj.send_osc(lo_addr);
    send_cnt = 2;
  }else
    send_cnt--;
  // do object decomposition:
  for(uint32_t kobj=0;kobj<obj.size();kobj++){
    TASCAR::wave_t outW(n,vOut[kobj]);
    scene_model_t::param_t par(obj.param(kobj));
    float az(PI2*par.cx/(float)azchannels-M_PI);
    float wx(cos(az));
    float wy(sin(az));
    for(uint32_t k=0;k<ola_w.s.size();k++){
      // by not using W channel gain, this is max-rE FOA decoder:
      ola_obj[kobj]->s[k] = (ola_w.s[k]+wx*ola_x.s[k]+wy*ola_y.s[k])*obj.bayes_prob(par.cx,f2band[k],kobj);
    }
    ola_obj[kobj]->s[0] = std::real(ola_obj[kobj]->s[0]);
    ola_obj[kobj]->ifft(outW);
  }
  for(uint32_t kb=0;kb<bands;kb++){
    for(uint32_t kc=0;kc<azchannels;kc++){
      obj.val(kc,kb) = 10.0f*log10f(std::max(1.0e-10f,haz[kb][kc]));
    }
  }
  vmin = objlp_c1*vmin+objlp_c2*obj.min();
  vmax = std::max(vmin+0.1f,objlp_c1*vmax+objlp_c2*obj.max());
  obj += -vmin;
  obj *= 100.0/(vmax-vmin);
  obj.iterate();
  if( level < levelthreshold )
    obj.reset();
  return 0;
}

foacoh_t::foacoh_t(const std::string& name, uint32_t channels, float bpo,
                   float fmin, float fmax,
                   const std::vector<std::string>& objnames,
                   uint32_t periodsize_, const std::string& url,
                   uint32_t sortmode, float levelthreshold_, float lpperiods,
                   float taumax)
    : freqinfo_t(bpo, fmin, fmax),
      // osc_server_t(OSC_ADDR,OSC_PORT),
      jackc_db_t("foacoh", periodsize_), osc_server_t("", "9788", "UDP"),
      periodsize(periodsize_), fftlen(std::max(512u, 4 * periodsize)),
      wndlen(std::max(256u, 2 * periodsize)),
      ola_w(fftlen, wndlen, periodsize, TASCAR::stft_t::WND_HANNING,
            TASCAR::stft_t::WND_HANNING, 0.5),
      ola_x(fftlen, wndlen, periodsize, TASCAR::stft_t::WND_HANNING,
            TASCAR::stft_t::WND_HANNING, 0.5),
      ola_y(fftlen, wndlen, periodsize, TASCAR::stft_t::WND_HANNING,
            TASCAR::stft_t::WND_HANNING, 0.5),
      lp_c1(ola_x.s.size()), lp_c2(ola_x.s.size()), ccohXY(ola_x.s.size()),
      cohXY(ola_x.s.size()), az(ola_x.s.size()), name_(name),
      fscale(get_srate() / (float)fftlen),
      // draw_image(true),
      col(0), azchannels(channels),
      obj(channels, bands, objnames.size(), bpo, fmin, objnames, sortmode),
      vmin(0), vmax(1), lo_addr(lo_address_new_from_url(url.c_str())),
      names(objnames), send_cnt(2),
      levellp(0.125, 0.125, get_srate() / (float)periodsize), level(-200),
      levelthreshold(levelthreshold_)
{
  lo_address_set_ttl(lo_addr, 1);
  image = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, channels, bands);
  add_input_port("in.0w");
  add_input_port("in.1x");
  add_input_port("in.1y");
  for(uint32_t k = 0; k < ola_w.s.size(); k++)
    f2band.push_back(band((float)k * fscale));
  float frame_rate(get_srate() / (float)periodsize);
  for(uint32_t ko = 0; ko < objnames.size(); ko++) {
    add_output_port(names[ko].c_str());
    ola_obj.push_back(new TASCAR::ola_t(fftlen, wndlen, periodsize,
                                        TASCAR::stft_t::WND_HANNING,
                                        TASCAR::stft_t::WND_HANNING, 0.5));
    // modflt.push_back(mod_analyzer_t(frame_rate,4,1));
    // paths_modf.push_back(std::string("/")+names[ko]+std::string("/modf"));
    // paths_modbw.push_back(std::string("/")+names[ko]+std::string("/modbw"));
  }
  // coherence/azimuth estimation smoothing: 40 ms
  for(uint32_t k = 0; k < lp_c1.size(); k++) {
    float f(fscale * std::max(k, 1u));
    float tau(std::min(0.5f, std::max(0.05f, 150.0f / f)));
    tau = 0.04;
    lp_c1[k] = exp(-1.0 / (tau * frame_rate));
    lp_c2[k] = 1.0f - lp_c1[k];
  }
  for(uint32_t k = 0; k < az.size(); k++)
    az[k] = 0.0;
  // intensity accumulators:
  // tau = 250/fc, max 1s, min 125ms
  for(uint32_t kH = 0; kH < bands; kH++) {
    haz.push_back(az_hist_t(channels));
    haz.back().set_tau(std::max(0.125f, std::min(taumax, lpperiods / fc[kH])),
                       frame_rate);
    haz.back().set_frange(fe[kH], fe[kH + 1]);
  }
  objlp_c1 = exp(-1.0 / (0.5 * frame_rate));
  objlp_c2 = 1.0f - objlp_c1;
  col.clim(-1, 100);
  // set_prefix("/"+name);
  Glib::signal_timeout().connect(sigc::mem_fun(*this, &foacoh_t::on_timeout),
                                 40);
#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  // Connect the signal handler if it isn't already a virtual method override:
  signal_draw().connect(sigc::mem_fun(*this, &mixergui_t::on_draw), false);
#endif // GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  obj.add_variables(this);
}

void foacoh_t::activate()
{
  jackc_db_t::activate();
  osc_server_t::activate();
  try{
    connect_in(0,"mhwe:out1");
    connect_in(1,"mhwe:out2");
    connect_in(2,"mhwe:out3");
  }
  catch( const std::exception& e ){
    std::cerr << "Warning: " << e.what() << std::endl;
  };
  try{
    connect_in(0,"tetraproc:B-form.W");
    connect_in(1,"tetraproc:B-form.X");
    connect_in(2,"tetraproc:B-form.Y");
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
  for(uint32_t k=0;k<ola_obj.size();k++)
    delete ola_obj[k];
}

void draw_ellipse( const Cairo::RefPtr<Cairo::Context>& cr, float x, float y, float sx, float sy )
{
  if( (sx > 0) && (sy > 0) ){
    cr->save();
    cr->translate( x, y );
    cr->scale( sx, sy );
    cr->arc( 0, 0, 1, 0, 2*M_PI );
    cr->restore();
  }else{
    DEBUG(sx);
    DEBUG(sy);
  }
}

  
bool foacoh_t::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  Gtk::Allocation allocation = get_allocation();
  const int width = allocation.get_width();
  const int height = allocation.get_height();
  float vmax(-1e8);
  float vmin(1e8);
  // fill image values:
  guint8* pixels = image->get_pixels();
  for(uint32_t k=0;k<azchannels;k++)
    for(uint32_t b=0;b<bands;b++){
      uint32_t pix(k+(bands-b-1)*azchannels);
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
  cr->save();
  cr->scale((double)width/(double)azchannels,(double)height/(double)bands);
  cr->set_line_width(0.2);
  Gdk::Cairo::set_source_pixbuf(cr, image, 0, 0);
  cr->paint();
  for(uint32_t ko=0;ko<obj.size();ko++){
    scene_model_t::param_t par(obj.param(ko));
    for(int32_t dx=-1;dx<2;++dx){
      float x(par.cx+(double)azchannels*(double)dx);
      cr->set_source_rgb( 1, 1, 1 );
      cr->save();
      cr->set_source_rgba( 0.7, 0.7, 0.7, 0.5 );
      draw_ellipse( cr, x, bands-par.cy-1, 
                    0.125*(double)azchannels/log2(2*par.wx), 0.5*par.wy );
      draw_ellipse( cr, x, bands-par.cy-1, 
                    0.1, 0.1 );
      cr->fill();
      cr->restore();
      cr->move_to(x,bands-par.cy-1 );
      cr->save();
      cr->scale((double)azchannels/(double)width,(double)bands/(double)height);
      cr->set_font_size(48);
      char ctmp[16];
      sprintf(ctmp,"%d",ko+1);
      cr->show_text(ctmp);
      cr->stroke();
      cr->restore();
    }
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
  float levelthreshold(-200);
  float lpperiods( 500 );
  float taumax( 1.0 );
  uint32_t periodsize(1024);
  uint32_t sortmode(0);
  std::vector<std::string> objnames;
  const char *options = "hj:c:b:l:u:p:d:s:t:f:x:";
  struct option long_options[] = { 
    { "help",      0, 0, 'h' },
    { "jackname",  1, 0, 'j' },
    { "desturl",   1, 0, 'd' },
    { "channels",  1, 0, 'c' },
    { "bpoctave",  1, 0, 'b' },
    { "fmin",      1, 0, 'l' },
    { "fmax",      1, 0, 'u' },
    { "periodsize",1, 0, 'p' },
    { "sort",      1, 0, 's' },
    { "threshold", 1, 0, 't' },
    { "lpperiods", 1, 0, 'f' },
    { "taumax", 1, 0, 'x' },
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
    case 'f':
      lpperiods = atof(optarg);
      break;
    case 'x':
      taumax = atof(optarg);
      break;
    case 'l':
      fmin = atof(optarg);
      break;
    case 'u':
      fmax = atof(optarg);
      break;
    case 't':
      levelthreshold = atof(optarg);
      break;
    case 'p':
      periodsize = atoi(optarg);
      break;
    case 's':
      sortmode = atoi(optarg);
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
  HoSGUI::foacoh_t c(jackname,channels,bpoctave,fmin,fmax,objnames,periodsize,desturl,sortmode,levelthreshold, lpperiods, taumax );
  win.add(c);
  win.set_default_size(640,480);
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
