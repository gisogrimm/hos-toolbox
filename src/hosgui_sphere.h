/**
   \file hosgui_sphere.h
   \ingroup apphos
   \brief Trajectory visualization
   \author Giso Grimm
   \date 2011

   \section license License (GPL)

   Copyright (C) 2011 Giso Grimm

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2
   of the License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
   USA.

*/
/**
 */
#ifndef HOS_SPHEREGUI_H
#define HOS_SPHEREGUI_H

#include <cairomm/context.h>
#include <gtkmm.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <lo/lo.h>

namespace HoSGUI {

  class pos_t {
  public:
    pos_t(double r_, double phi_) : r(r_), phi(phi_){};
    pos_t() : r(1.0), phi(0.0){};
    double get_x();
    double get_y();
    double r;
    double phi;
  };

  class pos_tail_t {
  public:
    static int addpoint(const char* path, const char* types, lo_arg** argv,
                        int argc, lo_message msg, void* user_data);
    pos_tail_t(const std::string& pAddr, lo_server_thread& lost,
               unsigned int k);
    ~pos_tail_t();
    void addpoint(float r, float phi);
    void set_rotate(double r);
    void draw(const Cairo::RefPtr<Cairo::Context>& cr);

  private:
    std::vector<pos_t> tail;
    std::vector<pos_t>::iterator tailp;
    unsigned int k_;
    pthread_mutex_t mutex;
    double rotate;
  };

  class visualize_t : public Gtk::DrawingArea {
  public:
    visualize_t(const std::vector<std::string>& paddr, lo_server_thread& l);
    virtual ~visualize_t();
    void set_rotate(double r);

  protected:
    // Override default signal handler:
    virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
    bool on_timeout();
    std::vector<pos_tail_t*> vTail;
    lo_server_thread& lost;
    std::vector<std::string> names;
    double rotate;
  };

} // namespace HoSGUI

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
