#include "libhos_random.h"
#include "hos_defs.h"
#include <math.h>
#include <stdlib.h>
#include <tascar/defs.h>
#include <tascar/errorhandling.h>

double drand()
{
  return (double)random() / (double)(RAND_MAX + 1.0);
}

pmf_t::pmf_t() {}

/**
   \brief re-normalize the pmf data and re-calculate the cumulative
   distribution function
 */
void pmf_t::update()
{
  icdf.clear();
  double psum(0);
  for(iterator it = begin(); it != end(); ++it)
    psum += it->second;
  for(iterator it = begin(); it != end(); ++it)
    it->second /= psum;
  double p(0);
  for(iterator it = begin(); it != end(); ++it) {
    if(fabs(it->second) > EPS) {
      p += it->second;
      icdf[p] = it->first;
    }
  }
}

/**
   \brief Return a random number based on the PMF

   After changes of the probability map, the update function needs to
   be called, otherwise the return values are undefind.

   \return random number
 */
double pmf_t::rand() const
{
  if(icdf.empty())
    throw TASCAR::ErrMsg("The cumulative distribution function is empty.");
  std::map<double, double>::const_iterator it(icdf.lower_bound(drand()));
  if(it == icdf.end())
    return icdf.rbegin()->second;
  return it->second;
}

void pmf_t::set(double v, double p)
{
  operator[](v) = p;
}

pmf_t pmf_t::operator+(const pmf_t& p2) const
{
  pmf_t retv;
  for(const_iterator it = begin(); it != end(); ++it)
    retv[it->first] = it->second * size();
  for(const_iterator it = p2.begin(); it != p2.end(); ++it)
    retv[it->first] += it->second * p2.size();
  retv.update();
  return retv;
}

pmf_t pmf_t::vadd(double dp) const
{
  pmf_t retv;
  for(const_iterator it = begin(); it != end(); ++it)
    retv[it->first + dp] = it->second;
  retv.update();
  return retv;
}

pmf_t pmf_t::vthreshold(double dp) const
{
  pmf_t retv;
  for(const_iterator it = begin(); it != end(); ++it)
    if(it->first >= dp)
      retv[it->first] = it->second;
  retv.update();
  return retv;
}

pmf_t pmf_t::vscale(double dp) const
{
  pmf_t retv;
  for(const_iterator it = begin(); it != end(); ++it)
    retv[it->first * dp] = it->second;
  retv.update();
  return retv;
}

pmf_t pmf_t::operator*(const pmf_t& p2) const
{
  pmf_t retv;
  for(const_iterator it = begin(); it != end(); ++it) {
    const_iterator it2(p2.find(it->first));
    if(it2 != p2.end())
      retv[it->first] = it->second * it2->second;
  }
  retv.update();
  return retv;
}

pmf_t& pmf_t::operator*=(const pmf_t& p2)
{
  *this = operator*(p2);
  return *this;
}

pmf_t pmf_t::operator*(double a) const
{
  pmf_t retv;
  for(const_iterator it = begin(); it != end(); ++it)
    retv[it->first] = it->second * a;
  return retv;
}

pmf_t operator*(double a, const pmf_t& p)
{
  return p * a;
}

double pmf_t::vmin() const
{
  if(!empty())
    return begin()->first;
  return 0;
}

double pmf_t::vmax() const
{
  if(!empty())
    return rbegin()->first;
  return 0;
}

double pmf_t::vpmax() const
{
  if(empty())
    return 0;
  double vm(begin()->first);
  double pm(begin()->second);
  for(const_iterator v = begin(); v != end(); ++v) {
    if(v->second > pm) {
      vm = v->first;
      pm = v->second;
    }
  }
  return vm;
}

double pmf_t::pmax() const
{
  if(empty())
    return 0;
  double pm(begin()->second);
  for(const_iterator v = begin(); v != end(); ++v) {
    if(v->second > pm) {
      pm = v->second;
    }
  }
  return pm;
}

double gauss(double x, double sigma)
{
  return 1.0 / sqrt(2.0 * M_PI * sigma * sigma) *
         exp(-(x * x) / (2 * sigma * sigma));
}

pmf_t gauss(double x, double sigma, double xmin, double xmax, double xstep)
{
  pmf_t p;
  for(double v = xmin; v <= xmax; v += xstep)
    p.set(v, gauss(v - x, sigma));
  p.update();
  return p;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
