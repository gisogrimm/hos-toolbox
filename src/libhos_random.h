#ifndef LIBHOS_RANDOM_H
#define LIBHOS_RANDOM_H

#include <map>

double drand();
double gauss(double x, double sigma);

class pdf_t : public std::map<double,double> {
public:
  pdf_t();
  void update();
  double rand() const;
  void set(double v,double p);
  pdf_t operator+(const pdf_t& p2) const;
  pdf_t vadd(double dp) const;
  pdf_t operator*(const pdf_t& p2) const;
  pdf_t operator*(double a) const;
  double vmin() const;
  double vmax() const;
private:
  std::map<double,double> icdf;
};

pdf_t operator*(double a,const pdf_t& p);


#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
