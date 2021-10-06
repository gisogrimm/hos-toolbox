#include <alsa/asoundlib.h>
#include <alsa/seq_event.h>
#include <iostream>
#include <mutex>
#include <tascar/cli.h>
#include <tascar/errorhandling.h>
#include <thread>

#include "libhos_midi_ctl.h"

#define DEBUG(x)                                                               \
  std::cerr << __FILE__ << ":" << __LINE__ << " " << #x << "=" << x << std::endl
#define MIDISC 0.0078740157480314959537182f

class midi_client_t : public midi_ctl_t {
public:
  midi_client_t(const std::string& name, const std::string& cmd_);
  ~midi_client_t();
  void command_runner();

protected:
  void emit_pc(int channel, int prgm);

private:
  std::thread runthread;
  std::mutex mtx;
  bool brunthread = true;
  int nextval = 0;
  FILE* h_cmdpipe;
  std::string cmd;
};

midi_client_t::midi_client_t(const std::string& name, const std::string& cmd_)
    : midi_ctl_t(name), cmd(cmd_)
{
  h_cmdpipe = popen("/bin/bash -s", "w");
  if(!h_cmdpipe)
    throw TASCAR::ErrMsg(
        "Unable to create pipe for triggered command with /bin/bash");
  runthread = std::thread(&midi_client_t::command_runner, this);
}

midi_client_t::~midi_client_t()
{
  brunthread = false;
  if(runthread.joinable())
    runthread.join();
  fclose(h_cmdpipe);
}

void midi_client_t::command_runner()
{
  char ctmp[1024];
  memset(ctmp, 0, 1024);
  while(brunthread) {
    mtx.lock();
    int val(nextval);
    nextval = 0;
    mtx.unlock();
    if(val) {
      std::cout << val + 1 << std::endl;
      snprintf(ctmp, 1024, "%s %d", cmd.c_str(), val + 1);
      fprintf(h_cmdpipe, "%s\n", ctmp);
      fflush(h_cmdpipe);
    } else {
      usleep(1000);
    }
  }
}

void midi_client_t::emit_pc(int channel, int param)
{
  mtx.lock();
  nextval = param;
  mtx.unlock();
}

int main(int argc, char** argv)
{
  std::string name("pc2cmd");
  std::string connect("");
  std::string cmd("echo");
  const char* options = "hn:c:";
  struct option long_options[] = {{"help", 0, 0, 'h'},
                                  {"name", 1, 0, 'n'},
                                  {"connect", 1, 0, 'c'},
                                  {0, 0, 0, 0}};
  int opt(0);
  int option_index(0);
  while((opt = getopt_long(argc, argv, options, long_options, &option_index)) !=
        -1) {
    switch(opt) {
    case 'h':
      TASCAR::app_usage("hos_midipc2cmd", long_options, "command");
      return -1;
    case 'n':
      name = optarg;
      break;
    case 'c':
      connect = optarg;
      break;
    }
  }
  if(optind < argc)
    cmd = argv[optind++];
  midi_client_t s(name, cmd);
  s.start_service();
  if(connect.size())
    s.connect_input(connect);
  while(true)
    sleep(1);
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
