#ifndef LIBHOSAUDIOCHUNKS_H
#define LIBHOSAUDIOCHUNKS_H

#include <fftw3.h>
#include <sndfile.h>
#include <stdint.h>
#include <string>

#include <tascar/audiochunks.h>

//#include <complex>

namespace HoS {

  class sndfile_handle_t {
  public:
    sndfile_handle_t(const std::string& fname);
    ~sndfile_handle_t();
    uint32_t get_frames() const { return sf_inf.frames; };
    uint32_t get_channels() const { return sf_inf.channels; };
    uint32_t readf_float(float* buf, uint32_t frames);

  private:
    SNDFILE* sfile;
    SF_INFO sf_inf;
  };

  class sndfile_t : public sndfile_handle_t, public TASCAR::wave_t {
  public:
    sndfile_t(const std::string& fname, uint32_t channel = 0);
    void add_chunk(int32_t chunk_time, int32_t start_time, float gain,
                   wave_t& chunk);
  };

  /**
     \brief Unit delay

     Delay a signal by one sample. The input is taken in the process() method,
     the result is stored in the class itself.
   */
  class delay1_t : public TASCAR::wave_t {
  public:
    delay1_t(uint32_t n);
    void process(const TASCAR::wave_t& w);

  private:
    float state;
  };

} // namespace HoS

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
