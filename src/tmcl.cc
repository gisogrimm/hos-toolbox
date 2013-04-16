#include <stdio.h> // standard input / output functions
#include <string.h> // string function definitions
#include <unistd.h> // UNIX standard function definitions
#include <fcntl.h> // File control definitions
#include <errno.h> // Error number definitions
#include <termios.h> // POSIX terminal control definitionss
#include <time.h>   // time calls
#include <stdint.h>

#include <iostream>

#include "osc_helper.h"


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

class tmcm6110_t : public TASCAR::osc_server_t {
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
  void move_to(uint8_t motor, int32_t pos){
    write_cmd(4,0,motor,pos);
  };
  static int mvp_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
private:
  int configure_port(int);
  int device;
  uint8_t module_addr;
  pthread_mutex_t mtx;
};

int tmcm6110_t::mvp_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (argc==2) && (types[0]=='i') && (types[1]=='i')){
    ((tmcm6110_t*)user_data)->move_to(argv[0]->i,argv[1]->i);
  }
  return 0;
}
  

tmcm6110_t::tmcm6110_t()
  : osc_server_t("","11001"),
    device(-1),
    module_addr(1)
{
  pthread_mutex_init( &mtx, NULL );
  add_method("/mvp","ii",tmcm6110_t::mvp_handler,this);
}

tmcm6110_t::~tmcm6110_t()
{
  if( device != -1 )
    close();
  pthread_mutex_trylock( &mtx );
  pthread_mutex_unlock(  &mtx);
  pthread_mutex_destroy( &mtx );
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
  pthread_mutex_lock( &mtx );
  try{
    printf("%d: type=%d motor=%d val=%d\n",command_no,type_no,motor_no,value);
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
        retv_value = (data[4]<<24) & (data[5]<<16) & (data[6]<<8) & (data[7]);
        if( retv != 100 ){
          throw tmcm_error(retv);
        }
        printf("cmd ok\n");
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
  usleep(20000);
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

int main(int argc, char** argv)
{
  try{
    tmcm6110_t tmcm;
    std::string dev("/dev/ttyACM0");
    if( argc > 1 )
      dev = argv[1];
    tmcm.open(dev);
    // acceleration
    tmcm.write_cmd(5,5,0,400);
    // velocity
    tmcm.write_cmd(5,4,0,2047);
    // standby current
    tmcm.write_cmd(5,7,0,12);
    // micro steps:
    tmcm.write_cmd(5,140,0,5);
    // reference:
    tmcm.write_cmd(13,0,0,0);
    bool b_run(true);
    tmcm.activate();
    while( b_run ){
      try{
        char s[1024];
        if( gets(s) ){
          char c_tmp[1024];
          memset(c_tmp,0,1024);
          uint32_t v;
          double d(0);
          if( sscanf(s, "ror %d",&v)==1 )
            tmcm.rotate(0,v,tmcm6110_t::right);
          if( sscanf(s, "rol %d",&v)==1 )
            tmcm.rotate(0,v,tmcm6110_t::left);
          if( strncmp(s,"mst",3)==0)
            tmcm.motor_stop(0);
          if( sscanf(s, "mvp %d",&v)==1 )
            tmcm.move_to(0,v);
          if( sscanf(s, "mv %lf",&d)==1 ){
            tmcm.move_to(0,(int32_t)(-8000.0*d));
          }
          if( sscanf(s, "sap_cs %d",&v)==1 )
            tmcm.write_cmd(5,6,0,v);
          if( sscanf(s, "sap_sc %d",&v)==1 )
            tmcm.write_cmd(5,7,0,v);
          if( sscanf(s, "sap_acc %d",&v)==1 )
            tmcm.write_cmd(5,5,0,v);
          if( sscanf(s, "sap_vel %d",&v)==1 )
            tmcm.write_cmd(5,4,0,v);
          if( sscanf(s, "sap_micro %d",&v)==1 )
            tmcm.write_cmd(5,140,0,v);
          if( sscanf(s, "rfs %s",c_tmp)==1 ){
            if( strcmp(c_tmp,"start")==0 )
              tmcm.write_cmd(13,0,0,0);
            if( strcmp(c_tmp,"stop")==0 )
              tmcm.write_cmd(13,1,0,0);
            if( strcmp(c_tmp,"status")==0 )
              printf("reference search %d\n",tmcm.write_cmd(13,2,0,0));
          }
          if( strncmp(s,"quit",4)==0){
            b_run = false;
            tmcm.move_to(0,0);
          }
        }
      }
      catch( const std::exception& e ){
        std::cerr << "Warning: " << e.what() << std::endl;
      }
    }
    tmcm.deactivate();
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
