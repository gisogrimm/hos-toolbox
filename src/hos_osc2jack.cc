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

class v_event_t {
public:
  v_event_t() : t(0.0), v(0.0){};
  v_event_t(double t_, float v_) : t(t_), v(v_){};
  double t;
  float v;
};

class looper_t {
public:
  enum mode_t { play, mute, rec, bypass };
  looper_t();
  void set_mode(mode_t m);
  float process(double t, float oscv);
  std::vector<v_event_t> loop;
  mode_t mode;
  float lastv;
  double lastt;
};

looper_t::looper_t() : mode(mute), lastv(0.0), lastt(0.0)
{
  loop.reserve(8192);
}

void looper_t::set_mode(mode_t m)
{
  if((m == rec) && (mode != rec))
    loop.clear();
  mode = m;
}

float looper_t::process(double t, float oscv)
{
  float rv(oscv);
  switch(mode) {
  case play:
    for(std::vector<v_event_t>::iterator it = loop.begin(); it != loop.end();
        ++it) {
      if((it->t <= t) && ((lastt >= t) || (it->t > lastt)))
        rv = it->v;
    }
    break;
  case mute:
    rv = 0.0f;
    break;
  case rec:
    loop.push_back(v_event_t(t, oscv));
    break;
  case bypass:
    break;
  }
  return rv;
}

class osc2jack_t : public jackc_t, public TASCAR::osc_server_t {
public:
  osc2jack_t(const std::string& oscad, const std::vector<std::string>& names,
             const std::string& multicast, const std::string& serverport,
             const std::string& jackname, bool use_touchosc_);
  ~osc2jack_t();
  void run();
  int process(jack_nframes_t nframes, const std::vector<float*>& inBuffer,
              const std::vector<float*>& outBuffer);
  void set_xy(float x, float y)
  {
    vx = x;
    vy = y;
  };
  static int osc_set_xy(const char* path, const char* types, lo_arg** argv,
                        int argc, lo_message msg, void* user_data);
  static int osc_upload(const char* path, const char* types, lo_arg** argv,
                        int argc, lo_message msg, void* user_data);
  void upload_touchosc();

private:
  uint32_t npar;
  float* v;
  std::vector<looper_t> vloop;
  int32_t bar;
  lo_address lo_addr;
  float matrix[96];
  float xy1, xy2, xy3;
  float vx, vy;
  float flt1_f, flt1_q;
  float flt2_f, flt2_q;
  float flt3_f, flt3_q;
  bool use_touchosc;
};

osc2jack_t::osc2jack_t(const std::string& oscad,
                       const std::vector<std::string>& names,
                       const std::string& multicast,
                       const std::string& serverport,
                       const std::string& jackname, bool use_touchosc_)
    : jackc_t(jackname), TASCAR::osc_server_t(multicast, serverport, "UDP"),
      npar(names.size()), v(new float[std::max(1u, npar)]), bar(0),
      lo_addr(lo_address_new_from_url(oscad.c_str())), xy1(0.0), xy2(0), xy3(0),
      vx(0), vy(0), flt1_f(0), flt1_q(0), flt2_f(0), flt2_q(0), flt3_f(0),
      flt3_q(0), use_touchosc(use_touchosc_)
{
  std::string prefix("/" + jackname + "/");
  add_input_port("time");
  if(use_touchosc) {
    add_output_port("matrix_1");
    add_output_port("matrix_2");
    add_output_port("matrix_3");
    add_output_port("matrix_4");
    add_output_port("matrix_5");
    add_output_port("matrix_6");
    add_output_port("resflt1_f");
    add_output_port("resflt1_q");
    add_output_port("resflt2_f");
    add_output_port("resflt2_q");
    add_output_port("resflt3_f");
    add_output_port("resflt3_q");
  }
  add_bool_true(prefix + "quit", &b_quit);
  add_float("/4/toggle1", &xy1);
  add_float("/4/toggle2", &xy2);
  add_float("/4/toggle3", &xy3);
  add_method("/4/xy", "ff", osc2jack_t::osc_set_xy, this);
  for(uint32_t k = 0; k < names.size(); k++) {
    add_float(prefix + names[k], &(v[k]));
    add_output_port(names[k]);
  }
  vloop.resize(names.size());
  memset(matrix, 0, sizeof(float) * 96);
  for(uint32_t ch = 0; ch < 3; ch++)
    for(uint32_t t = 0; t < 16; t++) {
      matrix[ch + 6 * t] = 1.0;
    }
  if(use_touchosc) {
    upload_touchosc();
    add_method(prefix + "upload", "", osc_upload, this);
    for(uint32_t ch = 0; ch < 6; ch++)
      for(uint32_t t = 0; t < 16; t++) {
        char addr[1024];
        sprintf(addr, "/2/multitoggle/%d/%d", ch + 1, t + 1);
        add_float(addr, &(matrix[ch + 6 * t]));
      }
    lo_send(lo_addr, "/2", "");
    lo_send(lo_addr, "/4/xy", "ff", 0.0f, 0.5f);
  }
}

void osc2jack_t::upload_touchosc()
{
  for(uint32_t ch = 0; ch < 6; ch++)
    for(uint32_t t = 0; t < 16; t++) {
      char addr[1024];
      sprintf(addr, "/2/multitoggle/%d/%d", ch + 1, t + 1);
      lo_send(lo_addr, addr, "f", matrix[ch + 6 * t]);
      usleep(5000);
    }
  lo_send(lo_addr, "/4/toggle1", "f", xy1);
  lo_send(lo_addr, "/4/toggle2", "f", xy2);
  lo_send(lo_addr, "/4/toggle3", "f", xy3);
  for(uint32_t ch = 3; ch < 5; ch++) {
    char addr[1024];
    sprintf(addr, "/4/toggle%d", ch + 1);
    lo_send(lo_addr, addr, "f", 0.0f);
  }
}

int osc2jack_t::osc_set_xy(const char* path, const char* types, lo_arg** argv,
                           int argc, lo_message msg, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 'f') && (types[1] == 'f'))
    ((osc2jack_t*)user_data)->set_xy(argv[1]->f, argv[0]->f);
  return 0;
}

int osc2jack_t::osc_upload(const char* path, const char* types, lo_arg** argv,
                           int argc, lo_message msg, void* user_data)
{
  if(user_data)
    ((osc2jack_t*)user_data)->upload_touchosc();
  return 0;
}

osc2jack_t::~osc2jack_t()
{
  delete[] v;
}

void osc2jack_t::run()
{
  jackc_t::activate();
  TASCAR::osc_server_t::activate();
  connect_in(0, "mha-house:time", true);
  if(use_touchosc) {
    connect_out(0, "mha-house:cv_bass", true);
    connect_out(1, "mha-house:cv_mid", true);
    connect_out(2, "mha-house:cv_high", true);
    connect_out(6, "resflt1:f", true);
    connect_out(7, "resflt1:q", true);
    connect_out(8, "resflt2:f", true);
    connect_out(9, "resflt2:q", true);
    connect_out(10, "resflt3:f", true);
    connect_out(11, "resflt3:q", true);
  }
  while(!b_quit)
    usleep(50000);
  TASCAR::osc_server_t::deactivate();
  jackc_t::deactivate();
}

int osc2jack_t::process(jack_nframes_t nframes,
                        const std::vector<float*>& inBuffer,
                        const std::vector<float*>& outBuffer)
{
  if(use_touchosc) {
    int32_t nbar(0.25 * inBuffer[0][0]);
    nbar = std::min(15, std::max(0, nbar));
    if(nbar != bar) {
      char addr[1024];
      sprintf(addr, "/2/led%d", nbar + 1);
      lo_send(lo_addr, addr, "f", 1.0f);
      sprintf(addr, "/2/led%d", bar + 1);
      lo_send(lo_addr, addr, "f", 0.0f);
    }
    bar = nbar;
    if(xy1 > 0.0f) {
      flt1_f = vx;
      flt1_q = 0.95 * vy;
    }
    if(xy2 > 0.0f) {
      flt2_f = vx;
      flt2_q = 0.95 * vy;
    }
    if(xy3 > 0.0f) {
      flt3_f = vx;
      flt3_q = 0.95 * vy;
    }
    for(uint32_t k = 0; k < nframes; k++) {
      outBuffer[6][k] = flt1_f;
      outBuffer[7][k] = flt1_q * matrix[3 + 6 * nbar];
      outBuffer[8][k] = flt2_f;
      outBuffer[9][k] = flt2_q * matrix[3 + 6 * nbar];
      outBuffer[10][k] = flt3_f;
      outBuffer[11][k] = flt3_q * matrix[3 + 6 * nbar];
    }
    for(uint32_t ch = 0; ch < 6; ch++)
      for(uint32_t k = 0; k < nframes; k++)
        outBuffer[ch][k] = matrix[ch + 6 * nbar];
  }
  for(uint32_t ch = 0; ch < npar; ch++)
    for(uint32_t k = 0; k < nframes; k++)
      outBuffer[ch + 12 * use_touchosc][k] = v[ch];
  return 0;
}

static void sighandler(int sig)
{
  b_quit = true;
}

void usage(struct option* opt)
{
  std::cout
      << "Usage:\n\nhos_osc2jack [options] path [ path .... ]\n\nOptions:\n\n";
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
  std::string jackname("osc2jack");
  std::string touchosc("osc.udp://192.168.178.31:9000/");
  std::string serverport("8000");
  std::string serveraddr("");
  bool use_touchosc(false);
  std::vector<std::string> path;
  const char* options = "d:hn:p:a:t";
  struct option long_options[] = {
      {"dest", 1, 0, 'd'}, {"help", 0, 0, 'h'},     {"multicast", 1, 0, 'm'},
      {"port", 1, 0, 'p'}, {"jackname", 1, 0, 'n'}, {"touchosc", 0, 0, 't'},
      {0, 0, 0, 0}};
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
    case 't':
      use_touchosc = true;
      break;
    }
  }
  while(optind < argc)
    path.push_back(argv[optind++]);
  // if( path.empty() ){
  //  usage(long_options);
  //  return -1;
  //}
  osc2jack_t s(touchosc, path, serveraddr, serverport, jackname, use_touchosc);
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
