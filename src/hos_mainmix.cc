
/*
 * hos_mainmix - read gain from jack port, create direct mapping in
 * HDSP 9652 from software output to hardware output with gain from
 * jack port.
 */

#include "jackclient.h"
#include <alsa/asoundlib.h>
#include <getopt.h>
#include <iostream>
#include <signal.h>

static bool b_quit;

class mainmix_t : public jackc_t {
public:
  mainmix_t(const std::string& jackname);
  virtual ~mainmix_t();
  void run();
private:
  virtual int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>&);
  void upload(float val);
  float gain;
  float oldgain;
};

mainmix_t::mainmix_t(const std::string& jackname)
  : jackc_t(jackname), gain(0),oldgain(1)
{
  add_input_port("gain");
  activate();
  snd_ctl_elem_id_t *id;
  snd_ctl_elem_id_alloca(&id);
  snd_ctl_elem_id_set_name(id, "Mixer");
  snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_HWDEP);
  snd_ctl_elem_id_set_device(id, 0);
  snd_ctl_elem_id_set_index(id, 0);
  snd_ctl_elem_value_t *ctl;
  snd_ctl_elem_value_alloca(&ctl);
  snd_ctl_elem_value_set_id(ctl, id);
  snd_ctl_t *handle;
  int err;
  if ((err = snd_ctl_open(&handle, "hw:DSP", SND_CTL_NONBLOCK)) < 0) {
    fprintf(stderr, "Alsa error: %s\n", snd_strerror(err));
    return;
  }
  for( uint32_t k_in=0;k_in<52;k_in++ ){
    for( uint32_t k_out=0;k_out<26;k_out++ ){
      snd_ctl_elem_value_set_integer(ctl, 0, k_in);
      snd_ctl_elem_value_set_integer(ctl, 1, k_out);
      snd_ctl_elem_value_set_integer(ctl, 2, 0);
      if ((err = snd_ctl_elem_write(handle, ctl)) < 0) {
        fprintf(stderr, "Alsa error: %s\n", snd_strerror(err));
      }
    }
  }
  snd_ctl_close(handle);
}

mainmix_t::~mainmix_t()
{
  deactivate();
}

int mainmix_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>&)
{
  gain = inBuffer[0][0];
  return 0;
}

void mainmix_t::run()
{
  uint32_t cnt(20);
  while(!b_quit){
    usleep(30000);
    if( (gain != oldgain)||(!cnt) ){
      upload(gain);
      oldgain = gain;
      cnt = 20;
    }
    if( cnt )
      cnt--;
  }
}

void mainmix_t::upload(float val)
{
  snd_ctl_elem_id_t *id;
  snd_ctl_elem_id_alloca(&id);
  snd_ctl_elem_id_set_name(id, "Mixer");
  snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_HWDEP);
  snd_ctl_elem_id_set_device(id, 0);
  snd_ctl_elem_id_set_index(id, 0);
  snd_ctl_elem_value_t *ctl;
  snd_ctl_elem_value_alloca(&ctl);
  snd_ctl_elem_value_set_id(ctl, id);
  snd_ctl_t *handle;
  int err;
  if ((err = snd_ctl_open(&handle, "hw:DSP", SND_CTL_NONBLOCK)) < 0) {
    fprintf(stderr, "Alsa error: %s\n", snd_strerror(err));
    return;
  }
  for( uint32_t k=0;k<26;k++ ){
    snd_ctl_elem_value_set_integer(ctl, 0, k+26);
    snd_ctl_elem_value_set_integer(ctl, 1, k);
    snd_ctl_elem_value_set_integer(ctl, 2, (int)(32767*std::min(1.0f,std::max(0.0f,val))));
    if ((err = snd_ctl_elem_write(handle, ctl)) < 0) {
      fprintf(stderr, "Alsa error: %s\n", snd_strerror(err));
    }
  }
  snd_ctl_close(handle);
}

void usage(struct option * opt)
{
  std::cout << "Usage:\n\nhos_mainmix [options]\n\nOptions:\n\n";
  std::cout << "Read gain from jack port, create direct mapping in HDSP 9652 from\n"
    "software output to hardware output with gain from jack port.\n";
  while( opt->name ){
    std::cout << "  -" << (char)(opt->val) << " " << (opt->has_arg?"#":"") <<
      "\n  --" << opt->name << (opt->has_arg?"=#":"") << "\n\n";
    opt++;
  }
}

static void sighandler(int sig)
{
  b_quit = true;
}

int main(int argc, char** argv)
{
  b_quit = false;
  signal(SIGABRT, &sighandler);
  signal(SIGTERM, &sighandler);
  signal(SIGINT, &sighandler);
  std::string jackname("mainmix");
  const char *options = "hj:";
  struct option long_options[] = { 
    { "help",      0, 0, 'h' },
    { "jackname",  1, 0, 'j' },
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
    }
  }
  mainmix_t mm(jackname);
  mm.run();
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
