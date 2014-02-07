#include "libhos_random.h"
#include <stdlib.h>
#include <math.h>

double drand()
{
  return (double)random()/(double)(RAND_MAX+1.0);
}

pdf_t::pdf_t()
{
}

void pdf_t::update()
{
  icdf.clear();
  double psum(0);
  for( iterator it=begin();it!=end();++it)
    psum += it->second;
  for( iterator it=begin();it!=end();++it)
    it->second /= psum;
  double p(0);
  for( iterator it=begin();it!=end();++it){
    if( it->second != 0 ){
      p+=it->second;
      icdf[p] = it->first;
    }
  }
}

double pdf_t::rand() const
{
  if( icdf.empty() )
    return 0;
  std::map<double,double>::const_iterator it(icdf.lower_bound(drand()));
  if( it == icdf.end() )
    return icdf.rbegin()->second;
  return it->second;
}

void pdf_t::set(double v,double p)
{
  operator[](v) = p;
}

pdf_t pdf_t::operator+(const pdf_t& p2) const
{
  pdf_t retv;
  for(const_iterator it=begin();it!=end();++it)
    retv[it->first] = it->second;
  for(const_iterator it=p2.begin();it!=p2.end();++it)
    retv[it->first] += it->second;
  retv.update();
  return retv;
}

pdf_t pdf_t::vadd(double dp) const
{
  pdf_t retv;
  for(const_iterator it=begin();it!=end();++it)
    retv[it->first+dp] = it->second;
  retv.update();
  return retv;
}

pdf_t pdf_t::operator*(const pdf_t& p2) const
{
  pdf_t retv;
  for(const_iterator it=begin();it!=end();++it){
    const_iterator it2(p2.find(it->first));
    if( it2 != p2.end() )
      retv[it->first] = it->second * it2->second;
  }
  retv.update();
  return retv;
}

pdf_t pdf_t::operator*(double a) const
{
  pdf_t retv;
  for(const_iterator it=begin();it!=end();++it)
    retv[it->first] = it->second * a;
  return retv;
}

pdf_t operator*(double a,const pdf_t& p)
{ 
  return p*a;
}

double pdf_t::vmin() const
{
  if( !empty() )
    return begin()->first;
  return 0;
}

double pdf_t::vmax() const
{
  if( !empty() )
    return rbegin()->first;
  return 0;
}

double gauss(double x, double sigma )
{
  return 1.0/sqrt(2.0*M_PI*sigma*sigma)*exp(-(x*x)/(2*sigma*sigma));
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
