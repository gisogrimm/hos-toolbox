#include "libhos_audiochunks.h"
#include "hos_defs.h"
#include <algorithm>
#include <iostream>
#include <math.h>
#include <string.h>
#include <tascar/errorhandling.h>

using namespace HoS;

/**
   \brief Constructor

   \param n Fragment size
 */
delay1_t::delay1_t(uint32_t n) : TASCAR::wave_t(n), state(0.0) {}

void delay1_t::process(const TASCAR::wave_t& w)
{
  for(unsigned int k = 1; k < n; k++)
    d[k] = w.d[k - 1];
  d[0] = state;
  state = w.d[n - 1];
}

sndfile_handle_t::sndfile_handle_t(const std::string& fname)
    : sfile(sf_open(fname.c_str(), SFM_READ, &sf_inf))
{
  if(!sfile)
    throw TASCAR::ErrMsg("Unable to open sound file \"" + fname +
                         "\" for reading.");
}

sndfile_handle_t::~sndfile_handle_t()
{
  sf_close(sfile);
}

uint32_t sndfile_handle_t::readf_float(float* buf, uint32_t frames)
{
  return sf_readf_float(sfile, buf, frames);
}

sndfile_t::sndfile_t(const std::string& fname, uint32_t channel)
    : sndfile_handle_t(fname), TASCAR::wave_t(get_frames())
{
  uint32_t ch(get_channels());
  uint32_t N(get_frames());
  wave_t chbuf(N * ch);
  readf_float(chbuf.d, N);
  for(uint32_t k = 0; k < N; k++)
    d[k] = chbuf[k * ch + channel];
}

void sndfile_t::add_chunk(int32_t chunk_time, int32_t start_time, float gain,
                          wave_t& chunk)
{
  for(int32_t k = std::max(start_time, chunk_time);
      k < std::min(start_time + (int32_t)(size()),
                   chunk_time + (int32_t)(chunk.size()));
      k++)
    chunk[k - chunk_time] += gain * d[k - start_time];
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
