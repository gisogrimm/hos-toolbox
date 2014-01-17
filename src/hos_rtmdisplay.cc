#include <gtkmm.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/drawingarea.h>
#include <cairomm/context.h>
#include <lo/lo.h>
#include <iostream>

class clef_t {
public:
  std::string symbol;
  std::string smallsymbol;
  int position;
  int pitch;
  void draw_full(Cairo::RefPtr<Cairo::Context> cr,double x, double y);
};

void clef_t::draw_full(Cairo::RefPtr<Cairo::Context> cr,double x, double y)
{
  cr->move_to(x, -(y+position) );
  cr->show_text(symbol);
}

static clef_t alto = { "", "",0,0};
static clef_t tenor = { "", "",2,0};
static clef_t bass = { "", "",2,-7};
static clef_t treble = {"","",-2,7};
const char* noteheads = "";

int major[7] = {2,2,1,2,2,2,1};

class graphical_pitch_t {
public:
  graphical_pitch_t(int pitch);
  int y;
  int alteration;
  friend std::ostream& operator<<(std::ostream& o, const graphical_pitch_t& p) { o << "y=" << p.y << " ("<<p.alteration << ")"; return o;};
};

graphical_pitch_t::graphical_pitch_t(int pitch)
{
  int octave(floor((double)pitch/12.0));
  int p_c = pitch-12*octave;
  y = floor(7.0/12.0*p_c)+(p_c==5);
  int nonaltered_pitch = floor((y+0.5)*12.0/7.0)-(y==3);
  alteration = p_c - nonaltered_pitch;
  y += 7*octave;
}

// 0   1   2   3   4   5   6   7   8   9  10  11  12  
// c       d       e   f       g       a       h   c

// graphical
//    0   1   2   3   4   5   6   7
// 0: c   d   e   f   g   a   b   c
// 1: c   d   e   fis g   a   b   c
// 2: cis d   e   fis g   a   b   cis
// 4: cis d   e   fis gis a   b   cis
//-1: c   d   e   f   g   a   bes c
//-2: c   d   ees f   g   a   bes c
//-3: c   d   ees f   g   aes bes c

// zeige die enharmonische verwechslung, die in der aktuellen tonart weniger visuelle vorzeichen hat

class staff_t {
public:
  staff_t();
  void set_coords(double width,double y);
  clef_t clef;
  int key;
  void draw(Cairo::RefPtr<Cairo::Context> cr);
  void draw_keychange(Cairo::RefPtr<Cairo::Context> cr,int oldkey,int newkey,double y);
private:
  double x_l;
  double x_r;
  double y_0;
};

staff_t::staff_t()
  : clef(alto),key(0),x_l(-10),x_r(10),y_0(0)
{
}

void staff_t::draw_keychange(Cairo::RefPtr<Cairo::Context> cr,int oldkey,int newkey,double y)
{
}

void staff_t::set_coords(double width,double y)
{
  x_l = -0.5*width;
  x_r = 0.5*width;
  y_0 = y;
}

void staff_t::draw(Cairo::RefPtr<Cairo::Context> cr)
{
  for(int i=-2;i<3;i++){
    cr->move_to( x_l, -(y_0 + 2*i) );
    cr->line_to( x_r, -(y_0 + 2*i) );
  }
  cr->stroke();
  clef.draw_full(cr,x_l,y_0);
  draw_keychange(cr,0,key,y_0);
}

class visualize_t : public Gtk::DrawingArea
{
public:
  visualize_t();
  virtual ~visualize_t();
protected:
  //Override default signal handler:
  virtual bool on_expose_event(GdkEventExpose* event);
  bool on_timeout();
  std::vector<staff_t> staves;
};

visualize_t::visualize_t()
{
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &visualize_t::on_timeout), 100 );
#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  //Connect the signal handler if it isn't already a virtual method override:
  signal_expose_event().connect(sigc::mem_fun(*this, &visualize_t::on_expose_event), false);
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  staves.resize(5);
  for(unsigned int k=0;k<staves.size();k++){
    staves[k].set_coords(100,-20.0*(k-0.5*(staves.size()-1.0)));
    staves[k].key = k;
  }
  staves[0].clef = treble;
  staves[1].clef = treble;
  staves[4].clef = bass;
}

visualize_t::~visualize_t()
{
}

bool visualize_t::on_expose_event(GdkEventExpose* event)
{
  // This is where we draw on the window
  Glib::RefPtr<Gdk::Window> window = get_window();
  if(window)
    {
      Gtk::Allocation allocation = get_allocation();
      const int width = allocation.get_width();
      const int height = allocation.get_height();
      Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();
      if(event)
	{
	  // clip to the area indicated by the expose event so that we only
	  // redraw the portion of the window that needs to be redrawn
	  cr->rectangle(event->area.x, event->area.y,
			event->area.width, event->area.height);
	  cr->clip();
	}
      cr->translate(0.5*width, 0.5*height);
      cr->scale(0.0075*height, 0.0075*height);
      //cr->set_line_width(0.1);
      cr->set_line_width(0.2);
      cr->save();
      //cr->set_source_rgb( 0,0.07,0.06 );
      cr->set_source_rgb( 1,1,1 );
      cr->paint();
      cr->restore();
      cr->set_source_rgb(0, 0, 0);
      cr->select_font_face("Emmentaler-16",Cairo::FONT_SLANT_NORMAL,Cairo::FONT_WEIGHT_NORMAL);
      cr->set_font_size(8);
      for(std::vector<staff_t>::iterator staff=staves.begin();staff!=staves.end();++staff)
        staff->draw(cr);
    }
  return true;
}

bool visualize_t::on_timeout()
{
  // force our program to redraw the entire clock.
  Glib::RefPtr<Gdk::Window> win = get_window();
  if (win)
    {
      Gdk::Rectangle r(0, 0, get_allocation().get_width(),
		       get_allocation().get_height());
      win->invalidate_rect(r, false);
    }
  return true;
}

int main(int argc, char** argv)
{
  Gtk::Main kit(argc, argv);
  Gtk::Window win;
  visualize_t n;
  win.add(n);
  win.set_title("music");
  win.set_default_size(1024,768);
  win.show_all();
  Gtk::Main::run(win);
  return 0;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
