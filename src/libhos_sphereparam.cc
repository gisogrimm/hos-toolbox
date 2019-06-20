/**
   \file libhos_sphereparam.cc
   \ingroup apphos
   \brief Cyclic trajectory parametrization

  Copyright (C) 2011 Giso Grimm

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; version 2
  of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
/**
\file libhos_sphereparam.cc
   \ingroup apphos
\brief Cyclic trajectory parametrization
*/

#include "libhos_sphereparam.h"
#include <math.h>
#include <iostream>

//#define OSC_ADDR "239.255.1.7"
#define OSC_ADDR "239.255.1.7"
#define OSC_PORT "6978"
#define OSC_FBPORT "6978"


using namespace HoS;


namespace OSC {

  void lo_err_handler_cb(int num, const char *msg, const char *where) 
  {
    std::cerr << "lo error " << num << ": " << msg << " (" << where << ")\n";
  }

  //int handler_f(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  //{
  //  if( (argc == 1) && (types[0] == 'f') ){
  //    *((float*)user_data) = argv[0]->f;
  //    return 0;
  //  }
  //  return 1;
  //}

  int handler_angle_f(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if( (argc == 1) && (types[0] == 'f') ){
      *((float*)user_data) = DEG2RAD*(argv[0]->f);
      return 0;
    }
    return 1;
  }

  int _feedbackaddr(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if( (argc == 1) && (types[0] == 's') ){
      ((HoS::parameter_t*)user_data)->set_feedback_osc_addr(&(argv[0]->s));
      return 0;
    }
    return 1;
  }

  int _sendphi(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    if( (argc == 1) && (types[0] == 's') ){
      ((HoS::parameter_t*)user_data)->send_phi(&(argv[0]->s));
      return 0;
    }
    return 1;
  }

  int _locate(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    //DEBUG(1);
    if( (argc == 1) && (types[0] == 'f') ){
      ((HoS::parameter_t*)user_data)->locate0(argv[0]->f);
      return 0;
    }
    return 1;
  }

  int _stopat(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    //DEBUG(1);
    if( (argc == 1) && (types[0] == 'f') ){
      ((HoS::parameter_t*)user_data)->set_stopat(DEG2RAD*(argv[0]->f));
      return 0;
    }
    return 1;
  }

  int _applyat(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    //DEBUG(1);
    if( (argc == 2) && (types[0] == 'f') && (types[1] == 'f') ){
      ((HoS::parameter_t*)user_data)->set_applyat(DEG2RAD*(argv[0]->f),argv[1]->f);
      return 0;
    }
    return 1;
  }

  int _setelev(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    //DEBUG(1);
    if( (argc == 1) && (types[0] == 'f') ){
      ((HoS::parameter_t*)user_data)->setelev(argv[0]->f);
      return 0;
    }
    return 1;
  }

  int _apply(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    //DEBUG(1);
    if( (argc == 1) && (types[0] == 'f') ){
      ((HoS::parameter_t*)user_data)->apply(argv[0]->f);
      return 0;
    }
    return 1;
  }

  int _az(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
  {
    //DEBUG(1);
    if( (argc == 1) && (types[0] == 'f') ){
      ((HoS::parameter_t*)user_data)->az(argv[0]->f);
      return 0;
    }
    return 1;
  }

}

  
void par_t::mix_static(float g,const par_t& p1,const par_t& p2){
  float g1 = 1.0f-g;
  float re, im;
  re = g*cos(p1.phi0)+g1*cos(p2.phi0);
  im = g*sin(p1.phi0)+g1*sin(p2.phi0);
  phi0 = atan2f(im,re);
  re = g*cos(p1.phi0_epi)+g1*cos(p2.phi0_epi);
  im = g*sin(p1.phi0_epi)+g1*sin(p2.phi0_epi);
  phi0_epi = atan2f(im,re);
}

void par_t::assign_static(const par_t& p1){
#define MIX_(x) x = p1.x;
  MIX_(phi0);       
  MIX_(phi0_epi);       
#undef MIX_
}

void par_t::mix_dynamic(float g,const par_t& p1,const par_t& p2){
  float g1 = 1.0f-g;
#define MIX_(x) x = g*p1.x + g1*p2.x;
  MIX_(random);       
  MIX_(f);       
  MIX_(r);       
  MIX_(width);   
  MIX_(rho_maxw);
  MIX_(theta);   
  MIX_(e);       
  MIX_(f_epi);   
  MIX_(r_epi);   
  MIX_(onset);   
  MIX_(g_in);    
  MIX_(g_rvb);   
#undef MIX_
}

void par_t::assign_dynamic(const par_t& p1){
#define MIX_(x) x = p1.x;
  MIX_(random);       
  MIX_(f);       
  MIX_(r);       
  MIX_(width);   
  MIX_(rho_maxw);
  MIX_(theta);   
  MIX_(e);       
  MIX_(f_epi);   
  MIX_(r_epi);   
  MIX_(onset);   
  MIX_(g_in);    
  MIX_(g_rvb);   
#undef MIX_
}

par_t::par_t()
  : phi0(0),      
    elev(0),
    rvbelev(0),
    random(0),
    f(0),       
    r(1.0),       
    width(18.0),   
    rho_maxw(5.0),
    theta(0),   
    e(0),       
    f_epi(0),   
    r_epi(0),   
    phi0_epi(0),   
    onset(0),   
    randpos(0),
    g_in(1.0),    
    g_rvb(1.0),
    map_octave(0),
    map_f(0)
{
}

void parameter_t::az(float az_)
{
  par_osc.phi0 = DEG2RAD*az_;
  locate0(0);
}

void parameter_t::locate0(float time)
{
  //DEBUG(time);
  time *= f_update;
  t_locate = 0;
  if( time > 0 )
    locate_gain = 1.0f/time;
  else{
    locate_gain = 0;
  }
  t_locate = 1+time;
}

void parameter_t::setelev(float time)
{
  //DEBUG(time);
  time *= f_update;
  t_elev = 0;
  if( time > 0 )
    elev_gain = 1.0f/time;
  else{
    elev_gain = 0;
  }
  t_elev = 1+time;
}

void parameter_t::apply(float time)
{
  //DEBUG(time);
  time *= f_update;
  t_apply = 0;
  if( time > 0 )
    apply_gain = 1.0f/time;
  else{
    apply_gain = 0;
  }
  t_apply = 1+time;
  b_stopat = false;
  b_applyat = false;
}

parameter_t::parameter_t(const std::string& name)
  : TASCAR::osc_server_t(OSC_ADDR,OSC_PORT,"UDP"),
    stopat(0),
    b_stopat(false),
    applyat(0),
    applyat_time(0),
    b_applyat(false),
    lo_addr(lo_address_new( OSC_ADDR, OSC_FBPORT )),
    b_quit(false),
    f_update(1),
    t_locate(0),
    t_elev(0),
    t_apply(0),
    lastphi(0),
    osc_prefix(std::string("/")+name+std::string("/"))
{
  lo_address_set_ttl( lo_addr, 1 );
  //set_preset();
  std::string s;
  //lost = lo_server_thread_new_multicast(OSC_ADDR,OSC_PORT,OSC::lo_err_handler_cb);
  set_prefix(osc_prefix);
  add_bool_true("quit",&b_quit);
//#define REGISTER_FLOAT_VAR(x) s = osc_prefix+#x;lo_server_thread_add_method(lost,s.c_str(),"f",OSC::handler_f,&(par_osc.x))
//#define REGISTER_FLOAT_VAR_DEGREE(x) s = osc_prefix+#x;lo_server_thread_add_method(lost,s.c_str(),"f",OSC::handler_angle_f,&(par_osc.x))
//#define REGISTER_CALLBACK(x,fmt) s=osc_prefix+#x;lo_server_thread_add_method(lost,s.c_str(),fmt,OSC::_ ## x,this)
#define REGISTER_FLOAT_VAR(x) add_float(#x,&(par_osc.x))
#define REGISTER_FLOAT_VAR_DEGREE(x) add_method(#x,"f",OSC::handler_angle_f,&(par_osc.x))
#define REGISTER_CALLBACK(x,fmt) add_method(#x,fmt,OSC::_ ## x,this)
  REGISTER_FLOAT_VAR_DEGREE(phi0);
  REGISTER_FLOAT_VAR_DEGREE(elev);
  REGISTER_FLOAT_VAR_DEGREE(rvbelev);
  REGISTER_FLOAT_VAR(random);
  REGISTER_FLOAT_VAR(f);
  REGISTER_FLOAT_VAR(r);
  REGISTER_FLOAT_VAR_DEGREE(width);
  REGISTER_FLOAT_VAR(rho_maxw);
  REGISTER_FLOAT_VAR_DEGREE(theta);
  REGISTER_FLOAT_VAR(e);
  REGISTER_FLOAT_VAR(f_epi);
  REGISTER_FLOAT_VAR(r_epi);
  REGISTER_FLOAT_VAR_DEGREE(phi0_epi);
  REGISTER_FLOAT_VAR(onset);
  REGISTER_FLOAT_VAR(randpos);
  REGISTER_FLOAT_VAR(g_in);
  REGISTER_FLOAT_VAR(g_rvb);
  REGISTER_FLOAT_VAR(map_octave);
  REGISTER_FLOAT_VAR(map_f);
  //REGISTER_FLOAT_VAR(locate);
  REGISTER_CALLBACK(feedbackaddr,"s");
  REGISTER_CALLBACK(sendphi,"s");
  REGISTER_CALLBACK(locate,"f");
  REGISTER_CALLBACK(setelev,"f");
  REGISTER_CALLBACK(apply,"f");
  REGISTER_CALLBACK(stopat,"f");
  REGISTER_CALLBACK(applyat,"ff");
  REGISTER_CALLBACK(az,"f");
#undef REGISTER_FLOAT_VAR
#undef REGISTER_CALLBACK
  //lo_server_thread_start(lost);
  activate();
  //set_feedback_osc_addr( OSC_ADDR );
}

parameter_t::~parameter_t()
{
  //lo_server_thread_stop(lost);
  //lo_server_thread_free(lost);
}


/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
