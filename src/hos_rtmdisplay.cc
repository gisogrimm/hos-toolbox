#include <gtkmm.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/drawingarea.h>
#include <cairomm/context.h>
#include <lo/lo.h>
#include "osc_helper.h"
#include "libhos_music.h"
#include "hos_defs.h"
#include "errorhandling.h"

#define DRAWKEY

class clef_t {
public:
  std::string symbol;
  std::string smallsymbol;
  int position;
  int c1pos;
  void draw_full(Cairo::RefPtr<Cairo::Context> cr,double x, double y);
};

uint32_t checklen(uint32_t l)
{
  if( l > MAXLEN )
    throw TASCAR::ErrMsg("Invalid length.");
  return l;
}

std::string text(uint32_t n)
{
  char ctmp[1024];
  sprintf(ctmp,"%d",n);
  return ctmp;
}

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
  std::string rest[7] = {"","","","","","",""};
  std::string dot(".");
}

class graphical_note_t : public note_t {
public:
  graphical_note_t(const note_t& note, const clef_t& clef, int key,const graphical_note_t& prev=graphical_note_t());
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

graphical_note_t::graphical_note_t(const note_t& note, const clef_t& clef, int key,const graphical_note_t& prev)
  : note_t(note),
    sym_head(Symbols::notehead[checklen(length)/2])
{
  if( pitch != PITCH_REST ){
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
    if( prev.y == y ){
      if( prev.alteration == alteration ){
        sym_alteration = "";
      }else{
        sym_alteration = Symbols::alteration[alteration+2];
      }
    }else{
      if( alteration != 0 )
        sym_alteration = Symbols::alteration[alteration+2];
    }
    if( y > 0 )
      sym_flag = Symbols::flag_down[checklen(length)/2];
    else
      sym_flag = Symbols::flag_up[checklen(length)/2];
  }else{
    y = 2*(length == 1);
    alteration = 0;
    sym_head = Symbols::rest[checklen(length)/2];
    sym_alteration = "";
    sym_flag = "";
  }
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
  // draw dot if needed:
  if( !(length & 1) ){
    double dy(-0.5);
    if( !(y & 1) )
      dy = 0.5;
    cr->move_to(x+extents.width+1,-(y_0+y+dy));
    cr->show_text(Symbols::dot);
  }
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
    flag_y = -std::max(6.0,3.0+length/2);
  }else{
    flag_x = extents.x_bearing+extents.width;
    flag_y = std::max(6.0,3.0+length/2);
  }
  // draw stem:
  if( (length/2 > 1) && (pitch != PITCH_REST) ){
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
  //int key;
  void draw_empty(Cairo::RefPtr<Cairo::Context> cr);
  //void draw_keychange(Cairo::RefPtr<Cairo::Context> cr,int oldkey,int newkey,double y);
  void draw_music(Cairo::RefPtr<Cairo::Context> cr,double time,double x);
  double left_space(Cairo::RefPtr<Cairo::Context> cr,double time);
  double right_space(Cairo::RefPtr<Cairo::Context> cr,double time);
  void add_note(note_t n,int32_t fifths);
  void clear_music(double t0);
  void clear_all();
private:
  double x_l;
  double x_r;
public:
  double y_0;
private:
  std::map<double,graphical_note_t> notes;
};

staff_t::staff_t()
  : clef(Symbols::alto),//key(0),
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

void staff_t::clear_all()
{
  notes.clear();
}

void staff_t::clear_music(double t0)
{
  while( notes.size() && (notes.begin()->first < t0) )
    notes.erase(notes.begin());
}

void staff_t::add_note(note_t n,int32_t fifths)
{
  notes[n.time] = graphical_note_t(n,clef,fifths);
  std::map<double,graphical_note_t>::iterator prev=notes.find(n.time);
  if( prev != notes.begin() ){
    prev--;
    notes[n.time] = graphical_note_t(n,clef,fifths,prev->second);
  }
}

//void staff_t::draw_keychange(Cairo::RefPtr<Cairo::Context> cr,int oldkey,int newkey,double y)
//{
//}

void staff_t::set_coords(double width,double y)
{
  x_l = -0.5*width;
  x_r = 0.6*width;
  y_0 = y;
}

void staff_t::draw_empty(Cairo::RefPtr<Cairo::Context> cr)
{
  for(int i=-2;i<3;i++){
    cr->move_to( x_l, -(y_0 + 2*i) );
    cr->line_to( x_r, -(y_0 + 2*i) );
  }
  cr->stroke();
  clef.draw_full(cr,x_l,y_0);
}

class graphical_time_signature_t : public time_signature_t {
public:
  graphical_time_signature_t();
  graphical_time_signature_t(double nom,double denom,double startt,uint32_t addb);
  void draw(Cairo::RefPtr<Cairo::Context> cr,double x,double y);
  double space(Cairo::RefPtr<Cairo::Context> cr);
private:
  std::string s_denom;
  std::string s_nom;
  double l_denom;
  double l_nom;
};

graphical_time_signature_t::graphical_time_signature_t()
  : time_signature_t(),l_denom(0),l_nom(0)
{
}

graphical_time_signature_t::graphical_time_signature_t(double nom,double denom,double startt,uint32_t addb)
  : time_signature_t(nom,denom,startt,addb),
    s_denom(text(denom)),s_nom(text(nom)),
    l_denom(0),l_nom(0)
{
}

void graphical_time_signature_t::draw(Cairo::RefPtr<Cairo::Context> cr,double x,double y)
{
  double xl(space(cr));
  //+0.5*(l_nom-std::min(l_nom,l_denom))
  cr->move_to(x-xl+0.5*(l_denom-std::min(l_nom,l_denom)),-y);
  cr->show_text(text(numerator));
  cr->move_to(x-xl+0.5*(l_nom-std::min(l_nom,l_denom)),-y+4);
  cr->show_text(text(denominator));
}

double graphical_time_signature_t::space(Cairo::RefPtr<Cairo::Context> cr)
{
  Cairo::TextExtents extents;
  cr->get_text_extents(s_denom,extents);
  l_denom = extents.x_bearing+extents.width;
  cr->get_text_extents(s_nom,extents);
  l_nom = extents.x_bearing+extents.width;
  return std::max(l_denom,l_nom)+1;
}

class score_t : public Gtk::DrawingArea, public TASCAR::osc_server_t
{
public:
  score_t();
  virtual ~score_t();
  static int set_time(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int add_note(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int clear_all(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int set_timesig(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int set_keysig(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  void set_time(double t);
  void clear_all();
  void add_note(unsigned int voice,int pitch,unsigned int length,double time);
  void draw(Cairo::RefPtr<Cairo::Context> cr);
  double get_xpos(double time);
  void set_time_signature(uint32_t numerator,uint32_t deominator,double starttime);
  double bar(double time);
  void set_keysig(double time,int32_t pitch,keysig_t::mode_t mode);
protected:
  //Override default signal handler:
  virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
  bool on_timeout();
  std::vector<staff_t> staves;
  std::map<double,double> xpositions;
  double timescale;
  double history;
  double time;
  double x_left;
  double prev_tpos;
  double xshift;
  std::map<double,graphical_time_signature_t> timesig;
  std::map<double,keysig_t> keysig;
  pthread_mutex_t mutex;
};

void score_t::set_keysig(double time,int32_t pitch,keysig_t::mode_t mode)
{
  pthread_mutex_lock( &mutex );
  keysig[time] = keysig_t(pitch,mode);
  pthread_mutex_unlock( &mutex );
}

double score_t::bar(double time)
{
  // no time signature, thus no bar number:
  if( timesig.empty() )
    return 0;
  // find next time signature which is after given time:
  std::map<double,graphical_time_signature_t>::iterator ts(timesig.lower_bound(time));
  // if time signature is not the first one decrease by one to find
  // current time signature:
  if( ts != timesig.begin() )
    ts--;
  // return bar number of appropriate time signature:
  return ts->second.bar(time);
}

double score_t::get_xpos(double time)
{
  if( xpositions.empty() )
    return 0;
  std::map<double,double>::const_iterator xp1(xpositions.lower_bound(time));
  if( xp1 == xpositions.end() ){
    // time is larger than all stored positions, extrapolate:
    xp1--;
    return xp1->second+(time-xp1->first)*timescale;
  }
  if( xp1->first == time )
    // exact match, return second:
    return xp1->second;
  if( xp1 == xpositions.begin() )
    // time is less than all stored positions, extrapolate:
    return xp1->second+(time-xp1->first)*timescale;
  // interpolate:
  //return xp1->second + xshift;
  std::map<double,double>::const_iterator xp0(xp1);
  xp0--;
  return (time-xp0->first)/(xp1->first-xp0->first)*(xp1->second-xp0->second)+xp0->second;
}

int score_t::set_keysig(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc == 3) )
    ((score_t*)user_data)->set_keysig(argv[0]->f,argv[1]->i,(keysig_t::mode_t)(argv[2]->i));
  return 0;
}

int score_t::set_time(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc == 1) && (types[0] == 'f') )
    ((score_t*)user_data)->set_time(argv[0]->f);
  return 0;
}

int score_t::set_timesig(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc == 3) )
    ((score_t*)user_data)->set_time_signature(argv[1]->i,argv[2]->i,argv[0]->f);
  return 0;
}

int score_t::add_note(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc == 4) )
    ((score_t*)user_data)->add_note(argv[0]->i,argv[1]->i,argv[2]->i,argv[3]->f);
  return 0;
}

int score_t::clear_all(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data  )
    ((score_t*)user_data)->clear_all();
  return 0;
}

void score_t::set_time(double t)
{
  pthread_mutex_lock( &mutex );
  time = t;
  pthread_mutex_unlock( &mutex );
}

void score_t::clear_all()
{
  pthread_mutex_lock( &mutex );
  for(std::vector<staff_t>::iterator staff=staves.begin();staff!=staves.end();++staff)
    staff->clear_all();
  xpositions.clear();
  timesig.clear();
  keysig.clear();
  xshift = 0;
  pthread_mutex_unlock( &mutex );
}

void score_t::add_note(unsigned int voice,int pitch,unsigned int length,double time)
{
  pthread_mutex_lock( &mutex );
  note_t n;
  n.pitch = pitch;
  n.length = length;
  n.time = time;
  int fifths(0);
  if( !keysig.empty()){
    std::map<double,keysig_t>::iterator ks(keysig.upper_bound(time));
    if( ks != keysig.begin() )
      ks--;
    fifths = ks->second.fifths;
  }
  staves[voice].add_note(n,fifths);
  // get maximum x-position:
  double xmax(0);
  xmax = n.duration()*timescale;
  xpositions[time] = xmax;
  pthread_mutex_unlock( &mutex );
}

void score_t::set_time_signature(uint32_t numerator,uint32_t denominator,double starttime)
{
  pthread_mutex_lock( &mutex );
  timesig[starttime] = graphical_time_signature_t(numerator,denominator,starttime,0);
  std::map<double,graphical_time_signature_t>::iterator ts(timesig.find(starttime));
  // if time signature is not the first one decrease by one to find
  // current time signature:
  if( ts != timesig.begin() )
    ts--;
  timesig[starttime].addbar = ts->second.bar(starttime);
  xpositions[starttime] = 0;
  pthread_mutex_unlock( &mutex );
}

score_t::score_t()
  //: TASCAR::osc_server_t("239.255.1.7","9877"),timescale(20),history(6),time(0),x_left(-105),prev_tpos(0),xshift(0)
  : TASCAR::osc_server_t("239.255.1.7","9877"),timescale(20),history(6),time(0),x_left(-105),prev_tpos(0),xshift(0)
{
  pthread_mutex_init( &mutex, NULL );
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &score_t::on_timeout), 20 );
#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  //Connect the signal handler if it isn't already a virtual method override:  signal_expose_event().connect(sigc::mem_fun(*this, &score_t::on_expose_event), false);
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  staves.resize(5);
  for(unsigned int k=0;k<staves.size();k++){
    staves[k].set_coords(-2*x_left+18,-20.0*(k-0.5*(staves.size()-1.0)));
    //staves[k].key = 0;
  }
  staves[0].clef = Symbols::treble;
  staves[1].clef = Symbols::treble;
  //staves[3].clef = Symbols::bass;
  staves[4].clef = Symbols::bass;
  add_method("/time","f",score_t::set_time,this);
  add_method("/note","iiif",score_t::add_note,this);
  add_method("/clear","",score_t::clear_all,this);
  add_method("/timesig","fii",score_t::set_timesig,this);
  add_method("/key","fii",score_t::set_keysig,this);
  osc_server_t::activate();
}

score_t::~score_t()
{
  osc_server_t::deactivate();
  pthread_mutex_trylock( &mutex );
  pthread_mutex_unlock(  &mutex );
  pthread_mutex_destroy( &mutex );
}

void score_t::draw(Cairo::RefPtr<Cairo::Context> cr)
{
  // initialize graphics:
  cr->set_source_rgb(0, 0, 0);
  cr->select_font_face("Emmentaler-16",Cairo::FONT_SLANT_NORMAL,Cairo::FONT_WEIGHT_NORMAL);
  cr->set_font_size(8);
  // start access to data:
  pthread_mutex_lock( &mutex );
  // clean time database:
  double t0(time-history);
  while( xpositions.size() && (xpositions.begin()->first < t0) )
    xpositions.erase(xpositions.begin());
  for(std::vector<staff_t>::iterator staff=staves.begin();staff!=staves.end();++staff)
    staff->clear_music(t0);
  // process graphical timing positions:
  // playhead marker:
  cr->save();
  double x_marker(get_xpos(time-5.1));
  for(uint32_t k=1;k<5;k++){
    double w(3.0);
    w = w/(w+k);
    cr->set_source_rgb( 1,w,w );
    cr->set_line_width(18*w);
    cr->move_to(x_left+x_marker,-(staves.begin()->y_0+18));
    cr->line_to(x_left+x_marker,-(staves.rbegin()->y_0-18));
    cr->stroke();
  }
  cr->restore();
  // draw empty staff:
  for(std::vector<staff_t>::iterator staff=staves.begin();staff!=staves.end();++staff)
    staff->draw_empty(cr);
  // draw music:
  double tpos(0);
  if( xpositions.size()){
    tpos = xpositions.begin()->first;
  }
  if( (tpos != prev_tpos) && (prev_tpos != 0) ){
    xshift = xpositions[tpos];
  }
  xshift -= 0.03*xshift;
  // main music draw section:
  double xpos(0);
  for(std::map<double,double>::iterator xp=xpositions.begin();xp!=xpositions.end();++xp){
    double lspace(0);
    double rspace(0);
    double tsspace(0);
    // get graphical extension of music and non-music:
    std::map<double,graphical_time_signature_t>::iterator ts(timesig.find(xp->first));
    if( ts != timesig.end() ){
      tsspace = ts->second.space(cr);
    }
    for(std::vector<staff_t>::iterator staff=staves.begin();staff!=staves.end();++staff){
      lspace = std::max(lspace,staff->left_space(cr,xp->first));
      rspace = std::max(rspace,staff->right_space(cr,xp->first));
    }
    // increase x-position by left space
    xpos += tsspace + lspace + (xp->first-tpos)*timescale;
    // draw time signature and music
    if( ts != timesig.end() ){
      for(std::vector<staff_t>::iterator staff=staves.begin();staff!=staves.end();++staff)
        ts->second.draw(cr,xpos+xshift+x_left-lspace,staff->y_0);
    }
    for(std::vector<staff_t>::iterator staff=staves.begin();staff!=staves.end();++staff)
      staff->draw_music(cr,xp->first,xpos+xshift+x_left);
    xp->second = xpos+xshift-lspace-tsspace;
    xpos += rspace;
    if( xp == xpositions.begin() ){
      prev_tpos = tpos;
    }
    tpos = xp->first;
  }
  // draw bar lines here:
  if( !timesig.empty() ){
    double bar_endtime(tpos);
    for(std::map<double,graphical_time_signature_t>::reverse_iterator it=timesig.rbegin();it!=timesig.rend();++it){
      if( bar_endtime > prev_tpos ){
        double bar_starttime(std::max(prev_tpos,it->second.starttime));
        if( bar_starttime > 0 ){
          // draw bar lines from ... to bar_endtime
          for(double bar=ceil(it->second.bar(bar_starttime));bar<ceil(it->second.bar(bar_endtime));bar+=1){
            double xbar(get_xpos(it->second.time(bar))-1.0);
            if( it->second.time(bar) == it->first )
              xbar += it->second.space(cr);
            // bar numbers for debugging:
            cr->save();
            cr->select_font_face("Arial",Cairo::FONT_SLANT_NORMAL,Cairo::FONT_WEIGHT_NORMAL);
            cr->set_font_size(3);
            char ctmp[40];
            cr->move_to(x_left+xbar,-(staves.begin()->y_0+10));
            sprintf(ctmp,"%g (%g)",bar,it->second.time(bar));
            cr->show_text(ctmp);
            cr->restore();
            // end bar numbers.
            for(std::vector<staff_t>::iterator staff=staves.begin();staff!=staves.end();++staff){
              std::vector<staff_t>::iterator nstaff(staff);
              nstaff++;
              if( nstaff != staves.end() ){
                cr->move_to(x_left+xbar,-(staff->y_0-4));
                cr->line_to(x_left+xbar,-(nstaff->y_0+4));
                cr->stroke();
              }
            }
          }
        }
      }
      bar_endtime = std::min(it->second.starttime,tpos);
    }
  }
#ifdef DRAWKEY
  // draw key signature here:
  for( std::map<double,keysig_t>::iterator ks=keysig.begin();ks!=keysig.end();++ks){
    if( (ks->first >= prev_tpos) && (ks->first <= tpos) ){
      double xpos(get_xpos(ks->first));
      std::map<double,graphical_time_signature_t>::iterator ts(timesig.find(ks->first));
      if( ts != timesig.end() )
        xpos += ts->second.space(cr);
      cr->save();
      cr->move_to(x_left+xpos,-staves.rbegin()->y_0+12);
      cr->select_font_face("Arial",Cairo::FONT_SLANT_NORMAL,Cairo::FONT_WEIGHT_BOLD);
      cr->set_font_size(6);
      cr->show_text(ks->second.name().c_str());
      cr->restore();
    }
  }
#endif
  pthread_mutex_unlock( &mutex );
}

bool score_t::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  Gtk::Allocation allocation = get_allocation();
  const int width = allocation.get_width();
  const int height = allocation.get_height();
  cr->rectangle(0,0,width,height);
  cr->clip();
  cr->translate(0.5*width, 0.5*height);
  cr->scale(0.004*width, 0.004*width);
  //cr->set_line_width(0.1);
  cr->set_line_width(0.2);
  cr->save();
  //cr->set_source_rgb( 0,0.07,0.06 );
  cr->set_source_rgb( 1,1,1 );
  cr->paint();
  cr->restore();
  draw(cr);
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
  win.set_default_size(1024,480);
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
