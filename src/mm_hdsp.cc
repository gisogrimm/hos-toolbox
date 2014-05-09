/**
   \file mm_hdsp.cc
   \brief Implementation of RME HDSP matrix mixer interface
   \ingroup apphos
   \author Giso Grimm
   \date 2011
   
   "mm_{hdsp,file,gui,midicc}" is a set of programs to control the matrix
   mixer of an RME hdsp compatible sound card using XML formatted files
   or a MIDI controller, and to visualize the mixing matrix.

   \section license License (GPL)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

*/
#include "libhos_gainmatrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <math.h>
#include <alsa/asoundlib.h>

void osc_err_handler_cb(int num, const char *msg, const char *where) 
{
  std::cerr << "lo error " << num << ": " << msg << " (" << where << ")\n";
}

namespace MM {

/**
\ingroup apphos
*/
class mm_hdsp_t
{
public:
  mm_hdsp_t(const std::string& osc_server_addr,const std::string& osc_server_port,const std::string& osc_dest_addr);
  ~mm_hdsp_t();
  void run();
private:
  static int hdspmm_new(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int hdspmm_destroy(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  void hdspmm_destroy();
  void hdspmm_new(unsigned int kout, unsigned int kin);
  void upload();
  MM::lo_matrix_t* mm;
  bool modified;
  pthread_mutex_t mutex;
  bool b_exit;
  lo_server_thread lost;
  lo_address addr;
};

}

using namespace MM;

void mm_hdsp_t::run()
{
  while( !b_exit ){
    usleep( 50000 );
    pthread_mutex_lock( &mutex );
    if( mm && mm->ismodified() ){
      pthread_mutex_unlock( &mutex );
      upload();
      pthread_mutex_lock( &mutex );
    }
    pthread_mutex_unlock( &mutex );
  }
}

mm_hdsp_t::mm_hdsp_t(const std::string& osc_server_addr,const std::string& osc_server_port,const std::string& osc_dest_addr)
  : mm(NULL),
    modified(true),
    b_exit(false)
{
  pthread_mutex_init( &mutex, NULL );
  if( osc_server_addr.size() )
    lost = lo_server_thread_new_multicast( osc_server_addr.c_str(), 
					   osc_server_port.c_str(), 
					   osc_err_handler_cb );
  else
    lost = lo_server_thread_new( osc_server_port.c_str(), osc_err_handler_cb );
  addr = lo_address_new_from_url( osc_dest_addr.c_str() );
  lo_address_set_ttl( addr, 1 );
  lo_server_thread_add_method(lost,"/hdspmm/backend_new","ii",mm_hdsp_t::hdspmm_new,this);
  lo_server_thread_add_method(lost,"/hdspmm/backend_quit","",mm_hdsp_t::hdspmm_destroy,this);
  lo_server_thread_start( lost );
}

mm_hdsp_t::~mm_hdsp_t()
{
  if( mm )
    delete mm;
  lo_server_thread_stop(lost);
  lo_server_thread_free(lost);
  lo_address_free( addr );
  pthread_mutex_destroy( &mutex );
}

void mm_hdsp_t::upload()
{
  //DEBUG(mm);
  pthread_mutex_lock( &mutex );
  if( mm ){
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_id_set_name(id, "Mixer");
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_HWDEP);
    snd_ctl_elem_id_set_device(id, 0);
    snd_ctl_elem_id_set_index(id, 0);
    snd_ctl_elem_value_t *ctl;
    snd_ctl_elem_value_alloca(&ctl);
    snd_ctl_elem_value_set_id(ctl, id);
    snd_ctl_t *handle;
    int err;
    if ((err = snd_ctl_open(&handle, "hw:DSP", SND_CTL_NONBLOCK)) < 0) {
      fprintf(stderr, "Alsa error: %s\n", snd_strerror(err));
      return;
    }
    for( unsigned int kin=0;kin<mm->get_num_inputs();kin++){
      for( unsigned int kout=0;kout<mm->get_num_outputs();kout++){
        std::vector<unsigned int> inports = mm->get_inports(kin);
        std::vector<unsigned int> outports = mm->get_outports(kout);
        for( unsigned int kinsub=0;kinsub < inports.size();kinsub++){
          for( unsigned int koutsub=0;koutsub < outports.size();koutsub++){
            unsigned int src = inports[kinsub];
            unsigned int dest = outports[koutsub];
            double val = mm->channelgain(kout, koutsub, kin, kinsub);
            snd_ctl_elem_value_set_integer(ctl, 0, src);
            snd_ctl_elem_value_set_integer(ctl, 1, dest);
            snd_ctl_elem_value_set_integer(ctl, 2, (int)(32767*std::min(1.0,std::max(0.0,val))));
            if ((err = snd_ctl_elem_write(handle, ctl)) < 0) {
              fprintf(stderr, "Alsa error: %s\n", snd_strerror(err));
            }
          }
        }
      }
    }
    snd_ctl_close(handle);
  }
  pthread_mutex_unlock( &mutex );
}

int mm_hdsp_t::hdspmm_new(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  //DEBUG(path);
  if( (argc == 2) && (types[0] == 'i') && (types[1] == 'i') ){
    ((mm_hdsp_t*)user_data)->hdspmm_new(argv[0]->i,argv[1]->i);
    return 0;
  }
  return 1;
};

int mm_hdsp_t::hdspmm_destroy(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  //DEBUG(path);
  ((mm_hdsp_t*)user_data)->hdspmm_destroy();
  return 0;
};

void mm_hdsp_t::hdspmm_destroy()
{
  //DEBUG("destroy");
  pthread_mutex_lock( &mutex );
  if( mm )
    delete mm;
  mm = NULL;
  modified = true;
  pthread_mutex_unlock( &mutex );
  //b_exit = true;
}

void mm_hdsp_t::hdspmm_new(unsigned int kout, unsigned int kin)
{
  //DEBUG("new");
  //DEBUG(kout);
  pthread_mutex_lock( &mutex );
  if( mm )
    delete mm;
  mm = new MM::lo_matrix_t(kout,kin,lost,addr);
  modified = true;
  pthread_mutex_unlock( &mutex );
  lo_send( addr, "/hdspmm/backend_new", "ii", (int)kout, (int)kin);
  upload();
}


int main(int argc, char** argv)
{
  try{
    //todo: midi device command line, other pars optional, midi mapping
    std::string osc_server_addr("");
    std::string osc_server_port("6976");
    std::string osc_dest_url("osc.udp://239.255.1.7:6978/");
    if( (argc == 2) && (strcmp(argv[1],"-h")==0) ){
      std::cerr << "Usage: mm_hdsp [ <osc_server_addr> <osc_server_port> <osc_dest_url> ]\n";
      return 1;
    }
    // setup OSC server:
    if( argc > 1 )
      osc_server_addr = argv[1];
    if( argc > 2 )
      osc_server_port = argv[2];
    if( argc > 3 )
      osc_dest_url = argv[3];
    mm_hdsp_t m(osc_server_addr,osc_server_port,osc_dest_url);
    m.run();
    return 0;
  }
  catch(const std::exception& e){
    std::cerr << e.what() << std::endl;
    exit(1);
  }
  catch(const char* e){
    std::cerr << e << std::endl;
    exit(1);
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
