#include "audiochunks.h"
#include <string.h>
#include <math.h>
#include <complex.h>
#include <algorithm>

using namespace HoS;

wave_t::wave_t(uint32_t n)
  : n_(n),b(new float[n_])
{
  memset(b,0,n_*sizeof(float));
}

wave_t::wave_t(const wave_t& src)
  : n_(src.n_),b(new float[n_])
{
  copy(src);
}

wave_t::~wave_t()
{
  delete [] b;
}

void wave_t::operator/=(const float& d)
{
  if(fabsf(d)>0)
    for(unsigned int k=0;k<n_;k++)
      b[k]/=d;
}

float wave_t::maxabs() const
{
  float r(0);
  for(unsigned int k=0;k<n_;k++)
    r = std::max(r,fabsf(b[k]));
  return r;
}

void wave_t::copy(const wave_t& src)
{
  memcpy(b,src.b,std::min(n_,src.n_)*sizeof(float));
}

spec_t::spec_t(uint32_t n) 
  : n_(n),b(new float _Complex [n_])
{
  memset(b,0,n_*sizeof(float _Complex));
}

spec_t::spec_t(const spec_t& src)
  : n_(src.n_),b(new float _Complex [n_])
{
  copy(src);
}

spec_t::~spec_t()
{
  delete [] b;
}

void spec_t::copy(const spec_t& src)
{
  memcpy(b,src.b,std::min(n_,src.n_)*sizeof(float _Complex));
}

void spec_t::operator/=(const spec_t& o)
{
  for(unsigned int k=0;k<std::min(o.n_,n_);k++){
    if( cabs(o.b[k]) > 0 )
      b[k] /= o.b[k];
  }
}

void fft_t::execute(const wave_t& src)
{
  w.copy(src);fftwf_execute(fftwp);
}

void fft_t::execute(const spec_t& src)
{
  s.copy(src);fftwf_execute(fftwp_s2w);
}

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


stft_t::stft_t(uint32_t fftlen, uint32_t wndlen, uint32_t chunksize, windowtype_t wnd)
  : fft_t(fftlen),
    fftlen_(fftlen),
    wndlen_(wndlen),
    chunksize_(chunksize),
    wshift(wndlen-chunksize),
    long_in(wndlen),
    long_windowed_in(fftlen),
    window(wndlen)
{
  unsigned int k;
  switch( wnd ){
  case WND_RECT :
    for(k=0;k<wndlen;k++)
      window.b[k] = 1.0;
  case WND_HANNING :
    for(k=0;k<wndlen;k++)
      window.b[k] = 0.5-0.5*cos(2.0*k*M_PI/wndlen);
  }
}
    
void stft_t::process(const wave_t& w)
{
  for(unsigned int k=wshift;k<wndlen_;k++)
    long_in.b[k-wshift] = long_in.b[k];
  for(unsigned int k=0;k<chunksize_;k++)
    long_in.b[k+wndlen_-chunksize_] = w.b[k];
  for(unsigned int k=0;k<wndlen_;k++)
    long_windowed_in.b[k] = window.b[k]*long_in.b[k];
  for(unsigned int k=wndlen_;k<fftlen_;k++)
    long_windowed_in.b[k] = 0.0;
  execute(long_windowed_in);
}

delay1_t::delay1_t(uint32_t n)
  : wave_t(n),
    state(0.0)
{
}
 
void delay1_t::process(const wave_t& w)
{
  for(unsigned int k=1;k<n_;k++)
    b[k] = w.b[k-1];
  b[0] = state;
  state = w.b[n_-1];
}


/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */

