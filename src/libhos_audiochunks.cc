#include "libhos_audiochunks.h"
#include <string.h>
#include <math.h>
#include <complex.h>
#include <algorithm>
#include "errorhandling.h"
#include "hos_defs.h"
#include <iostream>

using namespace HoS;

//wave_t::wave_t(uint32_t n)
//  : n_(n),b(new float[n_]), own_pointer(true)
//{
//  memset(b,0,n_*sizeof(float));
//}
//
//wave_t::wave_t(uint32_t n,float* ptr)
//  : n_(n),b(ptr), own_pointer(false)
//{
//}
//
//wave_t::wave_t(const wave_t& src)
//  : n_(src.n_),b(new float[n_]), own_pointer(true)
//{
//  copy(src);
//}
//
//wave_t::~wave_t()
//{
//  if( own_pointer )
//    delete [] b;
//}
//
//void wave_t::operator/=(const float& d)
//{
//  if(fabsf(d)>0)
//    for(unsigned int k=0;k<n_;k++)
//      b[k]/=d;
//}
//
//void wave_t::operator*=(const float& d)
//{
//  for(unsigned int k=0;k<n_;k++)
//    b[k]*=d;
//}
//
//void wave_t::operator+=(const wave_t& o)
//{
//  for(unsigned int k=0;k<std::min(o.n_,n_);k++){
//    b[k] += o.b[k];
//  }
//}
//
//void wave_t::operator*=(const wave_t& o)
//{
//  for(unsigned int k=0;k<std::min(o.n_,n_);k++){
//    b[k] *= o.b[k];
//  }
//}
//
//float wave_t::maxabs() const
//{
//  float r(0);
//  for(unsigned int k=0;k<n_;k++)
//    r = std::max(r,fabsf(b[k]));
//  return r;
//}
//
//void wave_t::copy(const wave_t& src)
//{
//  //memcpy(b,src.b,std::min(n_,src.n_)*sizeof(float));
//  memmove(b,src.b,std::min(n_,src.n_)*sizeof(float));
//}

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
  //memcpy(b,src.b,std::min(n_,src.n_)*sizeof(float _Complex));
  memmove(b,src.b,std::min(n_,src.n_)*sizeof(float _Complex));
}

void spec_t::operator/=(const spec_t& o)
{
  for(unsigned int k=0;k<std::min(o.n_,n_);k++){
    if( cabs(o.b[k]) > 0 )
      b[k] /= o.b[k];
  }
}

void spec_t::operator*=(const spec_t& o)
{
  for(unsigned int k=0;k<std::min(o.n_,n_);k++)
    b[k] *= o.b[k];
}

void spec_t::operator*=(const float& o)
{
  for(unsigned int k=0;k<n_;k++)
    b[k] *= o;
}

void spec_t::conj()
{
  for(uint32_t k=0;k<n_;k++)
    b[k] = conjf( b[k] );
}

void fft_t::fft()
{
  fftwf_execute(fftwp);
}

void fft_t::ifft()
{
  fftwf_execute(fftwp_s2w);
  w *= 1.0/w.size();
}

void fft_t::execute(const TASCAR::wave_t& src)
{
  w.copy(src);
  fft();
}

void fft_t::execute(const spec_t& src)
{
  s.copy(src);
  ifft();
}

fft_t::fft_t(uint32_t fftlen)
  : w(fftlen),
    s(fftlen/2+1),
    fftwp(fftwf_plan_dft_r2c_1d(fftlen,w.d,s.b,0)),
    fftwp_s2w(fftwf_plan_dft_c2r_1d(fftlen,s.b,w.d,0))
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
    zpad1((fftlen_-wndlen_)/2u),
    zpad2(fftlen_-wndlen_-zpad1),
    long_in(wndlen),
    long_windowed_in(fftlen),
    window(wndlen)
{
  unsigned int k;
  switch( wnd ){
  case WND_RECT :
    for(k=0;k<wndlen;k++)
      window.d[k] = 1.0;
    break;
  case WND_HANNING :
    for(k=0;k<wndlen;k++)
      window.d[k] = 0.5-0.5*cos(2.0*k*M_PI/wndlen);
    break;
  case WND_SQRTHANN :
    for(k=0;k<wndlen;k++)
      window.d[k] = sqrt(0.5-0.5*cos(2.0*k*M_PI/wndlen));
    break;
  }
}
    
void stft_t::process(const TASCAR::wave_t& w)
{
  TASCAR::wave_t zero1(zpad1,long_windowed_in.d);
  TASCAR::wave_t zero2(zpad2,&(long_windowed_in.d[zpad1+wndlen_]));
  TASCAR::wave_t newchunk(wndlen_,&(long_windowed_in.d[zpad1]));
  for(unsigned int k=chunksize_;k<wndlen_;k++)
    long_in.d[k-chunksize_] = long_in.d[k];
  for(unsigned int k=0;k<chunksize_;k++)
    long_in.d[k+wndlen_-chunksize_] = w.d[k];
  for(unsigned int k=0;k<wndlen_;k++)
    newchunk[k] = window.d[k]*long_in.d[k];
  for(unsigned int k=0;k<zpad1;k++)
    zero1[k] = 0.0f;
  for(unsigned int k=0;k<zpad2;k++)
    zero2[k] = 0.0f;
  execute(long_windowed_in);
}

ola_t::ola_t(uint32_t fftlen, uint32_t wndlen, uint32_t chunksize, windowtype_t wnd, windowtype_t zerownd,windowtype_t postwnd)
  : stft_t(fftlen,wndlen,chunksize,wnd),
    zwnd1(zpad1),
    zwnd2(zpad2),
    pwnd(fftlen),
    apply_pwnd(true),
    long_out(fftlen)
{
  switch( zerownd ){
  case WND_RECT :
    for(uint32_t k=0;k<zpad1;k++)
      zwnd1[k] = 1.0;
    for(uint32_t k=0;k<zpad2;k++)
      zwnd2[k] = 1.0;
    break;
  case WND_HANNING :
    for(uint32_t k=0;k<zpad1;k++)
      zwnd1[k] = 0.5-0.5*cos(k*M_PI/zpad1);
    for(uint32_t k=0;k<zpad2;k++)
      zwnd2[k] = 0.5+0.5*cos(k*M_PI/zpad2);
    break;
  case WND_SQRTHANN :
    for(uint32_t k=0;k<zpad1;k++)
      zwnd1[k] = sqrt(0.5-0.5*cos(k*M_PI/zpad1));
    for(uint32_t k=0;k<zpad2;k++)
      zwnd2[k] = sqrt(0.5+0.5*cos(k*M_PI/zpad2));
    break;
  }
  switch( postwnd ){
  case WND_RECT :
    for(uint32_t k=0;k<pwnd.size();k++)
      pwnd[k] = 1.0;
    apply_pwnd = false;
    break;
  case WND_HANNING :
    for(uint32_t k=0;k<pwnd.size();k++)
      pwnd[k] = 0.5-0.5*cos(PI2*(double)k/(double)(pwnd.size()));
    break;
  case WND_SQRTHANN :
    for(uint32_t k=0;k<pwnd.size();k++)
      pwnd[k] = sqrt(0.5-0.5*cos(PI2*(double)k/(double)(pwnd.size())));
    break;
  }
  //DEBUG(fftlen);
  //DEBUG(wndlen);
  //DEBUG(chunksize);
  //DEBUG(wnd);
  //DEBUG(zerownd);
  //DEBUG(postwnd);
  //DEBUG(zpad1);
  //DEBUG(zpad2);
}

void ola_t::ifft(TASCAR::wave_t& wOut)
{
  fft_t::ifft();
  TASCAR::wave_t zero1(zpad1,w.d);
  TASCAR::wave_t zero2(zpad2,&(w.d[fftlen_-zpad2]));
  zero1 *= zwnd1;
  zero2 *= zwnd2;
  if( apply_pwnd )
    w *= pwnd;
  long_out += w;
  TASCAR::wave_t w1(fftlen_-chunksize_,long_out.d);
  TASCAR::wave_t w2(fftlen_-chunksize_,&(long_out.d[chunksize_]));
  TASCAR::wave_t w3(chunksize_,long_out.d);
  // store output:
  wOut.copy(w3);
  // shift input:
  //for(uint32_t k=0;k<fftlen_-chunksize_;k++)
  w1.copy(w2);
  //long_out[k] = long_out[k+chunksize_];
  TASCAR::wave_t w4(chunksize_,&(long_out.d[fftlen_-chunksize_]));
  for(uint32_t k=0;k<w4.size();k++)
    w4[k] = 0;
}

/**
   \brief Constructor
   
   \param n Fragment size
 */
delay1_t::delay1_t(uint32_t n)
  : wave_t(n),
    state(0.0)
{
}
 
void delay1_t::process(const TASCAR::wave_t& w)
{
  for(unsigned int k=1;k<n;k++)
    d[k] = w.d[k-1];
  d[0] = state;
  state = w.d[n-1];
}

sndfile_handle_t::sndfile_handle_t(const std::string& fname)
  : sfile(sf_open(fname.c_str(),SFM_READ,&sf_inf))
{
  if( !sfile )
    throw TASCAR::ErrMsg("Unable to open sound file \""+fname+"\" for reading.");
}
    
sndfile_handle_t::~sndfile_handle_t()
{
  sf_close(sfile);
}

uint32_t sndfile_handle_t::readf_float( float* buf, uint32_t frames )
{
  return sf_readf_float( sfile, buf, frames );
}

sndfile_t::sndfile_t(const std::string& fname,uint32_t channel)
  : sndfile_handle_t(fname),
    wave_t(get_frames())
{
  uint32_t ch(get_channels());
  uint32_t N(get_frames());
  wave_t chbuf(N*ch);
  readf_float(chbuf.d,N);
  for(uint32_t k=0;k<N;k++)
    d[k] = chbuf[k*ch+channel];
}

void sndfile_t::add_chunk(int32_t chunk_time, int32_t start_time,float gain,wave_t& chunk)
{
  for(int32_t k=std::max(start_time,chunk_time);k < std::min(start_time+(int32_t)(size()),chunk_time+(int32_t)(chunk.size()));k++)
    chunk[k-chunk_time] += gain*d[k-start_time];
}



/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */

