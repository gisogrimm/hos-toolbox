#ifndef LININTERP_H
#define LININTERP_H

#include <vector>

class interp_table_t
{
public:
  interp_table_t();
  void set_range(float x1,float x2);
  float operator()(float x);
  void add(float y);
private:
  std::vector<float> y;
  float xmin;
  float xmax;
  float scale;
};

class colormap_t {
public:
  colormap_t(int tp);
  void clim(float vmin,float vmax);
  interp_table_t r;
  interp_table_t b;
  interp_table_t g;
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
