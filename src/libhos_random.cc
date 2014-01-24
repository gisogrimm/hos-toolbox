#include "libhos_random.h"
#include <stdlib.h>


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
  for( std::map<double,double>::iterator it=pdf.begin();it!=pdf.end();++it)
    psum += it->second;
  for( std::map<double,double>::iterator it=pdf.begin();it!=pdf.end();++it)
    it->second /= psum;
  double p(0);
  for( std::map<double,double>::iterator it=pdf.begin();it!=pdf.end();++it){
    p+=it->second;
    icdf[p] = it->first;
  }
}

double pdf_t::rand()
{
  if( icdf.empty() )
    return 0;
  std::map<double,double>::iterator it(icdf.lower_bound(drand()));
  if( it == icdf.end() )
    return icdf.rbegin()->second;
  return it->second;
}

void pdf_t::add(double v,double p)
{
  pdf[v] = p;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
