#ifndef AUDIOCHUNKS_H
#define AUDIOCHUNKS_H

#include <stdint.h>
#include <complex.h>
#include <fftw3.h>

namespace HoS {

  class wave_t {
  public:
    wave_t(uint32_t n);
    wave_t(const wave_t& src);
    ~wave_t();
    void operator/=(const float& d);
    float maxabs() const;
    void copy(const wave_t& src);
    uint32_t n_;
    float* b;
  };

  class spec_t {
  public:
    spec_t(uint32_t n);
    spec_t(const spec_t& src);
    ~spec_t();
    void copy(const spec_t& src);
    void operator/=(const spec_t& o);
    uint32_t n_;
    float _Complex * b;
  };

  class fft_t {
  public:
    fft_t(uint32_t fftlen);
    void execute(const wave_t& src);
    void execute(const spec_t& src);
    ~fft_t();
    wave_t w;
    spec_t s;
  private:
    fftwf_plan fftwp;
    fftwf_plan fftwp_s2w;
  };

  class stft_t : public fft_t {
  public:
    enum windowtype_t {
      WND_RECT, 
      WND_HANNING
    };
    stft_t(uint32_t fftlen, uint32_t wndlen, uint32_t chunksize, windowtype_t wnd);
    void process(const wave_t& w);
  private:
    uint32_t fftlen_;
    uint32_t wndlen_;
    uint32_t chunksize_;
    uint32_t wshift;
    wave_t long_in;
    wave_t long_windowed_in;
    wave_t window;
  };

  class delay1_t : public wave_t {
  public:
    delay1_t(uint32_t n);
    void process(const wave_t& w);
  private:
    float state;
  };

}

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */

