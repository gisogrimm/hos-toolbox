/**
   \file hos_sphere_amb30.cc
   \ingroup apphos
   \brief Create virtual sources on cyclic trajectories in 3rd order horizontal ambisonics format

   \author Giso Grimm
   \date 2011

   \section license License (GPL)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

*/

#include <iostream>
#include <stdlib.h>
#include <tascar/jackclient.h>
#include "filter.h"
#include "ambpan.h"
#include "libhos_sphereparam.h"
#include <unistd.h>
using namespace HoS;

namespace HoS {

  /**
     \ingroup apphos
  */
  class sphere_amb30_t : public jackc_t, private HoS::parameter_t {
  public:
    sphere_amb30_t(const std::string& name);
    void run();
  private:
    void update_trajectory();
    int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer);
    // osc address for feedback:
    std::string pos_addr;
    // audio sample frequency:
    double f_sample;
    // timing control for parameter update:
    unsigned int dt_update;
    unsigned int t_update;
    // timing control for position feedback:
    unsigned int dt_feedback;
    unsigned int t_feedback;
    // parameter update frequency:
    double f_update;
    // Ambisonics panner:
    amb_coeff amb_dry, amb_wet_left, amb_wet_right;
    // control parameter for trajectory:
    float phi;
    float phi_epi;
    // onset detector (for Paert):
    onset_detector ons;
    float ons_crit;
    float t_randpos;
  };

}

sphere_amb30_t::sphere_amb30_t(const std::string& name)
  : jackc_t(name),
    parameter_t(name),
    pos_addr("/pos/"+name),
    f_sample(srate),
    dt_update(f_sample/25.0),
    t_update(0),
    dt_feedback(f_sample/25.0),
    t_feedback(0),
    f_update(f_sample/(double)dt_update),
    amb_dry(dt_update),
    amb_wet_left(dt_update),
    amb_wet_right(dt_update),
    phi(0.0f),
    phi_epi(0.0f),
    ons(f_sample),
    t_randpos(0)
{
  for(unsigned int k=0;k<name.size();k++)
    srand(name[k]);
  set_par_fupdate(f_update);
  add_input_port("direct");
  add_input_port("reverb_l");
  add_input_port("reverb_r");
  add_output_port("reverb");
  add_output_port("0.W");
  add_output_port("1.X");
  add_output_port("1.Y");
  add_output_port("2.U");
  add_output_port("2.V");
  add_output_port("3.P");
  add_output_port("3.Q");
  //DEBUG(1);
}

double phidiff( double p1, double p2 )
{
  return atan2(sin(p1)*cos(p2) - cos(p1)*sin(p2),
               cos(p1)*cos(p2) + sin(p1)*sin(p2));
}

bool iscrossing( double phi0, double phi, double lastphi )
{
  double pd1(phidiff(phi0,phi));
  double pd2(phidiff(phi0,lastphi));
  return ( ( (pd1 >= 0) && (pd2 < 0) ) ||
           ( (pd1 < 0) && (pd2 >= 0) ) ) && (fabs(pd1)<0.5*M_PI) && (fabs(pd2)<0.5*M_PI);
}

void sphere_amb30_t::update_trajectory()
{
  // optionally apply dynamic parameters:
  if( t_apply ){
    t_apply--;
    float g = t_apply*apply_gain;
    par_current.mix_dynamic( g, par_previous, par_osc);
  }else{
    par_previous.assign_dynamic( par_current );
  }
  if( (par_osc.randpos > ons_crit) && (t_randpos <= 0) ){
    phi = rand()*PI2/RAND_MAX;
    t_randpos = 1.0*f_sample;
  }else{
    if( t_randpos )
      t_randpos -= dt_update;
    // overide phi by onset detector:
    phi = (1.0-par_current.onset)*phi + par_current.onset*(par_osc.phi0 + par_current.f * ons_crit);
  }
  // optionally reset static parameters:
  if( t_locate ){
    t_locate--;
    float g = t_locate*locate_gain;
    par_current.mix_static( g, par_previous, par_osc );
    phi = par_current.phi0;
    phi_epi = par_current.phi0_epi;
  }else{
    par_previous.assign_static( par_current );
    par_current.phi0 = phi;
    par_current.phi0_epi = phi_epi;
  }
  // apply elevation changes:
  if( t_elev ){
    t_elev--;
    float g = t_elev*elev_gain;
    par_current.elev = g*par_previous.elev + (1.0f-g)*par_osc.elev;
    par_current.rvbelev = g*par_previous.rvbelev + (1.0f-g)*par_osc.rvbelev;
  }else{
    par_previous.elev = par_current.elev;
    par_previous.rvbelev = par_current.rvbelev;
  }
  if( par_osc.map_octave ){
    //DEBUG(par_osc.map_f);
    if( (par_osc.map_f > 50) && (par_osc.map_f < 5000) ){
      // pitch detected:
      par_current.elev = 0;
      par_current.rvbelev = 0;
      phi = PI2*log2f( par_osc.map_f );
    }else{
      // no pitch detected, scale to elev=90
      par_current.elev = 0.5*M_PI;
      par_current.rvbelev = 0.5*M_PI;
      phi = 0;
    }
  }
  //  // ellipse parameters:
  par_current.e = std::min(par_current.e,0.9999f);
  e2 = par_current.e * par_current.e;
  r2 = par_current.r / sqrt(1.0 - e2);
  // normalized angular velocity:
  w_main = par_current.f * PI2 * par_current.r*par_current.r / f_update;
  w_epi = par_current.f_epi * PI2 / f_update;
  // random component:
  double r;
  r = (2.0*rand()/RAND_MAX)-0.7;
  r *= r*r;
  r *= par_current.random;
  w_main += r;
  r = (2.0*rand()/RAND_MAX)-0.7;
  r *= r*r;
  r *= par_current.random;
  w_epi += r;
  // panning parameters:
  c_rho = 1.0f/par_current.rho_maxw;
  // get ellipse in polar coordinates:
  float rho = r2*(1.0f - e2)/(1.0f - par_current.e*cosf(phi-par_current.theta));
  // add epicycles:
  float x = rho*cosf(phi) + par_current.r_epi*cosf(phi_epi);
  float y = rho*sinf(phi) + par_current.r_epi*sinf(phi_epi);
  // calc resulting polar coordinates:
  _az = atan2f(y,x);
  _rho = sqrtf(x*x+y*y);
  // increment phi and phi_epi:
  phi += w_main/std::max(rho*rho,EPSf);
  while( phi > M_PI )
    phi -= 2.0*M_PI;
  while( phi < -M_PI )
    phi += 2.0*M_PI;
  //phi_epi += ons_crit*w_epi;
  phi_epi += w_epi;
  while( phi_epi > M_PI )
    phi_epi -= 2.0*M_PI;
  while( phi_epi < -M_PI )
    phi_epi += 2.0*M_PI;
  if( b_stopat && iscrossing( stopat, phi, lastphi ) ){
    par_current.f = 0;
    par_osc.f = 0;
    par_previous.f = 0;
    phi = stopat;
    b_stopat = false;
  }
  if( b_applyat && iscrossing( applyat, phi, lastphi ) ){
    par_current.f = 0;
    par_osc.f = 0;
    par_previous.f = 0;
    phi = applyat;
    b_applyat = false;
    apply(applyat_time);
  }
  lastphi = phi;
  float normd(_rho * c_rho);
  normd *= normd;
  float _width(par_current.width * 1.0f*normd/(normd*normd + 1.0f));
  float G(1.0f/(1.0f+normd));
  // apply panning:
  amb_dry.set( _az, par_current.elev, G );
  amb_wet_left.set(_az+_width, par_current.rvbelev, 1.0f-G );
  amb_wet_right.set(_az-_width, par_current.rvbelev, 1.0f-G );
}

int sphere_amb30_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer)
{
  //set_preset();
  float* vRvb(outBuffer[0]);
  float* vW(outBuffer[1]);
  float* vX(outBuffer[2]);
  float* vY(outBuffer[3]);
  float* vU(outBuffer[4]);
  float* vV(outBuffer[5]);
  float* vP(outBuffer[6]);
  float* vQ(outBuffer[7]);
  // main loop:
  for (jack_nframes_t i = 0; i < nframes; ++i){
    // well, this is not really real-time safe...
    // we need asynchronous parameter update and feedback sending.
    if( !t_update ){
      t_update = dt_update;
      update_trajectory();
    }
    t_update--;
    if( !t_feedback ){
      t_feedback = dt_feedback;
      lo_send( lo_addr, pos_addr.c_str(),"ff",_az,_rho*cos(par_current.elev));
    }
    t_feedback--;
    float dry = par_current.g_in * inBuffer[0][i];
    vRvb[i] = par_current.g_rvb * dry;
    float wet_left = inBuffer[1][i];
    float wet_right = inBuffer[2][i];
    ons_crit = ons.detect( dry );
    // calculate panning:
    amb_dry.clear(vW[i],vX[i],vY[i],vU[i],vV[i],vP[i],vQ[i]);
    amb_dry.addpan30(dry,vW[i],vX[i],vY[i],vU[i],vV[i],vP[i],vQ[i]);
    amb_wet_left.addpan30(wet_left,vW[i],vX[i],vY[i],vU[i],vV[i],vP[i],vQ[i]);
    amb_wet_right.addpan30(wet_right,vW[i],vX[i],vY[i],vU[i],vV[i],vP[i],vQ[i]);
  }
  return 0;
}

void sphere_amb30_t::run()
{
  jackc_t::activate();
  while( !b_quit ){
    sleep( 1 );
  }
  jackc_t::deactivate();
}

int main(int argc, char** argv)
{
  std::string name("sphere");
  if( argc > 1 )
    name = argv[1];
  sphere_amb30_t S(name);
  S.run();
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
