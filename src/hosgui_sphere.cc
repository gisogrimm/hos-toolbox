/**
   \file hosgui_sphere.cc
   \ingroup apphos
   \brief Classes for trajectory visualization
   \author Giso Grimm
   \date 2011

   \section license License (GPL)
   
   Copyright (C) 2011 Giso Grimm

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2
   of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
/**
 */
#include "hosgui_sphere.h"
#include "hos_defs.h"
#include <math.h>

using namespace HoSGUI;

static double spheres_colours [6][3] = {
  {0.581818, 0.290909, 0.290909},
  {0.522727, 0.581818, 0.290909},
  {0.290909, 0.581818, 0.404545},
  {.290909, 0.404545, 0.581818},
  {0.522727, 0.290909, 0.581818},
  {0.3, 0.3, 0.3}
};
extern double brightness;
double brightness = 0.5;

void set_hoscolor( unsigned int k, const Cairo::RefPtr<Cairo::Context>& cr, double alpha)
{
  k = k % 6;
  /*
    switch( k ){
    case 0 :
    cr->set_source_rgba( 0.581818, 0.290909, 0.290909, alpha );
    break;
    case 1 :
    cr->set_source_rgba( 0.522727, 0.581818, 0.290909, alpha );
    break;
    case 2 :
    cr->set_source_rgba( 0.290909, 0.581818, 0.404545, alpha );
    break;
    case 3 :
    cr->set_source_rgba( 0.290909, 0.404545, 0.581818, alpha );
    break;
    case 4 :
    cr->set_source_rgba( 0.522727, 0.290909, 0.581818, alpha );
    break;
    case 5 :
    cr->set_source_rgba( 0.3, 0.3, 0.3, alpha );
    break;
    }
  */
  double r = spheres_colours[k][0];
  double g = spheres_colours[k][1];
  double b = spheres_colours[k][2];
  if (brightness >= 0.5 && brightness <= 1.0) {
    double df = (brightness - 0.5) * 2;
    r += (1-r) * df;
    g += (1-g) * df;
    b += (1-b) * df;
  }
  else if (brightness >= 0.0 && brightness < 0.5) {
    double df = brightness * 2;
    r *= df;
    g *= df;
    b *= df;
  }
  cr->set_source_rgba( r, g, b, alpha );
}

double pos_t::get_x()
{
  return r*cos(phi); 
}

double pos_t::get_y()
{
  return -r*sin(phi); 
}

int pos_tail_t::addpoint(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (argc == 2) && (types[0] == 'f') && (types[1] == 'f') ){
    ((pos_tail_t*)user_data)->addpoint(argv[1]->f,argv[0]->f);
    return 0;
  }
  return 1;
}

void pos_tail_t::set_rotate(double r)
{
  rotate = r;
}

pos_tail_t::pos_tail_t(const std::string& pAddr, lo_server_thread & lost, unsigned int k)
  : rotate(0)
{
  pthread_mutex_init( &mutex, NULL );
  tail.resize(100);
  tailp = tail.begin();
  lo_server_thread_add_method(lost,pAddr.c_str(),"ff",pos_tail_t::addpoint,this);
  k_ = k;
}

pos_tail_t::~pos_tail_t()
{
  pthread_mutex_destroy( &mutex );
}

void pos_tail_t::addpoint(float r,float phi)
{
  pthread_mutex_lock( &mutex );
  std::vector<pos_t>::iterator next(tailp);
  if( next == tail.begin() )
    next = tail.end();
  --next;
  tailp = next;
  *tailp = pos_t(r,phi-rotate);
  pthread_mutex_unlock( &mutex );
}

void pos_tail_t::draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  pthread_mutex_lock( &mutex );
  cr->save();
  set_hoscolor( k_, cr, 0.6 );
  std::vector<pos_t>::iterator tp(tailp);
  cr->move_to( tp->get_x(), tp->get_y() );
  for( std::vector<pos_t>::iterator t = tp; t != tail.end(); ++t ){
    cr->line_to( t->get_x(), t->get_y() );
    //cr->move_to( t->get_x(), t->get_y() );
  }
  for( std::vector<pos_t>::iterator t = tail.begin(); t != tp; ++t ){
    cr->line_to( t->get_x(), t->get_y() );
    //cr->move_to( t->get_x(), t->get_y() );
  }
  cr->stroke();
  cr->restore();
  cr->save();
  set_hoscolor( k_, cr, 1 );
  //cr->arc(tp->get_x(), tp->get_y(), 0.3, 0, 2 * M_PI);
  cr->arc(tp->get_x(), tp->get_y(), 0.6, 0, 2 * M_PI);
  cr->fill();
  cr->restore();
  pthread_mutex_unlock( &mutex );
}

visualize_t::visualize_t(const std::vector<std::string>& paddr, lo_server_thread & l)
  : lost(l),names(paddr),rotate(0)
{
  for(unsigned int k=0;k<paddr.size();k++){
    vTail.push_back(new pos_tail_t("/pos/"+paddr[k],lost,k));
  }
  
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &visualize_t::on_timeout), 100 );
  
#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  //Connect the signal handler if it isn't already a virtual method override:
  //signal_expose_event().connect(sigc::mem_fun(*this, &visualize_t::on_expose_event), false);
  signal_draw().connect(sigc::mem_fun(*this, &visualize_t::on_draw), false);
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED

}

visualize_t::~visualize_t()
{
  for( unsigned int k=0;k<vTail.size();k++){
    delete vTail[k];
  }
}

void visualize_t::set_rotate(double r)
{
  rotate = r;
  for(unsigned int k=0;k<vTail.size();k++)
    vTail[k]->set_rotate(r);
}

bool visualize_t::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  //// This is where we draw on the window
  //Glib::RefPtr<Gdk::Window> window = get_window();
  //if(window)
  //  {
  Gtk::Allocation allocation = get_allocation();
  const int width = allocation.get_width();
  const int height = allocation.get_height();
  //
  //Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();
  //if(event)
  //{
  // clip to the area indicated by the expose event so that we only
  // redraw the portion of the window that needs to be redrawn
  cr->rectangle(0,0,width,height);
  cr->clip();
  //}
  cr->translate(0.5*width, 0.5*height);
  cr->scale(0.05*height, 0.05*height);
  //cr->set_line_width(0.1);
  cr->set_line_width(0.2);
  cr->save();
  //cr->set_source_rgb( 0,0.07,0.06 );
  cr->set_source_rgb( 0,0,0 );
  cr->paint();
  cr->restore();
  cr->set_source_rgba(0.8, 0.8, 0.8, 0.8);
  cr->move_to(-0.3, 0 );
  cr->line_to( 0.3, 0 );
  cr->move_to( 0, -0.3 );
  cr->line_to( 0,  0.3 );
  cr->stroke();
    cr->save();
  cr->set_line_width(0.1);
  cr->set_font_size(0.5);
  for(unsigned int k=0;k<vTail.size();k++){
    set_hoscolor( k, cr, 1 );
    cr->move_to(0,0);
    pos_t p( 6, PI2*((double)k+0.75)/vTail.size()-rotate);
    cr->line_to(p.get_x(),p.get_y());
    cr->stroke();
    cr->move_to(p.get_x(),p.get_y());
    cr->show_text(names[k]);
    vTail[k]->draw(cr);
  }
    cr->restore();
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



/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
