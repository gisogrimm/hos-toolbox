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

#define TMCL_ROR 1
#define TMCL_ROL 2
#define TMCL_MST 3
#define TMCL_MVP 4
#define TMCL_SIO 14
#define TMCL_GIO 15
#define TMCL_SCO 30
#define TMCL_GCO 31

uint8_t write_tmcl(int fd,
                   uint8_t module_addr,
                   uint8_t command_no,
                   uint8_t type_no,
                   uint8_t motor_no,
                   uint32_t value,
                   uint32_t& ret_value)
{
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
  int wcnt(write(fd, data, 9));
  DEBUG(wcnt);
  FD_ZERO( &rdfs );
  FD_SET( fd, &rdfs );
  // do the select
  int n = select(fd + 1, &rdfs, NULL, NULL, &timeout);
  // check if an error has occured
  if(n < 0){
      perror("select failed\n");
      return 0;
  }
  if( n >0 ){
    size_t rlen(read(fd,data,9));
    if( rlen == 9 ){
      retv = data[2];
      ret_value = (data[4]<<24) & (data[5]<<16) & (data[6]<<8) & (data[7]);
    }else{
      DEBUG(rlen);
    }
  }else{
    DEBUG(n);
  }
  return retv;
}                 

int open_port(void)
{
  int fd; // file description for the serial port
	
  fd = open("/dev/ttyACM0", O_RDWR | O_NOCTTY | O_NDELAY);
	
  if(fd == -1) // if open is unsucessful
    {
      //perror("open_port: Unable to open /dev/ttyS0 - ");
      printf("open_port: Unable to open /dev/ttyS0. \n");
    }
  else
    {
      fcntl(fd, F_SETFL, 0);
      printf("port is open.\n");
    }
	
  return(fd);
} //open_port

int configure_port(int fd)      // configure the port
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

int query_modem(int fd)   // query modem with an AT command
{
  char n;
  fd_set rdfs;
  struct timeval timeout;
	
  // initialise the timeout structure
  timeout.tv_sec = 10; // ten second timeout
  timeout.tv_usec = 0;
	
  //Create byte array
  //unsigned char send_bytes[] = { 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC8, 0xCA};
  //unsigned char send_bytes[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04};
  unsigned char send_bytes[] = {0x01, 0x0f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11};
	
	
  write(fd, send_bytes, 9);  //Send data
  printf("Wrote the bytes. \n");
  
  FD_ZERO( &rdfs );
  FD_SET( fd, &rdfs );
  // do the select
  n = select(fd + 1, &rdfs, NULL, NULL, &timeout);
	
  // check if an error has occured
  if(n < 0)
    {
      perror("select failed\n");
    }
  else if (n == 0)
    {
      puts("Timeout!");
    }
  else
    {
      unsigned char response[1024];
      size_t rlen=0;
      rlen=read(fd,response,40);
      DEBUG(rlen);
      for( unsigned int k=0;k<rlen;k++)
        DEBUG((int)(response[k]));
      std::cout << response << std::endl;
      printf("\nBytes detected on the port!\n");
    }

  return 0;
	
} //query_modem

#include <fstream>

int main(void)
{ 
  //std::fstream file("/dev/ttyACM0");
  //file << ":35" << std::endl; // endl does flush, which may be important
  //std::string response;
  //file >> response;
  //std::cout << response;
  int fd = open_port();
  configure_port(fd);
  //query_modem(fd);
  uint32_t retv(0);
  DEBUG((int)write_tmcl(fd,1,TMCL_ROR,0,0,700,retv));
  DEBUG(retv);
  DEBUG((int)write_tmcl(fd,1,TMCL_GIO,0,0,0,retv));
  DEBUG(retv);
  DEBUG((int)write_tmcl(fd,1,TMCL_GCO,0,0,0,retv));
  DEBUG(retv);
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
