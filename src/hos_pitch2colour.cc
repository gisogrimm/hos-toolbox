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

class pitch2colour_t : public jackc_db_t, public TASCAR::osc_server_t {
public:
  pitch2colour_t(const std::string& server_addr, const std::string& server_port,
                 const std::string& jackname, uint32_t fragsize,
                 std::string const& url, std::string const& path, int p_scale);
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
  float tau_std;
  std::vector<float> pitches;
  lo_message msg;
  lo_address target;
  std::string path_;
  float* p_hue;
  float* p_sat;
  int p_scale;
};

pitch2colour_t::pitch2colour_t(const std::string& server_addr,
                               const std::string& server_port,
                               const std::string& jackname, uint32_t fragsize,
                               const std::string& url, const std::string& path,
                               int p_scale)
    : jackc_db_t(jackname, fragsize), TASCAR::osc_server_t(server_addr,
                                                           server_port, "UDP"),
      ola(4 * fragsize, 2 * fragsize, fragsize, TASCAR::stft_t::WND_HANNING,
          TASCAR::stft_t::WND_HANNING, 0.5),
      dtfft(4 * fragsize, 2 * fragsize, fragsize, TASCAR::stft_t::WND_HANNING,
            0.5),
      d1(fragsize), ifscale(-srate / (TASCAR_PI2)),
      mean_lp(ola.s.n_, srate / (double)fragsize),
      std_lp(ola.s.n_, srate / (double)fragsize),
      pow_lp(2, srate / (double)fragsize), tau_std(0.05), msg(lo_message_new()),
      target(lo_address_new_from_url(url.c_str())), path_(path),
      p_scale(p_scale)
{
  set_prefix("/" + jackname + "/");
  add_bool_true("quit", &b_quit);
  add_float("tau", &tau_std);
  add_input_port("in");
  pitches.resize(12);
  lo_message_add_float(msg, 0.0f);
  lo_message_add_float(msg, 0.0f);
  lo_message_add_float(msg, 1.0f);
  lo_message_add_float(msg, 0.001f);
  auto argv = lo_message_get_argv(msg);
  p_hue = &(argv[0]->f);
  p_sat = &(argv[1]->f);
}

void pitch2colour_t::activate()
{
  jackc_t::activate();
  osc_server_t::activate();
}

void pitch2colour_t::deactivate()
{
  osc_server_t::deactivate();
  jackc_t::deactivate();
}

float sqr(float x)
{
  return x * x;
}

int pitch2colour_t::inner_process(jack_nframes_t n,
                                  const std::vector<float*>& inBuf,
                                  const std::vector<float*>&)
{
  if(inBuf.size() == 0)
    return 1;
  for(auto& p : pitches)
    p = 0.0f;
  TASCAR::wave_t w_in(n, inBuf[0]);
  mean_lp.set_lowpass(tau_std);
  std_lp.set_lowpass(tau_std);
  d1.process(w_in);
  ola.process(w_in);
  dtfft.process(d1);
  for(unsigned int k = 0; k < dtfft.s.n_; k++) {
    dtfft.s.b[k] *= conj(ola.s.b[k]);
    float ifreq(std::arg(dtfft.s.b[k]) * ifscale);
    float ifreq_mean(mean_lp.filter(k, ifreq));
    float intens = std::abs(dtfft.s.b[k]);
    intens *= intens;
    int key = 12.0f * log2f(ifreq_mean / 440.0f);
    key = key % 12;
    pitches[key] += intens;
  }
  float i_mean = 0.0f;
  size_t k_max = 0;
  float i_max = 0.0f;
  for(size_t k = 0; k < pitches.size(); ++k) {
    if(i_max < pitches[k]) {
      i_max = pitches[k];
      k_max = k;
    }
    i_mean += pitches[k];
  }
  // i_mean /= pitches.size();
  i_max /= (i_mean + EPSf);
  // std::cout << k_max << "  " << i_max << std::endl;
  *p_hue = (k_max * 30 * p_scale) % 360;
  *p_sat = std::max(0.0f, std::min(1.0f, i_max));
  lo_send_message(target, path_.c_str(), msg);
  return 0;
}

static void sighandler(int sig)
{
  b_quit = true;
}

void usage(struct option* opt)
{
  std::cout << "Usage:\n\nhos_pitch2colour [options]\n\nOptions:\n\n";
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
  std::string jackname("pitch2col");
  uint32_t periodsize(2048);
  int p_scale(1);
  std::string serverport("6978");
  std::string serveraddr("");
  const char* options = "hj:s:m:p:u:t:l:";
  std::string url("osc.udp://localhost:9877/");
  std::string path("/light/*/hsv");
  struct option long_options[] = {{"help", 0, 0, 'h'},
                                  {"jackname", 1, 0, 'j'},
                                  {"periodsize", 1, 0, 's'},
                                  {"multicast", 1, 0, 'm'},
                                  {"url", 1, 0, 'u'},
                                  {"targetpath", 1, 0, 't'},
                                  {"scale", 1, 0, 'l'},
                                  {"port", 1, 0, 'p'},
                                  {0, 0, 0, 0}};
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
    case 't':
      path = optarg;
      break;
    case 'l':
      p_scale = atoi(optarg);
      break;
    case 'u':
      url = optarg;
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
  pitch2colour_t iff(serveraddr, serverport, jackname, periodsize, url, path,
                     p_scale);
  iff.activate();
  while(!b_quit) {
    usleep(100000);
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
