/**
   \file libhos_random.h
   \ingroup rtm
 */

#ifndef LIBHOS_RANDOM_H
#define LIBHOS_RANDOM_H

#include <map>
#include <iostream>

/**
   \brief Return randum number between 0 (included) and 1 (excluded)
   \ingroup rtm
 */
double drand();

/**
   \brief Gauss function with mean=0 and variance sigma
   \ingroup rtm
   \param x input value
   \param sigma variance
 */
double gauss(double x, double sigma);

/**
   \brief Probability mass function
   \ingroup rtm
 */
class pmf_t : public std::map<double,double> {
public:
  pmf_t();
  void update();
  double rand() const;
  void set(double v,double p);
  pmf_t operator+(const pmf_t& p2) const;
  pmf_t vadd(double dp) const;
  pmf_t vscale(double dp) const;
  pmf_t vthreshold(double dp) const;
  pmf_t operator*(const pmf_t& p2) const;
  pmf_t operator*(double a) const;
  double vmin() const;
  double vmax() const;
  double vpmax() const;
  double pmax() const;
  bool icdfempty() const { return icdf.empty();};
  pmf_t& operator*=(const pmf_t& p2);
  friend std::ostream& operator<<(std::ostream& o, const pmf_t& p){
    o << "\n[--\n";
    for(pmf_t::const_iterator it=p.begin();it!=p.end();++it)
      o << " " << it->first << "   " << it->second << std::endl;
    o<< "--]\n";
    return o;
  };
private:
  std::map<double,double> icdf;//< Cumulative distribution function
};

pmf_t operator*(double a,const pmf_t& p);

/**
   \ingroup rtm
 */
pmf_t gauss(double x,double sigma,double xmin,double xmax,double xstep);


#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
