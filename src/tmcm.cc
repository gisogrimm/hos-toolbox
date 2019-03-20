#include "tmcm.h"
#include <fcntl.h> // File control definitions
#include <iostream>
#include <termios.h> // POSIX terminal control definitionss
#include <string.h>
#include <unistd.h>
#include <tascar/errorhandling.h>

#define DEBUG(x) std::cerr << __FILE__ << ":" << __LINE__ << " " #x "=" << x << std::endl

tmcm_error::tmcm_error(uint8_t err_no)
  : errno_(err_no)
{
}

const char* tmcm_error::what() const throw()
{
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
}

tmcm6110_t::tmcm6110_t()
  : device(-1),
    module_addr(1)
{
  pthread_mutex_init( &mtx, NULL );
}

tmcm6110_t::~tmcm6110_t()
{
  if( device != -1 )
    dev_close();
  pthread_mutex_trylock( &mtx );
  pthread_mutex_unlock(  &mtx);
  pthread_mutex_destroy( &mtx );
}


void tmcm6110_t::dev_open(const std::string& devname)
{
  device = open(devname.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
  if(device == -1){
    throw str_error("Unable to open device "+devname);
  }else{
    fcntl(device, F_SETFL, 0);
    std::cout << "port is open.\n";
  }
  configure_port(device);
  usleep(20000);
  write_cmd(9,65,0,7);
}

int tmcm6110_t::configure_port(int fd)      // configure the port
{
  struct termios port_settings;      // structure to store the port settings in
  memset(&port_settings,0,sizeof(port_settings));
  //cfsetispeed(&port_settings, B9600);    // set baud rates
  //cfsetospeed(&port_settings, B9600);
  cfsetispeed(&port_settings, B115200);    // set baud rates
  if( tcsetattr(fd, TCSANOW, &port_settings) != 0 )
    throw TASCAR::ErrMsg("Unable to set ispeed");
  cfsetospeed(&port_settings, B115200);
  if( tcsetattr(fd, TCSANOW, &port_settings) != 0 )
    throw TASCAR::ErrMsg("Unable to set ospeed");
  port_settings.c_cflag &= ~PARENB;    // set no parity, stop bits, data bits
  port_settings.c_cflag &= ~CSTOPB;
  port_settings.c_cflag &= ~CSIZE;
  port_settings.c_cflag |= CS8;
  tcsetattr(fd, TCSANOW, &port_settings);    // apply the settings to the port
  return(fd);
}

void tmcm6110_t::dev_close()
{
  ::close(device);
}

void tmcm6110_t::set_module_addr(uint8_t ma)
{
  module_addr = ma;
}

uint32_t tmcm6110_t::write_cmd(uint8_t command_no,
                               uint8_t type_no,
                               uint8_t motor_no,
                               uint32_t value)
{
  uint32_t retv_value(0);
  pthread_mutex_lock( &mtx );
  try{
    //printf("%d: type=%d motor=%d val=%d\n",command_no,type_no,motor_no,value);
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
    //DEBUG(wcnt);
    if( wcnt != 9 )
      throw str_error("Unable to write 9 bytes to device.");
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
        uint32_t rv(0x1000000*data[4]+0x10000*data[5]+0x100*data[6]+data[7]);
        retv_value = rv;
        if( retv != 100 ){
          throw tmcm_error(retv);
        }
        //printf("cmd ok\n");
      }else{
        DEBUG(rlen);
      }
    }else{
      DEBUG(n);
      throw str_error("Timeout while reading response");
    }
    pthread_mutex_unlock(&mtx);
  }
  catch(...){
    pthread_mutex_unlock(&mtx);
    throw;
  }
  return retv_value;
}                 

void tmcm6110_t::mot_rotate(uint8_t motor,uint32_t vel,dir_t dir)
{
  uint8_t cmd(1);
  switch( dir ){
  case right :
    cmd = 1;
    break;
  case left :
    cmd = 2;
    break;
  }
  write_cmd(cmd,0,motor,vel);
}

void tmcm6110_t::mot_stop(uint8_t motor)
{
  write_cmd(3,0,motor,0);
}

void tmcm6110_t::mot_moveto(uint8_t motor, int32_t pos)
{
  write_cmd(4,0,motor,pos);
}

void tmcm6110_t::mot_moveto_rel(uint8_t motor, int32_t pos)
{
  write_cmd(4,1,motor,pos);
}

int32_t tmcm6110_t::mot_get_pos( uint8_t motor)
{
  return write_cmd(6,1,motor,0);
}

bool tmcm6110_t::get_target_reached()
{
  bool reached(true);
  for( uint8_t k=0;k<3;k++)
    if( write_cmd(6,8,k,0) == 0 )
      reached = false;
  return reached;
}

void tmcm6110_t::mot_sap(uint8_t motor, uint8_t cmd, uint32_t value)
{
  write_cmd(5,cmd,motor,value);
}

int32_t tmcm6110_t::mot_gap(uint8_t motor, uint8_t cmd)
{
  return write_cmd(6,cmd,motor,0);
}

void tmcm6110_t::mot_rfs(uint8_t motor)
{
  write_cmd(13,0,motor,0);
}

int32_t tmcm6110_t::mot_get_rfs(uint8_t motor)
{
  return write_cmd(13,2,motor,0);
}


/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
