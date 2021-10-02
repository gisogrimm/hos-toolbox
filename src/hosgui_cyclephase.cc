#include "hosgui_cyclephase.h"

using namespace HoSGUI;

#define OSC_ADDR "239.255.1.7"
#define OSC_PORT "6978"

#define HIST_SIZE 100

#include <iostream>

cycle_t::cycle_t(double x, double y, uint32_t channels, const std::string& name)
    : x_(x), y_(y), channels_(channels), name_(name), current(channels, 0.0)
{
  history.resize(HIST_SIZE);
  for(unsigned int k = 0; k < history.size(); k++)
    history[k].resize(channels);
  col_r.resize(channels);
  col_g.resize(channels);
  col_b.resize(channels);
  for(unsigned int k = 0; k < channels; k++) {
    col_r[k] = 0.5 + 0.5 * cos(k * M_PI * 2.0 / channels);
    col_g[k] = 0.5 + 0.5 * cos(k * M_PI * 2.0 / channels + 2.0 / 3.0 * M_PI);
    col_b[k] = 0.5 + 0.5 * cos(k * M_PI * 2.0 / channels + 4.0 / 3.0 * M_PI);
  }
}

void cycle_t::update_history(double phase)
{
  unsigned int k(phase * (HIST_SIZE - 1));
  k = std::min(k, (unsigned int)(HIST_SIZE - 1));
  for(unsigned int ch = 0; ch < channels_; ch++)
    history[k][ch] = current[ch];
}

void cycle_t::set_values(const std::vector<double>& v)
{
  if(v.size() == current.size())
    for(unsigned int k = 0; k < v.size(); k++)
      current[k] = v[k];
}

int cycle_t::osc_setval(const char* path, const char* types, lo_arg** argv,
                        int argc, lo_message msg, void* user_data)
{
  std::vector<double> vtmp;
  for(int k = 0; k < argc; k++)
    if(types[k] == 'f')
      vtmp.push_back(argv[k]->f);
  if(user_data)
    ((cycle_t*)user_data)->set_values(vtmp);
  return 0;
}

void cycle_t::draw(Cairo::RefPtr<Cairo::Context> cr, double phase)
{
  cr->save();
  cr->translate(-x_, -y_);
  cr->scale(0.43, 0.43);
  // cr->set_source_rgb( 0.24706, 0.18039, 0.203921 );
  // cr->set_line_width(1.0/20.0);
  // cr->begin_new_path();
  // cr->arc( 0, 0, 1, 0, 2*M_PI);
  // cr->stroke();
  double hscale(2 * M_PI / history.size());
  for(unsigned int ch = 0; ch < channels_; ch++) {
    double r(0.4 + 0.6 * ch / channels_);
    cr->set_source_rgb(col_r[ch], col_g[ch], col_b[ch]);
    for(unsigned int kh = 0; kh < history.size(); kh++) {
      cr->set_line_width(history[kh][ch] / (2 * channels_));
      cr->arc(0, 0, r, ((double)kh - 1.0) * hscale - 0.5 * M_PI,
              kh * hscale - 0.5 * M_PI);
      cr->stroke();
    }
  }
  cr->begin_new_path();
  double cx(sin(phase * M_PI * 2.0));
  double cy(-cos(phase * M_PI * 2.0));
  cr->set_source_rgb(0.905882, 0.117647, 0);
  cr->set_line_width(1.0 / 40.0);
  cr->move_to(cx, cy);
  cr->line_to(0, 0);
  cr->stroke();
  // cr->arc( 1.2*cx, 1.2*cy, 0.15, 0, 2*M_PI);
  // cr->fill();
  cr->restore();
}

int cyclephase_t::process(jack_nframes_t n, const std::vector<float*>& vIn,
                          const std::vector<float*>& vOut)
{
  phase = vIn[0][0];
  for(unsigned int kt = 0; kt < n; kt++)
    for(unsigned int k = 0; k < vCycle.size(); k++)
      vCycle[k]->update_history(vIn[0][kt]);
  return 0;
}

cyclephase_t::cyclephase_t(const std::string& name, uint32_t channels)
    : osc_server_t(OSC_ADDR, OSC_PORT), jackc_t("cyclephase"), phase(0),
      channels_(channels), name_(name)
{
  set_prefix("/" + name);
  Glib::signal_timeout().connect(
      sigc::mem_fun(*this, &cyclephase_t::on_timeout), 40);
#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  // Connect the signal handler if it isn't already a virtual method override:
  signal_expose_event().connect(
      sigc::mem_fun(*this, &cyclephase_t::on_expose_event), false);
#endif // GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  add_input_port("phase");
}

void cyclephase_t::activate()
{
  jackc_t::activate();
  osc_server_t::activate();
  try {
    connect_in(0, "phase:phase");
  }
  catch(const std::exception& e) {
    std::cerr << "Warning: " << e.what() << std::endl;
  };
}

void cyclephase_t::deactivate()
{
  osc_server_t::deactivate();
  jackc_t::deactivate();
}

cyclephase_t::~cyclephase_t()
{
  for(unsigned int k = 0; k < vCycle.size(); k++)
    delete vCycle[k];
}

void cyclephase_t::add_cycle(double x, double y, const std::string& name)
{
  std::string fmt(channels_, 'f');
  cycle_t* ctmp(new cycle_t(x, y, channels_, name));
  vCycle.push_back(ctmp);
  add_method("/" + name, fmt.c_str(), cycle_t::osc_setval, ctmp);
}

bool cyclephase_t::on_expose_event(GdkEventExpose* event)
{
  Glib::RefPtr<Gdk::Window> window = get_window();
  if(window) {
    Gtk::Allocation allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();
    // double ratio = (double)width/(double)height;
    Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();
    if(event) {
      // clip to the area indicated by the expose event so that we only
      // redraw the portion of the window that needs to be redrawn
      cr->rectangle(event->area.x, event->area.y, event->area.width,
                    event->area.height);
      cr->clip();
    }
    cr->translate(0.5 * width, 0.5 * height);
    double scale = std::min(width, height) / 3;
    cr->scale(scale, scale);
    cr->set_line_width(1.0 / 400.0);
    cr->set_source_rgb(0.7, 0.7, 0.7);
    cr->set_line_cap(Cairo::LINE_CAP_ROUND);
    cr->set_line_join(Cairo::LINE_JOIN_ROUND);
    // bg:
    cr->save();
    cr->set_source_rgb(1.0, 1.0, 1.0);
    cr->paint();
    cr->restore();
    // end bg
    for(unsigned int k = 0; k < vCycle.size(); k++)
      vCycle[k]->draw(cr, phase);
  }
  return true;
}

bool cyclephase_t::on_timeout()
{
  Glib::RefPtr<Gdk::Window> win = get_window();
  if(win) {
    Gdk::Rectangle r(0, 0, get_allocation().get_width(),
                     get_allocation().get_height());
    win->invalidate_rect(r, false);
  }
  return true;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
