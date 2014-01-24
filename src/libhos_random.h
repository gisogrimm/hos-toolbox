#ifndef LIBHOS_RANDOM_H
#define LIBHOS_RANDOM_H

#include <map>

double drand();

class pdf_t {
public:
  pdf_t();
  void update();
  double rand();
  void add(double v,double p);
private:
  std::map<double,double> pdf;
  std::map<double,double> icdf;
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
