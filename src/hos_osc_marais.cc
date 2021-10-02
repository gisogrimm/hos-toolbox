#include "hos_defs.h"
#include <getopt.h>
#include <iostream>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tascar/errorhandling.h>
#include <tascar/jackclient.h>
#include <tascar/osc_helper.h>
#include <unistd.h>

static bool b_quit;

class osc_marais_t : public jackc_t, public TASCAR::osc_server_t {
public:
  osc_marais_t(const std::string& oscad, const std::string& multicast,
               const std::string& serverport, const std::string& jackname);
  ~osc_marais_t();
  void run();
  int process(jack_nframes_t nframes, const std::vector<float*>& inBuffer,
              const std::vector<float*>& outBuffer);
  static int osc_upload(const char* path, const char* types, lo_arg** argv,
                        int argc, lo_message msg, void* user_data);
  void upload_touchosc();

private:
  lo_address lo_addr;
  float g_f, g_q;
  float g_flt1, g_flt2;
  float g_echo;
  float g_detone;
};

osc_marais_t::osc_marais_t(const std::string& oscad,
                           const std::string& multicast,
                           const std::string& serverport,
                           const std::string& jackname)
    : jackc_t(jackname), TASCAR::osc_server_t(multicast, serverport, "UDP"),
      lo_addr(lo_address_new_from_url(oscad.c_str())), g_f(0), g_q(0),
      g_flt1(0), g_flt2(0), g_echo(0), g_detone(0)
{
  std::string prefix("/" + jackname + "/");
  add_output_port("cv_1f");
  add_output_port("cv_1q");
  add_output_port("cv_2f");
  add_output_port("cv_2q");
  add_output_port("cv_echo");
  add_output_port("cv_detone");
  add_bool_true(prefix + "quit", &b_quit);
  add_float("/1/fader1", &g_f);
  add_float("/1/fader2", &g_q);
  add_float("/1/toggle1", &g_flt1);
  add_float("/1/toggle2", &g_flt2);
  add_float("/1/push7", &g_echo);
  add_float("/1/push4", &g_detone);
  add_method(prefix + "upload", "", osc_upload, this);
  upload_touchosc();
}

void osc_marais_t::upload_touchosc()
{
  lo_send(lo_addr, "/1/fader1", "f", g_f);
  lo_send(lo_addr, "/1/fader2", "f", g_q);
  lo_send(lo_addr, "/1/toggle1", "f", g_flt1);
  lo_send(lo_addr, "/1/toggle2", "f", g_flt2);
  lo_send(lo_addr, "/1/push7", "f", g_echo);
  lo_send(lo_addr, "/1/push4", "f", g_detone);
  lo_send(lo_addr, "/1", "");
}

int osc_marais_t::osc_upload(const char* path, const char* types, lo_arg** argv,
                             int argc, lo_message msg, void* user_data)
{
  if(user_data)
    ((osc_marais_t*)user_data)->upload_touchosc();
  return 0;
}

osc_marais_t::~osc_marais_t() {}

void osc_marais_t::run()
{
  jackc_t::activate();
  TASCAR::osc_server_t::activate();
  connect_out(0, "resflt1:f", true);
  connect_out(1, "resflt1:q", true);
  connect_out(2, "resflt2:f", true);
  connect_out(3, "resflt2:q", true);
  connect_out(4, "mha-sigproc:cv_delay", true);
  connect_out(5, "mha-sigproc:cv_detone", true);
  while(!b_quit)
    usleep(50000);
  TASCAR::osc_server_t::deactivate();
  jackc_t::deactivate();
}

int osc_marais_t::process(jack_nframes_t nframes,
                          const std::vector<float*>& inBuffer,
                          const std::vector<float*>& outBuffer)
{
  for(uint32_t k = 0; k < nframes; k++) {
    outBuffer[0][k] = g_f;
    outBuffer[1][k] = g_q * g_flt1;
    outBuffer[2][k] = g_f;
    outBuffer[3][k] = g_q * g_flt2;
    outBuffer[4][k] = g_echo;
    outBuffer[5][k] = g_detone;
  }
  return 0;
}

static void sighandler(int sig)
{
  b_quit = true;
}

void usage(struct option* opt)
{
  std::cout << "Usage:\n\nhos_osc_marais [options]\n\nOptions:\n\n";
  while(opt->name) {
    std::cout << "  -" << (char)(opt->val) << " " << (opt->has_arg ? "#" : "")
              << "\n  --" << opt->name << (opt->has_arg ? "=#" : "") << "\n\n";
    opt++;
  }
}

int main(int argc, char** argv)
{
  b_quit = false;
  signal(SIGABRT, &sighandler);
  signal(SIGTERM, &sighandler);
  signal(SIGINT, &sighandler);
  std::string jackname("osc_marais");
  std::string touchosc("osc.udp://192.168.178.31:9000/");
  std::string serverport("8000");
  std::string serveraddr("");
  const char* options = "d:hn:p:a:";
  struct option long_options[] = {{"dest", 1, 0, 'd'},      {"help", 0, 0, 'h'},
                                  {"multicast", 1, 0, 'm'}, {"port", 1, 0, 'p'},
                                  {"jackname", 1, 0, 'n'},  {0, 0, 0, 0}};
  int opt(0);
  int option_index(0);
  while((opt = getopt_long(argc, argv, options, long_options, &option_index)) !=
        -1) {
    switch(opt) {
    case 'd':
      touchosc = optarg;
      break;
    case 'p':
      serverport = optarg;
      break;
    case 'm':
      serveraddr = optarg;
      break;
    case 'h':
      usage(long_options);
      return -1;
    case 'n':
      jackname = optarg;
      break;
    }
  }
  osc_marais_t s(touchosc, serveraddr, serverport, jackname);
  s.run();
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
