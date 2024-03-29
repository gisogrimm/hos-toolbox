/**
   \file hos_if_filter.cc
   \ingroup apphos
   \brief Instantaneouos frequency based filter
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
   USA.

*/

#include "filter.h"
#include "hos_defs.h"
#include "libhos_audiochunks.h"
#include <getopt.h>
#include <iostream>
#include <signal.h>
#include <stdlib.h>
#include <tascar/jackclient.h>
#include <tascar/ola.h>
#include <tascar/osc_helper.h>
#include <unistd.h>

static bool b_quit(false);

class if_filter_t : public jackc_db_t, public TASCAR::osc_server_t {
public:
  if_filter_t(const std::string& server_addr, const std::string& server_port,
              const std::string& jackname, uint32_t fragsize);
  int inner_process(jack_nframes_t n, const std::vector<float*>& inBuf,
                    const std::vector<float*>& outBuf);
  void activate();
  void deactivate();

private:
  TASCAR::ola_t ola;
  TASCAR::stft_t dtfft;
  HoS::delay1_t d1;
  float ifscale;
  HoS::filter_array_t mean_lp;
  HoS::filter_array_t std_lp;
  HoS::filter_array_t pow_lp;
  float sigma0;
  float tau_std;
  float tau_gain;
  float extgain;
  bool b_invert;
  int32_t debugchannel;
};

if_filter_t::if_filter_t(const std::string& server_addr,
                         const std::string& server_port,
                         const std::string& jackname, uint32_t fragsize)
    : jackc_db_t(jackname, fragsize), TASCAR::osc_server_t(server_addr,
                                                           server_port, "UDP"),
      ola(4 * fragsize, 2 * fragsize, fragsize, TASCAR::stft_t::WND_HANNING,
          TASCAR::stft_t::WND_HANNING, 0.5),
      dtfft(4 * fragsize, 2 * fragsize, fragsize, TASCAR::stft_t::WND_HANNING,
            0.5),
      d1(fragsize), ifscale(-srate / (PI2)),
      mean_lp(ola.s.n_, srate / (double)fragsize),
      std_lp(ola.s.n_, srate / (double)fragsize),
      pow_lp(2, srate / (double)fragsize), sigma0(30.0), tau_std(0.05),
      tau_gain(0.4), extgain(1.0), b_invert(false), debugchannel(4)
{
  set_prefix("/" + jackname + "/");
  add_bool_true("quit", &b_quit);
  add_float("sigma", &sigma0);
  add_float("tau", &tau_std);
  add_float("taugain", &tau_gain);
  add_float_db("gain", &extgain);
  add_bool("invert", &b_invert);
  add_int("debug", &debugchannel);
  add_input_port("in");
  add_output_port("out");
}

void if_filter_t::activate()
{
  jackc_t::activate();
  osc_server_t::activate();
}

void if_filter_t::deactivate()
{
  osc_server_t::deactivate();
  jackc_t::deactivate();
}

float sqr(float x)
{
  return x * x;
}

int if_filter_t::inner_process(jack_nframes_t n,
                               const std::vector<float*>& inBuf,
                               const std::vector<float*>& outBuf)
{
  if((inBuf.size() == 0) || (outBuf.size() == 0))
    return 1;
  TASCAR::wave_t w_in(n, inBuf[0]);
  TASCAR::wave_t w_out(n, outBuf[0]);
  float sigma0_corr(0.69315f / sigma0);
  mean_lp.set_lowpass(tau_std);
  std_lp.set_lowpass(tau_std);
  pow_lp.set_lowpass(tau_gain);
  d1.process(w_in);
  ola.process(w_in);
  dtfft.process(d1);
  double pow_in(1e-20);
  double pow_out(1e-20);
  for(unsigned int k = 0; k < dtfft.s.n_; k++) {
    dtfft.s.b[k] *= conj(ola.s.b[k]);
    float ifreq(std::arg(dtfft.s.b[k]) * ifscale);
    float ifreq_mean(mean_lp.filter(k, ifreq));
    float ifreq_diff(ifreq_mean - ifreq);
    ifreq_diff *= ifreq_diff;
    float ifreq_std(sqrtf(std::max(0.0f, std_lp.filter(k, ifreq_diff))));
    float gain = 1.0f - expf(-ifreq_std * sigma0_corr);
    // if( k==debugchannel ){
    //  std::cout << ifreq << " " << ifreq_mean << " " << ifreq_std << " " <<
    //  gain << "\n";
    //}
    if(gain < 1e-4)
      gain = 1e-4;
    if(b_invert)
      gain = 1.0 - gain;
    pow_in += sqr(std::abs(ola.s[k]));
    ola.s[k] *= gain;
    pow_out += sqr(std::abs(ola.s[k]));
  }
  float bbgain(sqrtf(pow_lp.filter(0, pow_in) / pow_lp.filter(1, pow_out)));
  ola.s *= (bbgain * extgain);
  ola.ifft(w_out);
  return 0;
}

static void sighandler(int sig)
{
  b_quit = true;
}

void usage(struct option* opt)
{
  std::cout << "Usage:\n\nhos_if_filter [options]\n\nOptions:\n\n";
  while(opt->name) {
    std::cout << "  -" << (char)(opt->val) << " " << (opt->has_arg ? "#" : "")
              << "\n  --" << opt->name << (opt->has_arg ? "=#" : "") << "\n\n";
    opt++;
  }
}

int main(int argc, char** argv)
{
  signal(SIGABRT, &sighandler);
  signal(SIGTERM, &sighandler);
  signal(SIGINT, &sighandler);
  std::string jackname("iff");
  uint32_t periodsize(512);
  std::string serverport("6978");
  std::string serveraddr("239.255.1.7");
  const char* options = "hj:s:m:p:";
  struct option long_options[] = {
      {"help", 0, 0, 'h'},       {"jackname", 1, 0, 'j'},
      {"periodsize", 1, 0, 's'}, {"multicast", 1, 0, 'm'},
      {"port", 1, 0, 'p'},       {0, 0, 0, 0}};
  int opt(0);
  int option_index(0);
  while((opt = getopt_long(argc, argv, options, long_options, &option_index)) !=
        -1) {
    switch(opt) {
    case 'h':
      usage(long_options);
      return -1;
    case 'j':
      jackname = optarg;
      break;
    case 's':
      periodsize = atoi(optarg);
      break;
    case 'p':
      serverport = optarg;
      break;
    case 'm':
      serveraddr = optarg;
      break;
    }
  }
  if_filter_t iff(serveraddr, serverport, jackname, periodsize);
  iff.activate();
  while(!b_quit) {
    sleep(1);
  }
  iff.deactivate();
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
