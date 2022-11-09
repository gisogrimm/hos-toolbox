#ifndef HOSDEFS_H
#define HOSDEFS_H

/**
   \file hosdefs.h
   \brief Some basic definitions
*/

/**
    \brief average radius of earth in meters:
*/
#define R_EARTH 6367467.5

#define DEBUG(x)                                                               \
  std::cerr << __FILE__ << ":" << __LINE__ << ": " << __PRETTY_FUNCTION__      \
            << " " << #x << "=" << x << std::endl
//#define DEBUGMSG(x) std::cerr << __FILE__ << ":" << __LINE__ << ": " << x <<
// std::endl
#define DEBUGMSG(x)                                                            \
  std::cerr << __FILE__ << ":" << __LINE__ << ": " << __PRETTY_FUNCTION__      \
            << " --" << x << "--" << std::endl

/**
   \defgroup libtascar TASCAR core library

   This library is published under the LGPL.
*/
/**
   \defgroup apptascar TASCAR applications

   This set of applications is published under the GPL.
*/
/**
   \defgroup apphos Applications specific to the Harmony of the Spheres project

   This set of applications is published under the GPL and free licenses.
*/

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
