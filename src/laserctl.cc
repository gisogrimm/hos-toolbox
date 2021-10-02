#include <exception>
#include <stdint.h>
#include <string>
//#include <fcntl.h> // File control definitions
#include <iostream>
//#include <termios.h> // POSIX terminal control definitionss
#include <SerialStream.h>
#include <signal.h>
#include <string.h>
#include <tascar/osc_helper.h>
#include <unistd.h>

#define DEBUG(x)                                                               \
  std::cerr << __FILE__ << ":" << __LINE__ << " " << #x << "=" << x << std::endl

using namespace LibSerial;

static bool b_quit;

// static bool liblo_errflag;

class str_error : public std::exception {
public:
  str_error(const std::string& msg) : msg_(msg){};
  ~str_error() throw(){};
  const char* what() const throw() { return msg_.c_str(); };

private:
  std::string msg_;
};

class tasten_t : public SerialPort, public TASCAR::osc_server_t {
public:
  tasten_t(const std::string& dev, const std::string& server_address,
           const std::string& server_port);
  ~tasten_t() throw();
  void run();
  // void process(const std::string& cmd);
  static int osc_set_mic(const char* path, const char* types, lo_arg** argv,
                         int argc, lo_message msg, void* user_data);
  void osc_set_mic(int32_t m);

private:
  int32_t pos;
  uint32_t npos;
};

int tasten_t::osc_set_mic(const char* path, const char* types, lo_arg** argv,
                          int argc, lo_message msg, void* user_data)
{
  if(user_data && (argc == 1) && (types[0] == 'i'))
    ((tasten_t*)user_data)->osc_set_mic(argv[0]->i);
  return 0;
}

void tasten_t::osc_set_mic(int32_t m)
{
  if(m >= 0) {
    int32_t delta(m - pos);
    while(delta > 1024)
      delta -= 2048;
    while(delta < -1024)
      delta += 2048;
    char ctmp[1024];
    sprintf(ctmp, "%d\n", delta);
    Write(ctmp);
    pos = m;
    // std::cerr << "wrote " << ctmp;
  }
}

tasten_t::tasten_t(const std::string& dev, const std::string& server_address,
                   const std::string& server_port)
    : SerialPort(dev), osc_server_t(server_address, server_port, "UDP"), pos(0),
      npos(8)
{
  Open(BAUD_9600, CHAR_SIZE_8, PARITY_NONE, STOP_BITS_1, FLOW_CONTROL_NONE);
  add_method("/pos", "i", tasten_t::osc_set_mic, this);
  activate();
}

tasten_t::~tasten_t() throw()
{
  deactivate();
}

void tasten_t::run()
{
  std::string cmd;
  while(!b_quit) {
    try {
      cmd = ReadLine(1000, '\n');
      if(cmd.size() > 0)
        cmd.erase(cmd.end() - 1);
      std::cout << cmd << std::endl;
      cmd = "";
    }
    catch(const ReadTimeout& t) {
      // timeoutcnt++;
      // lo_send(lo_addr,"/timeout","i",timeoutcnt);
    }
  }
}

static void sighandler(int sig)
{
  b_quit = true;
}

int main(int argc, char** argv)
{
  try {
    b_quit = false;
    signal(SIGABRT, &sighandler);
    signal(SIGTERM, &sighandler);
    signal(SIGINT, &sighandler);
    if(argc < 2) {
      std::cerr << "Usage: tasten <device>\n";
      return -1;
    }
    tasten_t t(argv[1], "239.255.1.7", "9877");
    t.run();
    return 0;
  }
  catch(const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make"
 * End:
 */
