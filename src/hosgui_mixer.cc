/**
   \file hosgui_mixer.cc
   \ingroup apphos
   \brief Classes for visualization of hdsp matrix mixer
   \author Giso Grimm
   \date 2011

   \section license License (GPL)
   
   Copyright (C) 2011 Giso Grimm

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
/*
 */
#include "hosgui_mixer.h"

using namespace HoSGUI;

int mixergui_t::hdspmm_new(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (argc == 2) && (types[0] == 'i') && (types[1] == 'i') ){
    ((mixergui_t*)user_data)->hdspmm_new(argv[0]->i,argv[1]->i);
    return 0;
  }
  return 1;
};

int mixergui_t::hdspmm_destroy(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  //((mixergui_t*)user_data)->hdspmm_destroy();
  return 0;
};

mixergui_t::mixergui_t(lo_server_thread& l, lo_address& a)
  : mm(NULL),
    modified(false),
    lost(l),
    addr(a)
{
  pthread_mutex_init( &mutex, NULL );
  lo_server_thread_add_method(lost,"/hdspmm/backend_new","ii",mixergui_t::hdspmm_new,this);
  lo_server_thread_add_method(lost,"/hdspmm/backend_quit","",mixergui_t::hdspmm_destroy,this);
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &mixergui_t::on_timeout), 125 );
#ifndef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  //Connect the signal handler if it isn't already a virtual method override:
  signal_expose_event().connect(sigc::mem_fun(*this, &mixergui_t::on_expose_event), false);
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
}

mixergui_t::~mixergui_t()
{
  pthread_mutex_destroy( &mutex );
}

void mixergui_t::hdspmm_destroy()
{
  pthread_mutex_lock( &mutex );
  if( mm )
    delete mm;
  mm = NULL;
  modified = true;
  pthread_mutex_unlock( &mutex );
}

void mixergui_t::hdspmm_new(unsigned int kout, unsigned int kin)
{
  pthread_mutex_lock( &mutex );
  if( mm )
    delete mm;
  mm = new MM::lo_matrix_t(kout,kin,lost,addr);
  modified = true;
  pthread_mutex_unlock( &mutex );
}


bool mixergui_t::on_expose_event(GdkEventExpose* event)
{
  // This is where we draw on the window
  Glib::RefPtr<Gdk::Window> window = get_window();
  if(window)
    {
      Gtk::Allocation allocation = get_allocation();
      const int width = allocation.get_width();
      const int height = allocation.get_height();
      double ratio = (double)width/(double)height;

      Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();

      if(event)
        {
          // clip to the area indicated by the expose event so that we only
          // redraw the portion of the window that needs to be redrawn
          cr->rectangle(event->area.x, event->area.y,
                        event->area.width, event->area.height);
          cr->clip();
        }
      pthread_mutex_lock( &mutex );
      double scale = (0.9*std::min(width,height))/1.4;

      cr->scale(scale,scale);
      cr->translate(0.06+0.4*ratio,0.45);
      cr->set_line_width(1.0/400.0);
      cr->set_source_rgb( 0.7, 0.7, 0.7 );
      // bg:
      cr->save();
      cr->set_source_rgb( 0, 0.07, 0.06 );
      cr->paint();
      cr->restore();
      // end bg
      if( mm ){
        double scx = 1.0/mm->get_num_inputs();
        double scy = 1.0/mm->get_num_outputs();
        double r = 0.35*std::min(ratio*scx,scy);
        // grid:
        //cr->set_line_width(1.0/400.0);
        //cr->set_source_rgb( 0.7, 0.7, 0.7 );
        for( unsigned int k=0;k<=mm->get_num_outputs();k++){
          double y = (double)k*scy;
          cr->move_to( -0.4*ratio, y );
          cr->line_to( ratio, y );
          cr->stroke();
        }
        for( unsigned int k=0;k<=mm->get_num_inputs();k++){
          double x = ratio*(double)k*scx;
          cr->move_to( x-0.4, -0.4 );
          cr->line_to( x, 0.0 );
          cr->line_to( x, 1.0 );
          cr->stroke();
        }
        // end grid
        // labels outputs:
        cr->save();
        cr->set_font_size( std::min(0.1,0.6*scy) );
        for( unsigned int k=0;k<mm->get_num_outputs();k++){
          double y = (k+0.8)*scy;
          std::string l = mm->get_name_out( k );
          cr->move_to( -0.4*ratio, y );
          cr->show_text( l.c_str() );
        }
        cr->restore();
        // inputs:
        cr->save();
        cr->set_font_size( std::min(0.1,ratio*0.6*0.7*scx) );
        for( unsigned int k=0;k<mm->get_num_inputs();k++){
          double x = ratio*(k+0.14)*scx;
          std::string l = mm->get_name_in( k );
          cr->save();
          cr->translate( x-0.35, -0.35 );
          cr->move_to( 0, 0 );
          cr->rotate( 0.7854 );
          cr->show_text( l.c_str() );
          cr->restore();
        }
        cr->restore();
        // gains:
        for( unsigned int kin=0;kin<mm->get_num_inputs(); kin++){
          for( unsigned int kout=0;kout < mm->get_num_outputs(); kout++){
            double xc = ratio*(kin+0.5)*scx;
            double yc = (kout+0.5)*scy;
            double x1 = ratio*(kin+0.03)*scx;
            double xtxt = ratio*(kin+0.06)*scx;
            double x2 = ratio*(kin+0.97)*scx;
            double y1 = (kout+0.03)*scy;
            double y2 = (kout+0.97)*scy;
            double ytxt = (kout+0.94)*scy;
            double gain = mm->get_gain( kout, kin);
            double pan = mm->get_pan( kout, kin);
            // color:
            cr->save();
            cr->set_source_rgb( gain, (gain>0)-gain, 0 );
            cr->move_to( x1, y1 );
            cr->line_to( x1, y2 );
            cr->line_to( x2, y2 );
            cr->line_to( x2, y1 );
            cr->fill();
            cr->restore();
            // end color
            // panner:
            if( (mm->get_n_in( kin ) == 1) && (pan >= 0) ){
              cr->save();
              cr->set_line_width(1.0/300.0);
              cr->set_source_rgba( 1, 1, 1, 0.5 );
              cr->begin_new_path();
              cr->arc( xc,yc, r, 0, 2*M_PI);
              cr->stroke();
              cr->set_line_width(1.0/150.0);
              //cr->move_to( xc, yc );
              cr->move_to( xc-0.6*r*cos(pan*2*M_PI), yc-0.6*r*sin(pan*2*M_PI) );
              cr->line_to( xc-r*cos(pan*2*M_PI), yc-r*sin(pan*2*M_PI) );
              cr->stroke();
              cr->restore();
            }
            // n_out:
            cr->save();
            cr->set_line_width(1.0/50.0);
            for( unsigned int kc = 0;kc < mm->get_n_out( kout ); kc++ ){
              cr->set_source_rgba( 1, 1, 0.3, 0.8 );
              double pk = (double)kc/mm->get_n_out( kout );
              cr->move_to( xc-0.9*r*cos(pk*2*M_PI), yc-0.9*r*sin(pk*2*M_PI) );
              cr->line_to( xc-1.1*r*cos(pk*2*M_PI), yc-1.1*r*sin(pk*2*M_PI) );
              cr->stroke();
            }
            cr->restore();
            // end n_out
            // numerical text:
            if( gain > 0 ){
              cr->save();
              cr->set_font_size( r );
              cr->set_source_rgb( 0, 0, 0 );
              cr->move_to( xtxt, ytxt );
              char stmp[100];
              sprintf(stmp,"%.2g",20*log10(gain));
              cr->show_text( stmp );
              cr->restore();
            }
          }
        }
        // output gains:
        for( unsigned int kout=0;kout < mm->get_num_outputs(); kout++){
          //DEBUG(kout);
          double x1 = ratio*(-1+0.03)*scx;
          double xtxt = ratio*(-1+0.06)*scx;
          double x2 = ratio*(-1+0.97)*scx;
          double y1 = (kout+0.6)*scy;
          double y2 = (kout+0.97)*scy;
          double ytxt = (kout+0.94)*scy;
          double gain = mm->get_out_gain( kout );
          //DEBUG(gain);
          //double gain = 0.5;
          cr->save();
          cr->set_source_rgb( std::min(1.0,gain), std::min(1.0,(gain>0)-gain), 0 );
          cr->move_to( x1, y1 );
          cr->line_to( x1, y2 );
          cr->line_to( x2, y2 );
          cr->line_to( x2, y1 );
          cr->fill();
          cr->restore();
          cr->save();
          cr->set_font_size( r );
          cr->set_source_rgb( 0, 0, 0 );
          cr->move_to( xtxt, ytxt );
          char stmp[100];
          sprintf(stmp,"%.2g",20*log10(gain));
          cr->show_text( stmp );
          cr->restore();
        }
        // end output gains
        // output selection:
        cr->save();
        cr->set_source_rgba( 0.6, 0.6, 1, 0.2 );
        for( unsigned int kout=0;kout < mm->get_num_outputs(); kout++){
          if( mm->get_select_out( kout ) ){
            cr->move_to( -0.4*ratio, kout*scy );
            cr->line_to( ratio, kout*scy );
            cr->line_to( ratio, (kout+1)*scy );
            cr->line_to( -0.4*ratio, (kout+1)*scy );
            cr->fill();
          }
        }
        cr->restore();
        // input selection:
        cr->save();
        cr->set_source_rgba( 1, 0.6, 0.6, 0.2 );
        for( unsigned int k=0;k < mm->get_num_inputs(); k++){
          if( mm->get_select_in( k ) ){
            double x1 = ratio*k*scx;
            double x2 = ratio*(k+1)*scx;
            cr->move_to( x1-0.4, -0.4 );
            cr->line_to( x1, 0.0 );
            cr->line_to( x1, 1.0 );
            cr->line_to( x2, 1.0 );
            cr->line_to( x2, 0.0 );
            cr->line_to( x2-0.4, -0.4 );
            cr->fill();
          }
        }
        cr->restore();
        // input mute:
        cr->save();
        cr->set_source_rgba( 1, 1, 0, 0.2 );
        for( unsigned int k=0;k < mm->get_num_inputs(); k++){
          if( mm->get_mute( k )==0 ){
            double x1 = ratio*k*scx;
            double x2 = ratio*(k+1)*scx;
            cr->move_to( x1-0.4, -0.4 );
            cr->line_to( x1, 0.0 );
            cr->line_to( x1, 1.0 );
            cr->line_to( x2, 1.0 );
            cr->line_to( x2, 0.0 );
            cr->line_to( x2-0.4, -0.4 );
            cr->fill();
          }
        }
        cr->restore();
      }
      pthread_mutex_unlock( &mutex );

    }

  return true;
}


bool mixergui_t::on_timeout()
{
  bool lmod;
  pthread_mutex_lock( &mutex );
  lmod = modified;
  if( mm )
    lmod = lmod || mm->ismodified();
  modified = false;
  pthread_mutex_unlock( &mutex );
  // force our program to redraw the entire clock.
  Glib::RefPtr<Gdk::Window> win = get_window();
  if (win && lmod){
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
