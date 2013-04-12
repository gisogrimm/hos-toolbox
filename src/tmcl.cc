#include <stdio.h> // standard input / output functions
#include <string.h> // string function definitions
#include <unistd.h> // UNIX standard function definitions
#include <fcntl.h> // File control definitions
#include <errno.h> // Error number definitions
#include <termios.h> // POSIX terminal control definitionss
#include <time.h>   // time calls
#include <stdint.h>

#include <iostream>


#define DEBUG(x) std::cerr << __FILE__ << ":" << __LINE__ << " " #x "=" << x << std::endl

class str_error : public std::exception {
public:
  str_error(const std::string& msg):msg_(msg){};
  ~str_error() throw() {};
  const char* what() const throw() { return msg_.c_str();};
private:
  std::string msg_;
};

class tmcm_error : public std::exception {
public:
  tmcm_error(uint8_t err_no);
  const char* what() const throw() {
    switch( errno_ ){
      case 101 : return "Command loaded into TMCL program error";
      case 1 : return "Wrong checksum";
      case 2 : return "Invalid command";
      case 3 : return "Wrong type";
      case 4 : return "Invalid value";
      case 5 : return "Configuration EEPROM locked";
      case 6 : return "Command not available";
      };
    return "Unknown error";
  };
private:
  uint32_t errno_;
};

tmcm_error::tmcm_error(uint8_t err_no)
 : errno_(errno)
{
}

class tmcm6110_t {
public:
  enum dir_t { left, right };
  tmcm6110_t();
  ~tmcm6110_t();
  void open(const std::string& devname);
  void set_module_addr(uint8_t ma);
  uint32_t write_cmd(uint8_t cmd_no,uint8_t type_no,uint8_t motor_no,uint32_t value);
  void close();
  void rotate(uint8_t motor,uint32_t vel,dir_t dir);
  void motor_stop(uint8_t motor){write_cmd(3,0,motor,0);};
private:
  int configure_port(int);
  int device;
  uint8_t module_addr;
};

tmcm6110_t::tmcm6110_t()
  : device(-1),
    module_addr(1)
{
}

tmcm6110_t::~tmcm6110_t()
{
  if( device != -1 )
    close();
}

void tmcm6110_t::rotate(uint8_t motor,uint32_t vel,dir_t dir)
{
  uint8_t cmd(1);
  switch( dir ){
  case right :
    cmd = 1;
  case left :
    cmd = 2;
  }
  write_cmd(cmd,0,motor,vel);
}

uint32_t tmcm6110_t::write_cmd(uint8_t command_no,
                               uint8_t type_no,
                               uint8_t motor_no,
                               uint32_t value)
{
  uint32_t retv_value(0);
  fd_set rdfs;
  struct timeval timeout;
  // initialise the timeout structure
  timeout.tv_sec = 10; // ten second timeout
  timeout.tv_usec = 0;
  uint8_t retv(0);
  uint8_t data[9];
  data[0] = module_addr;
  data[1] = command_no;
  data[2] = type_no;
  data[3] = motor_no;
  data[4] = (value>>24) & 0xff;
  data[5] = (value>>16) & 0xff;
  data[6] = (value>>8) & 0xff;
  data[7] = value & 0xff;
  data[8] = 0;
  for(unsigned int k=0;k<8;k++)
    data[8] += data[k];
  int wcnt(write(device, data, 9));
  DEBUG(wcnt);
  FD_ZERO( &rdfs );
  FD_SET( device, &rdfs );
  // do the select
  int n = select(device + 1, &rdfs, NULL, NULL, &timeout);
  // check if an error has occured
  if(n < 0){
    throw str_error("select failed");
  }
  if( n >0 ){
    size_t rlen(read(device,data,9));
    if( rlen == 9 ){
      retv = data[2];
      retv_value = (data[4]<<24) & (data[5]<<16) & (data[6]<<8) & (data[7]);
      if( retv != 100 ){
        throw tmcm_error(retv);
      }
    }else{
      DEBUG(rlen);
    }
  }else{
    DEBUG(n);
    throw str_error("Timeout while reading response");
  }
  return retv_value;
}                 

void tmcm6110_t::open(const std::string& devname)
{
  device = ::open(devname.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
  if(device == -1){
    throw str_error("Unable to open device "+devname);
  }else{
    fcntl(device, F_SETFL, 0);
    printf("port is open.\n");
  }
  configure_port(device);
} //open_port

int tmcm6110_t::configure_port(int fd)      // configure the port
{
  struct termios port_settings;      // structure to store the port settings in

  cfsetispeed(&port_settings, B9600);    // set baud rates
  cfsetospeed(&port_settings, B9600);

  port_settings.c_cflag &= ~PARENB;    // set no parity, stop bits, data bits
  port_settings.c_cflag &= ~CSTOPB;
  port_settings.c_cflag &= ~CSIZE;
  port_settings.c_cflag |= CS8;
	
  tcsetattr(fd, TCSANOW, &port_settings);    // apply the settings to the port
  return(fd);

} //configure_port

void tmcm6110_t::close()
{
  ::close(device);
}

#include <fstream>

int main(void)
{
  try{
  tmcm6110_t tmcm;
  tmcm.open("/dev/ttyACM0");
  tmcm.rotate(0,1000,tmcm6110_t::left);
  sleep(4);
  tmcm.motor_stop(0);
  }
  catch( const std::exception& e){
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return(0);
	
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
