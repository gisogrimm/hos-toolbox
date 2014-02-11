#ifndef LIBHOS_RANDOM_H
#define LIBHOS_RANDOM_H

#include <map>

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
  pmf_t operator*(const pmf_t& p2) const;
  pmf_t operator*(double a) const;
  double vmin() const;
  double vmax() const;
private:
  std::map<double,double> icdf;
};

pmf_t operator*(double a,const pmf_t& p);


#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
