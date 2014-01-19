#include <gtkmm.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/drawingarea.h>
#include <cairomm/context.h>
#include <lo/lo.h>
#include "osc_helper.h"
#include "libhos_music.h"
#include "defs.h"

class clef_t {
public:
  std::string symbol;
  std::string smallsymbol;
  int position;
  int c1pos;
  void draw_full(Cairo::RefPtr<Cairo::Context> cr,double x, double y);
};

void clef_t::draw_full(Cairo::RefPtr<Cairo::Context> cr,double x, double y)
{
  cr->move_to(x, -(y+position) );
  cr->show_text(symbol);
}

namespace Symbols {
  clef_t alto = { "", "",0,0};
  clef_t tenor = { "", "",2,2};
  clef_t bass = { "", "",2,6};
  clef_t treble = {"","",-2,-6};
  // 0:brevis,1:ganze,2:halbe,3:4tel,4:8tel,5:16tel,6:32tel
  std::string notehead[7] = {"","","","","","",""};
  std::string flag_up[7] = {"","","","","","",""};
  std::string flag_down[7] = {"","","","","","",""};
  std::string alteration[5] = {"","","","",""};

}

class graphical_note_t : public note_t {
public:
  graphical_note_t(const note_t& note, const clef_t& clef, int key);
  graphical_note_t();
  int y;
  int alteration;
  friend std::ostream& operator<<(std::ostream& o, const graphical_note_t& p) { o << "y=" << p.y << " ("<<p.alteration << ")"; return o;};
  void draw(Cairo::RefPtr<Cairo::Context> cr,double x,double y_0);
  double xmin(Cairo::RefPtr<Cairo::Context> cr);
  double xmax(Cairo::RefPtr<Cairo::Context> cr);
private:
  std::string sym_head;
  std::string sym_alteration;
  std::string sym_flag;
};

graphical_note_t::graphical_note_t()
  : y(0),
    alteration(0)
{
}

graphical_note_t::graphical_note_t(const note_t& note, const clef_t& clef, int key)
  : note_t(note),
    sym_head(Symbols::notehead[length])
{
  int octave(floor((double)pitch/12.0));
  int p_c = pitch-12*octave;
  y = floor(7.0/12.0*p_c)+(p_c==5);
  int nonaltered_pitch = floor((y+0.5)*12.0/7.0)-(y==3);
  alteration = p_c - nonaltered_pitch;
  key = closest_key( p_c, key );
  if( (key < 0) && (alteration > 0) ){
    y++;
    alteration-=2;
  }
  if( (key > 0) && (alteration < 0) ){
    y--;
    alteration+=2;
  }
  if( (key < -6) && (alteration == 0) ){
    y++;
    alteration-=2;
  }
  if( (key > 6) && (alteration == 0) ){
    y--;
    alteration+=2;
  }
  y += 7*octave;
  y += clef.c1pos;
  if( alteration != 0 )
    sym_alteration = Symbols::alteration[alteration+2];
  if( y > 0 )
    sym_flag = Symbols::flag_down[length];
  else
    sym_flag = Symbols::flag_up[length];
}

double graphical_note_t::xmin(Cairo::RefPtr<Cairo::Context> cr)
{
  Cairo::TextExtents extents;
  cr->get_text_extents(sym_head,extents);
  double x(extents.x_bearing);
  if( sym_alteration.size() )
    x -= 2.4;
  return x;
}

double graphical_note_t::xmax(Cairo::RefPtr<Cairo::Context> cr)
{
  Cairo::TextExtents extents;
  cr->get_text_extents(sym_head,extents);
  double x(extents.width+extents.x_bearing);
  double x0(extents.x_bearing);
  if( sym_flag.size() ){
    cr->get_text_extents(sym_flag,extents);
    if( y > 0 )
      x = std::max(x,x0+extents.x_bearing+extents.width);
    else
      x += extents.x_bearing+extents.width;
  }
  return x;
}

void graphical_note_t::draw(Cairo::RefPtr<Cairo::Context> cr,double x,double y_0)
{
  Cairo::TextExtents extents;
  cr->get_text_extents(sym_head,extents);
  // draw head:
  cr->move_to(x,-(y_0+y));
  cr->show_text(sym_head);
  // draw alteration:
  if( sym_alteration.size() ){
    cr->move_to(x-2.4+extents.x_bearing,-(y_0+y));
    cr->show_text(sym_alteration);
  }
  // draw flag:
  double flag_x;
  double flag_y;
  if( y > 0 ){
    flag_x = extents.x_bearing;
    flag_y = -std::max(6.0,3.0+length);
  }else{
    flag_x = extents.x_bearing+extents.width;
    flag_y = std::max(6.0,3.0+length);
  }
  // draw stem:
  if( length > 1 ){
    cr->save();
    cr->set_line_width(0.3);
    cr->move_to(x+flag_x,-(y_0+y));
    cr->line_to(x+flag_x,-(y_0+y+flag_y));
    cr->stroke();
    cr->move_to(x+flag_x,-(y_0+y+flag_y));
    cr->show_text(sym_flag);
    cr->restore();
  }
  // draw support lines:
  cr->save();
  cr->set_line_width(0.4);
  for(int yl=6;yl<=y;yl+=2){
    cr->move_to(x+extents.x_bearing-1,-(y_0+yl));
    cr->line_to(x+extents.x_bearing+extents.width+1,-(y_0+yl));
  }
  for(int yl=-6;yl>=y;yl-=2){
    cr->move_to(x+extents.x_bearing-1,-(y_0+yl));
    cr->line_to(x+extents.x_bearing+extents.width+1,-(y_0+yl));
  }
  cr->stroke();
  cr->restore();
}

class staff_t {
public:
  staff_t();
  void set_coords(double width,double y);
  clef_t clef;
  int key;
  void draw(Cairo::RefPtr<Cairo::Context> cr);
  void draw_keychange(Cairo::RefPtr<Cairo::Context> cr,int oldkey,int newkey,double y);
  void draw_music(Cairo::RefPtr<Cairo::Context> cr,double time,double x);
  double left_space(Cairo::RefPtr<Cairo::Context> cr,double time);
  double right_space(Cairo::RefPtr<Cairo::Context> cr,double time);
  void add_note(note_t n);
  void clean_music(double t0);
private:
  double x_l;
  double x_r;
  double y_0;
  std::map<double,graphical_note_t> notes;
};

staff_t::staff_t()
  : clef(Symbols::alto),key(0),
    x_l(-10),x_r(10),y_0(0)
{
}

void staff_t::draw_music(Cairo::RefPtr<Cairo::Context> cr,double time,double x)
{
  std::map<double,graphical_note_t>::iterator note(notes.find(time));
  if( note!=notes.end())
    note->second.draw(cr,x,y_0);
}

double staff_t::left_space(Cairo::RefPtr<Cairo::Context> cr,double time)
{
  std::map<double,graphical_note_t>::iterator note(notes.find(time));
  if( note!=notes.end())
    return -note->second.xmin(cr);
  return 0;
}

double staff_t::right_space(Cairo::RefPtr<Cairo::Context> cr,double time)
{
  std::map<double,graphical_note_t>::iterator note(notes.find(time));
  if( note!=notes.end())
    return note->second.xmax(cr);
  return 0;
}

void staff_t::clean_music(double t0)
{
  while( notes.size() && (notes.begin()->first < t0) )
    notes.erase(notes.begin());
}

void staff_t::add_note(note_t n)
{
  notes[n.time] = graphical_note_t(n,clef,key);
}

void staff_t::draw_keychange(Cairo::RefPtr<Cairo::Context> cr,int oldkey,int newkey,double y)
{
}

void staff_t::set_coords(double width,double y)
{
  x_l = -0.5*width;
  x_r = 0.6*width;
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
}

class score_t : public Gtk::DrawingArea, public TASCAR::osc_server_t
{
public:
  score_t();
  virtual ~score_t();
  static int set_time(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int add_note(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  void set_time(double t);
  void add_note(unsigned int voice,int pitch,unsigned int length,double time);
  void draw(Cairo::RefPtr<Cairo::Context> cr);
protected:
  //Override default signal handler:
  virtual bool on_expose_event(GdkEventExpose* event);
  bool on_timeout();
  std::vector<staff_t> staves;
  std::map<double,double> xwidth;
  double timescale;
  double history;
  double time;
  double x_left;
  double smoothed_tpos;
  double smoothed_tempo;
  double prev_tpos;
};

int score_t::set_time(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc == 1) && (types[0] == 'f') )
    ((score_t*)user_data)->set_time(argv[0]->f);
  return 0;
}

int score_t::add_note(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc == 4) )
    ((score_t*)user_data)->add_note(argv[0]->i,argv[1]->i,argv[2]->i,argv[3]->f);
  return 0;
}

void score_t::set_time(double t)
{
  time = t;
}

void score_t::add_note(unsigned int voice,int pitch,unsigned int length,double time)
{
  note_t n;
  n.pitch = pitch;
  n.length = length;
  n.time = time;
  staves[voice].add_note(n);
  // get maximum x-position:
  double xmax(0);
  xmax = n.duration()*timescale;
  xwidth[time] = xmax;
}

score_t::score_t()
  : TASCAR::osc_server_t("239.255.1.7","9877"),timescale(30),history(1.2),time(0),x_left(-85),smoothed_tpos(0),smoothed_tempo(0),prev_tpos(0)
{
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &score_t::on_timeout), 20 );
#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  //Connect the signal handler if it isn't already a virtual method override:
  signal_expose_event().connect(sigc::mem_fun(*this, &score_t::on_expose_event), false);
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  staves.resize(5);
  for(unsigned int k=0;k<staves.size();k++){
    staves[k].set_coords(-2*x_left+18,-20.0*(k-0.5*(staves.size()-1.0)));
    staves[k].key = 0;
  }
  staves[0].clef = Symbols::treble;
  staves[1].clef = Symbols::treble;
  staves[3].clef = Symbols::tenor;
  staves[4].clef = Symbols::bass;
  add_method("/time","f",score_t::set_time,this);
  add_method("/note","iiif",score_t::add_note,this);
  osc_server_t::activate();
}

score_t::~score_t()
{
  osc_server_t::deactivate();
}

void score_t::draw(Cairo::RefPtr<Cairo::Context> cr)
{
  // initialize graphics:
  cr->set_source_rgb(0, 0, 0);
  cr->select_font_face("Emmentaler-16",Cairo::FONT_SLANT_NORMAL,Cairo::FONT_WEIGHT_NORMAL);
  cr->set_font_size(8);
  // clean time database:
  double t0(time-history);
  while( xwidth.size() && (xwidth.begin()->first < t0) )
    xwidth.erase(xwidth.begin());
  for(std::vector<staff_t>::iterator staff=staves.begin();staff!=staves.end();++staff)
    staff->clean_music(t0);
  // process graphical timing positions:
  // draw empty staff:
  for(std::vector<staff_t>::iterator staff=staves.begin();staff!=staves.end();++staff)
    staff->draw(cr);
  // draw music:
  double tpos(0);
  if( xwidth.size())
    tpos = xwidth.begin()->first;
  if( smoothed_tpos == 0 )
    smoothed_tpos = tpos;
  if( prev_tpos == 0 )
    prev_tpos = tpos+history-time;
  //double xpos(x_left+(tpos-time+history)*timescale);
  //smoothed_tempo = 0.99*smoothed_tempo + 0.01*(tpos-prev_tpos);
  //smoothed_tpos += smoothed_tempo;
  //smoothed_tpos = 0.999*smoothed_tpos + 0.001*tpos;
  //DEBUG(smoothed_tempo);
  //DEBUG(smoothed_tpos);
  //DEBUG(tpos);
  smoothed_tempo = 0.99*smoothed_tempo + 0.01*(tpos+history-time - prev_tpos);
  prev_tpos = tpos+history-time;
  smoothed_tpos += smoothed_tempo;
  DEBUG(smoothed_tempo);
  //smoothed_tpos = 0.99*smoothed_tpos + 0.01*tpos;
  //double xpos(x_left+(tpos-time+0.75*history)*timescale+0.25*history*smoothed_tempo);
  //double xpos(x_left+(smoothed_tpos+history-time)*timescale);
  double xpos(x_left+(tpos+history-time)*timescale);
  //smoothed_tempo = 0.99*smoothed_tempo + 0.01*xpos;
  //xpos = smoothed_tempo;
  //double xpos(x_left+(tpos-time+history)*smoothed_tempo);
  //double xpos(x_left);
  double xpos0(xpos);
  double tpos0(tpos);
  double xpos1(xpos);
  double tpos1(tpos);
  for(std::map<double,double>::iterator xp=xwidth.begin();xp!=xwidth.end();++xp){
    double lspace(0);
    double rspace(0);
    for(std::vector<staff_t>::iterator staff=staves.begin();staff!=staves.end();++staff){
      lspace = std::max(lspace,staff->left_space(cr,xp->first));
      rspace = std::max(rspace,staff->right_space(cr,xp->first));
    }
    xpos += lspace + (xp->first-tpos)*timescale;
    for(std::vector<staff_t>::iterator staff=staves.begin();staff!=staves.end();++staff)
      staff->draw_music(cr,xp->first,xpos);
    xpos += rspace;
    tpos = xp->first;
    if( tpos-tpos0 <= 0.25 ){
      xpos1 = xpos;
      tpos1 = tpos;
    }
  }
  xpos0 = xpos1 - xpos0;
  tpos0 = tpos1 - tpos0;
  if( tpos0 > 0 ){
    xpos0 = xpos0/tpos0;
    //smoothed_tempo = 0.99*smoothed_tempo + 0.01*xpos0;
  }
  //smoothed_tempo = xpos0;
  //DEBUG(xpos0);
  //DEBUG(smoothed_tempo);
  //if( xwidth.size())
  //prev_tpos = xwidth.begin()->first;
}

bool score_t::on_expose_event(GdkEventExpose* event)
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
      cr->scale(0.005*width, 0.005*width);
      //cr->set_line_width(0.1);
      cr->set_line_width(0.2);
      cr->save();
      //cr->set_source_rgb( 0,0.07,0.06 );
      cr->set_source_rgb( 1,1,1 );
      cr->paint();
      cr->restore();
      draw(cr);
    }
  return true;
}

bool score_t::on_timeout()
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
  score_t n;
  win.add(n);
  win.set_title("music");
  win.set_default_size(1280,720);
  win.fullscreen();
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
