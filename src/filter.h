/**
   \file filter.h
   \brief Filter classes and functions

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
*/
#ifndef HOSFILTER_H
#define HOSFILTER_H

#include <math.h>
#include "defs.h"

namespace HoS {

  /**
     \brief First order attack-release filter
  */
  class arflt {
  public:
    /**
       \param ta attack time constant
       \param tr release time constant
       \param fs sampling rate
    */
    arflt(double ta, double tr, double fs) : z(0) 
    {
      tau2c(ta,fs,c1a,c2a);
      tau2c(tr,fs,c1r,c2r);
    };
    /**
       \brief Apply filter
       \param x input value
    */
    inline float filter(float x)
    {
      if( x >= z )
        z = c1a * z + c2a * x;
      else
        z = c1r * z + c2r * x;
      return z;
    };
  private:
    inline void tau2c( double tau, double fs, double &c1, double &c2)
    {
      if( tau > 0.0)
        c1 = exp( -1.0/(tau * fs) );
      else
        c1 = 0;
      c2 = 1.0-c1;
    };
    double z;
    double c1a, c1r, c2a, c2r;
  };


  /**
     \brief Very simple onset-detector
  */
  class onset_detector {
  public:
    // smooth: 0.02 0.02
    onset_detector( double fs ) : env_peak( 0.0, 0.005, fs ), env_smooth( 0.4, 1.5, fs), mintrack( 5.0, 0.0, fs ), crit_smooth( 0.05, 0.5, fs ){};
    inline float detect( float x )
    {
      float env = env_smooth.filter(std::max(EPSf,env_peak.filter( std::max(EPSf,fabsf(x)) ) ) );
      return env;
    };
  private:
    arflt env_peak;
    arflt env_smooth;
    arflt mintrack;
    arflt crit_smooth;
  };

}

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */

