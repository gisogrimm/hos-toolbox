#ifndef LIBHOS_RANDOM_H
#define LIBHOS_RANDOM_H

#include <map>
#include <iostream>

double drand();
double gauss(double x, double sigma);

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
  pmf_t& operator*=(const pmf_t& p2);
  friend std::ostream& operator<<(std::ostream& o, const pmf_t& p){
    o << "\n[--\n";
    for(pmf_t::const_iterator it=p.begin();it!=p.end();++it)
      o << " " << it->first << "   " << it->second << std::endl;
    o<< "--]\n";
    return o;
  };
private:
  std::map<double,double> icdf;
};

pmf_t operator*(double a,const pmf_t& p);

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
