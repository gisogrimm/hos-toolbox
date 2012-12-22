/*

  "sphere" is a program to create dynamically moved virtual sources in
  3rd order horizontal ambisonics format.

  Copyright (C) 2011 Giso Grimm

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
/**
\file libhos_sphereparam.h
   \ingroup apphos
\brief Cyclic trajectory parametrization
*/
#ifndef SPHERE_PARAM_H
#define SPHERE_PARAM_H

#include <string>
#include <lo/lo.h>
#include "defs.h"

/**
\brief Classes mainly used for artistic purposes

Members are mostly dirty hacks and undocumented...
 */
namespace HoS {

  class par_t {
  public:
    par_t();
    void mix_static(float g,const par_t& p1,const par_t& p2);
    void assign_static(const par_t& p1);
    void mix_dynamic(float g,const par_t& p1,const par_t& p2);
    void assign_dynamic(const par_t& p1);
    float phi0;     // starting phase
    float elev;
    float rvbelev;
    float random;   // random component
    float f;        // main rotation frequency
    float r;        // main radius
    float width;    // stereo width in deg
    float rho_maxw; // rho of maximum stereo width
    float theta;    // ellipse main axis rotation
    float e;        // excentricity
    float f_epi;    // epicycle frequency
    float r_epi;    // epicycle radius
    float phi0_epi; // starting phase epicycle
    float onset;    // follow onset?
    float randpos;  // random position during silence?
    float g_in;     // gain for input signal
    float g_rvb;    // gain for reverb signal
    float map_octave;// flag: map one octave to a circle
    float map_f;    // frequency in Hz
  };

  class parameter_t {
  public:
    parameter_t(const std::string& name);
    ~parameter_t();
    void set_quit() { b_quit=true; };
    //void set_preset();
    void locate0( float time );
    void setelev( float time );
    void apply( float time );
    void set_stopat( float sa ){
      stopat = sa;
      b_stopat = true;
    };
    void set_applyat( float sa, float t ){
      applyat = sa;
      applyat_time = t;
      b_applyat = true;
    };
    void set_feedback_osc_addr(const char* s){
      lo_addr = lo_address_new( s, "6977" );
      lo_address_set_ttl( lo_addr, 1 );
    }
    void set_par_fupdate(float f_update_){f_update=f_update_;};
    void send_phi(const char* addr){
      lo_send( lo_addr, addr, "f", _az * RAD2DEG );
    }
  protected:
    HoS::par_t par_osc;
    HoS::par_t par_current;
    HoS::par_t par_previous;
    float stopat;
    bool b_stopat;
    float applyat;
    float applyat_time;
    bool b_applyat;
    // ellipse parameters:
    float e2;
    float r2;
    // normalized angular velocity:
    double w_main;
    double w_epi;
    // panning parameters:
    float c_rho;
    lo_address lo_addr;
    bool b_quit;
    float f_update;
    unsigned int t_locate; // slowly locate to position
    float locate_gain;
    unsigned int t_elev; // slowly locate to elevation
    float elev_gain;
    unsigned int t_apply; // slowly apply parameters
    float apply_gain;
    // control parameter for panner:
    float _az;
    float _rho;
    float lastphi;
  private:
    //int osc_preset;
    lo_server_thread lost;
    std::string osc_prefix;
  };

};

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
