#include "tmcm.h"
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <vector>
#include <math.h>
#include "errorhandling.h"
#include "hos_defs.h"
#include "osc_helper.h"

class hos_spokes_t : public tmcm6110_t, public TASCAR::osc_server_t {
public:
  hos_spokes_t(const std::string& devname);
  void configure_axes();
  void rfs();
  void run();
  void mv(int32_t p);
  void relais_on();
  void relais_off();
  void relais(uint8_t k,bool state);
  static int osc_move_to(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int osc_rfs(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int osc_relais(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
private:
  bool b_quit;
};

int hos_spokes_t::osc_move_to(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc == 1) && (types[0] == 'i') )
    ((hos_spokes_t*)user_data)->mv(argv[0]->i);
  return 0;
}

int hos_spokes_t::osc_rfs(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data )
    ((hos_spokes_t*)user_data)->rfs();
  return 0;
}

int hos_spokes_t::osc_relais(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc==2) && (types[0]=='i') && (types[1]=='i') )
    ((hos_spokes_t*)user_data)->relais(argv[0]->i,argv[1]->i);
  return 0;
}

void hos_spokes_t::mv(int32_t p)
{
  mot_moveto(0,p);
  // standby current
  mot_sap(0,7,std::max(40.0,std::min(0.15*(p-300.0),100.0)));
}

void hos_spokes_t::relais(uint8_t k,bool state)
{
  write_cmd(14,k,2,state);
}

void hos_spokes_t::relais_on()
{
  write_cmd(14,0,2,1);
}

void hos_spokes_t::relais_off()
{
  write_cmd(14,0,2,0);
}

hos_spokes_t::hos_spokes_t(const std::string& devname)
  : tmcm6110_t(),
    TASCAR::osc_server_t("239.255.1.7","6978"),
    b_quit(false)
{
  set_prefix("/tmcm");
  dev_open(devname);
  configure_axes();
  add_method("/mv","i",&hos_spokes_t::osc_move_to,this);
  add_method("/rfs","",&hos_spokes_t::osc_rfs,this);
  add_method("/relais","ii",&hos_spokes_t::osc_relais,this);
  add_bool_true("/quit",&b_quit);
}

void hos_spokes_t::configure_axes()
{
  for( uint8_t k=0;k<3;k++){
    // acceleration
    mot_sap(k,5,100);
    // velocity
    mot_sap(k,4,1800);
    // max current
    mot_sap(k,6,160);
    // standby current
    mot_sap(k,7,10);
    // micro steps:
    mot_sap(k,140,5);
  }
}

void hos_spokes_t::rfs()
{
  // reduce max current:
  mot_sap(0,6,104);
  // rotate left for 200 ms:
  mot_rotate(0,120,left);
  usleep(100000);
  // reduce max current:
  mot_sap(0,6,44);
  // rotate left for 200 ms:
  mot_rotate(0,80,left);
  usleep(100000);
  mot_rotate(0,0,left);
  // reset current position:
  mot_sap(0,1,0);
  // restore max current:
  mot_sap(0,6,120);
}

void hos_spokes_t::run()
{
  activate();
  while( !b_quit ){
    usleep(50000);
  }
  deactivate();
}


int main(int argc, char** argv)
{
  try{
    std::string dev("/dev/ttyACM0");
    if( argc > 1 )
      dev = argv[1];
    hos_spokes_t cnc(dev);
    cnc.run();
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
