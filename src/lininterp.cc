#include "lininterp.h"
#include <math.h>
#include <stdint.h>

interp_table_t::interp_table_t() : xmin(0), xmax(1), scale(1) {}

void interp_table_t::add(float vy)
{
  y.push_back(vy);
  set_range(xmin, xmax);
}

void interp_table_t::set_range(float x1, float x2)
{
  xmin = x1;
  xmax = x2;
  scale = (y.size() - 1) / (xmax - xmin);
}

float interp_table_t::operator()(float x)
{
  float xs((x - xmin) * scale);
  if(xs <= 0.0f)
    return y.front();
  uint32_t idx(floor(xs));
  if(idx >= y.size() - 1)
    return y.back();
  float dx(xs - (float)idx);
  return (1.0f - dx) * y[idx] + dx * y[idx + 1];
}

void colormap_t::clim(float vmin, float vmax)
{
  r.set_range(vmin, vmax);
  g.set_range(vmin, vmax);
  b.set_range(vmin, vmax);
}

colormap_t::colormap_t(int tp)
{
  switch(tp) {
  case 1: // rgb linear
    r.add(1.0);
    r.add(0.0);
    r.add(0.0);
    g.add(0.0);
    g.add(1.0);
    g.add(0.0);
    b.add(0.0);
    b.add(0.0);
    b.add(1.0);
    break;
  case 2: // gray
    r.add(0.0);
    r.add(1.0);
    g.add(0.0);
    g.add(1.0);
    b.add(0.0);
    b.add(1.0);
    break;
  default: // cos
    b.add(0.0);
    g.add(0.0);
    r.add(0.0);
    for(unsigned int k = 0; k < 64; k++) {
      float phi = k / 63.0 * M_PI - M_PI / 6.0;
      b.add(powf(cosf(phi), 2.0));
      g.add(powf(cosf(phi + M_PI / 3.0), 2.0));
      r.add(powf(cosf(phi + 2.0 * M_PI / 3.0), 2.0));
    }
  }
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
