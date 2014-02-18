/**
   \file mm_gui.cc
   \brief Matrix mixer visualization
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
#include "hosgui_mixer.h"
#include "defs.h"
#include "libhos_midi_ctl.h"

using namespace HoSGUI;

namespace MM {

  /**
     \ingroup apphos
  */
  class mm_midicc_t : public midi_ctl_t, public MM::observer_t
  {
  public:
    mm_midicc_t();
    ~mm_midicc_t();
    float midi2gain(unsigned int v);
    unsigned int gain2midi(float v);
    float midi2pan(unsigned int v);
    unsigned int pan2midi(float v);
    float midi2pan2(unsigned int v);
    unsigned int pan22midi(float v);
    //void create_hos_mapping();
    //void run();
    void hdspmm_destroy();
    void hdspmm_new(MM::namematrix_t* mm);
  private:
    virtual void emit_event(int channel, int param, int value);

    void upload();

    void start_updt_service();
    void stop_updt_service();
    static void * updt_service(void *);
    void updt_service();

    MM::namematrix_t* mm;
    bool modified;
    pthread_mutex_t mutex;

    bool b_shift1;
    bool b_shift2;
    unsigned int selected_out;
    //bool b_exit;

    bool b_run_service;
    pthread_t srv_thread;
  };


  /**
     \ingroup apphos
  */
  class mm_hdsp_t : public MM::observer_t
  {
  public:
    mm_hdsp_t();
    ~mm_hdsp_t();
    void hdspmm_destroy();
    void hdspmm_new(MM::namematrix_t* m_);
  private:
    void upload();
    MM::namematrix_t* mm;
    bool modified;
    pthread_mutex_t mutex;
    //bool b_exit;
    pthread_t srv_thread;
    bool b_run_service;
    void start_updt_service();
    void stop_updt_service();
    static void * updt_service(void *);
    void updt_service();
  };

}

using namespace MM;


void * mm_hdsp_t::updt_service(void* h)
{
  ((mm_hdsp_t*)h)->updt_service();
  return NULL;
}

void mm_hdsp_t::start_updt_service()
{
  if( b_run_service )
    return;
  b_run_service = true;
  int err = pthread_create( &srv_thread, NULL, &mm_hdsp_t::updt_service, this);
  if( err < 0 )
    throw "pthread_create failed";
}

void mm_hdsp_t::stop_updt_service()
{
  if( !b_run_service )
    return;
  b_run_service = false;
  pthread_join( srv_thread, NULL );
}

void mm_hdsp_t::updt_service()
{
  while( b_run_service ){
    usleep( 50000 );
    pthread_mutex_lock( &mutex );
    if( mm && mm->ismodified(this) ){
      pthread_mutex_unlock( &mutex );
      upload();
      pthread_mutex_lock( &mutex );
    }
    pthread_mutex_unlock( &mutex );
  }
}

mm_hdsp_t::mm_hdsp_t()
  : mm(NULL),
    modified(true),
    b_run_service(false)
{
  pthread_mutex_init( &mutex, NULL );
  start_updt_service();
}

mm_hdsp_t::~mm_hdsp_t()
{
  stop_updt_service();
  pthread_mutex_destroy( &mutex );
}

void mm_hdsp_t::upload()
{
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


void mm_hdsp_t::hdspmm_destroy()
{
  pthread_mutex_lock( &mutex );
  mm = NULL;
  modified = true;
  pthread_mutex_unlock( &mutex );
  //b_exit = true;
}

void mm_hdsp_t::hdspmm_new(MM::namematrix_t* mm_)
{
  pthread_mutex_lock( &mutex );
  mm = mm_;
  if( mm )
    mm->add_observer(this);
  modified = true;
  pthread_mutex_unlock( &mutex );
  upload();
}


mm_midicc_t::mm_midicc_t()
  : midi_ctl_t("mm_mididcc"),
    mm(NULL),
    modified(true),
    b_shift1(false),
    b_shift2(false),
    selected_out(0),
    b_run_service(false)
{
  pthread_mutex_init( &mutex, NULL );
  start_service();
  start_updt_service();
}

void * mm_midicc_t::updt_service(void* h)
{
  ((mm_midicc_t*)h)->updt_service();
  return NULL;
}

void mm_midicc_t::start_updt_service()
{
  if( b_run_service )
    return;
  b_run_service = true;
  int err = pthread_create( &srv_thread, NULL, &mm_midicc_t::updt_service, this);
  if( err < 0 )
    throw "pthread_create failed";
}

void mm_midicc_t::stop_updt_service()
{
  if( !b_run_service )
    return;
  b_run_service = false;
  pthread_join( srv_thread, NULL );
}

void mm_midicc_t::updt_service()
{
  while(b_run_service ){
    usleep(700000);
    pthread_mutex_lock( &mutex );
    if( mm && mm->ismodified(this) ){
      pthread_mutex_unlock( &mutex );
      upload();
      pthread_mutex_lock( &mutex );
    }
    pthread_mutex_unlock( &mutex );
  }
}

mm_midicc_t::~mm_midicc_t()
{
  stop_updt_service();
  stop_service();
  pthread_mutex_destroy( &mutex );
}

float mm_midicc_t::midi2gain(unsigned int v)
{
  double db(v/90.0);
  db *= db;
  return db;
}

unsigned int mm_midicc_t::gain2midi(float v)
{
  double midi(v);
  midi = 90.0*sqrt(midi);
  return (unsigned int)(std::min(127.0,std::max(0.0,midi+0.5)));
}


float mm_midicc_t::midi2pan(unsigned int v)
{
  if( v < 63 )
    return (float)v/126.0;
  if( v > 65 )
    return 0.5+((float)v-65.0)/126.0;
  return 0.5;
}

unsigned int mm_midicc_t::pan2midi(float v)
{
  v = std::min(1.0f,std::max(0.0f,v));
  if( v < 0.5 )
    return 126.0*v;
  if( v > 0.5 )
    return 65+(126.0*(v-0.5));
  return 64;
}


float mm_midicc_t::midi2pan2(unsigned int v)
{
  if( v < 63 )
    return 0.5*(float)v/126.0;
  if( v > 65 )
    return 0.5*(0.5+((float)v-65.0)/126.0);
  return 0.25;
}

unsigned int mm_midicc_t::pan22midi(float v)
{
  v *= 2;
  v = std::min(1.0f,std::max(0.0f,v));
  if( v < 0.5 )
    return 126.0*v;
  if( v > 0.5 )
    return 65+(126.0*(v-0.5));
  return 64;
}

void mm_midicc_t::emit_event(int channel, int param, int value)
{
  if( channel != 0 )
    return;
  pthread_mutex_lock( &mutex );
  if( mm ){
    unsigned int mod_in = 7*b_shift1+14*b_shift2;
    switch( param ){
    case 7 : // output gain
      mm->set_out_gain( selected_out, midi2gain( value ) );
      break;
    case 0 :
    case 1 :
    case 2 :
    case 3 :
    case 4 :
    case 5 :
    case 6 :
      mm->set_gain( selected_out, param+mod_in, midi2gain( value ) );
      break;
    case 8 :
    case 9 :
    case 10 :
    case 11 :
    case 12 :
    case 13 :
    case 14 :
      mm->set_mute( param+mod_in-8, (value == 0) );
      break;
    case 24 :
    case 25 :
    case 26 :
    case 27 :
    case 28 :
    case 29 :
    case 30 :
      if( mm->get_n_out( selected_out ) == 2 )
        mm->set_pan( selected_out, param-24+mod_in, midi2pan2( value ));
      else
        mm->set_pan( selected_out, param-24+mod_in, midi2pan( value ));
      break;
    case 16 :
    case 17 :
    case 18 :
    case 19 :
    case 20 :
    case 21 :
    case 22 :
      selected_out = param-16;
      pthread_mutex_unlock( &mutex );
      upload();
      pthread_mutex_lock( &mutex );
      break;
    case 23 :
      b_shift2 = (value>0);
      pthread_mutex_unlock( &mutex );
      upload();
      pthread_mutex_lock( &mutex );
      break;
    case 15 :
      b_shift1 = (value>0);
      pthread_mutex_unlock( &mutex );
      upload();
      pthread_mutex_lock( &mutex );
      break;
    }
  }
  pthread_mutex_unlock( &mutex );
}

void mm_midicc_t::upload()
{
  pthread_mutex_lock( &mutex );
  if( mm ){
    unsigned int mod_in = 7*b_shift1+14*b_shift2;
    for( unsigned int k=0;k<mm->get_num_outputs();k++)
      mm->set_select_out( k, k==selected_out );
    for( unsigned int k=0;k<mm->get_num_inputs();k++)
      mm->set_select_in( k, (k>=mod_in) && (k<mod_in+7) );
    // output gain:
    if( selected_out < mm->get_num_outputs() )
      send_midi( 0, 7, gain2midi( mm->get_out_gain( selected_out ) ) );
    else
      send_midi( 0, 7, 0 );
    // matrix gains:
    for( unsigned int k=0;k<7;k++){
      unsigned int kin = k+mod_in;
      if( kin < mm->get_num_inputs() )
        send_midi(0, k, gain2midi( mm->get_gain( selected_out, kin ) ) );
      else
        send_midi(0, k, 0 );
    }
    // matrix pans:
    for( unsigned int k=24;k<31;k++){
      unsigned int kin = k-24+mod_in;
      if( kin < mm->get_num_inputs() )
        if( mm->get_n_out( selected_out ) == 2 )
          send_midi(0, k, pan22midi( mm->get_pan( selected_out, kin ) ) );
        else
          send_midi(0, k, pan2midi( mm->get_pan( selected_out, kin ) ) );
      else
        send_midi(0, k, 0 );
    }
    // selection:
    for( unsigned int k=16;k<23;k++){
      if( k-16 == selected_out )
        send_midi(0, k, 127 );
      else
        send_midi(0, k, 0 );
    }
    for( unsigned int k=8;k<15;k++){
      unsigned int kin = k+mod_in-8;
      if( kin < mm->get_num_inputs() )
        send_midi(0, k, 127*(mm->get_mute( kin )==0) );
      else
        send_midi(0, k, 0 );
    }
    send_midi( 0, 23, 127*b_shift2 );
    send_midi( 0, 15, 127*b_shift2 );
  }else{
    for( unsigned int k=0;k<64;k++ ){
      send_midi( 0, k, 0 );
    }
  }
  pthread_mutex_unlock( &mutex );
}

void mm_midicc_t::hdspmm_destroy()
{
  pthread_mutex_lock( &mutex );
  mm = NULL;
  modified = true;
  pthread_mutex_unlock( &mutex );
  //b_exit = true;
}

void mm_midicc_t::hdspmm_new(MM::namematrix_t* mm_)
{
  pthread_mutex_lock( &mutex );
  mm = mm_;
  if( mm )
    mm->add_observer(this);
  modified = true;
  pthread_mutex_unlock( &mutex );
  upload();
}

class mm_window_t : public Gtk::Window
{
public:
  mm_window_t();
  virtual ~mm_window_t();

protected:
  //Signal handlers:
  void on_menu_file_new();
  void on_menu_file_open();
  void on_menu_file_reload();
  void on_menu_file_save();
  void on_menu_file_saveas();
  void on_menu_file_close();
  void on_menu_file_quit();

  void mm_destroy();
  void mm_new();

  //Child widgets:
  Gtk::VPaned vbox;
  Gtk::Box m_Box;

  mixergui_t mixer;
  mm_midicc_t midi;
  mm_hdsp_t hdsp;

  MM::namematrix_t* mm;

  std::string mm_filename;

  Glib::RefPtr<Gtk::UIManager> m_refUIManager;
  Glib::RefPtr<Gtk::ActionGroup> m_refActionGroup;
  Glib::RefPtr<Gtk::RadioAction> m_refChoiceOne, m_refChoiceTwo;
};

void mm_window_t::mm_destroy()
{
  mixer.hdspmm_destroy();
  midi.hdspmm_destroy();
  hdsp.hdspmm_destroy();
}

void mm_window_t::mm_new()
{
  mixer.hdspmm_new(mm);
  midi.hdspmm_new(mm);
  hdsp.hdspmm_new(mm);
}


mm_window_t::mm_window_t()
  : m_Box(Gtk::ORIENTATION_VERTICAL),mm(NULL)
{
  set_title("main menu example");
  set_default_size(200, 200);
  add(vbox);
  vbox.add(m_Box); // put a MenuBar at the top of the box and other stuff below it.
  vbox.add(mixer);
  //Create actions for menus and toolbars:
  m_refActionGroup = Gtk::ActionGroup::create();
  //File|New sub menu:
  //File menu:
  m_refActionGroup->add(Gtk::Action::create("FileMenu", "File"));
  m_refActionGroup->add(Gtk::Action::create("FileNew", Gtk::Stock::NEW),
                        sigc::mem_fun(*this, &mm_window_t::on_menu_file_new));
  m_refActionGroup->add(Gtk::Action::create("FileOpen",Gtk::Stock::OPEN),
                        sigc::mem_fun(*this, &mm_window_t::on_menu_file_open));
  m_refActionGroup->add(Gtk::Action::create("FileReload",Gtk::Stock::REVERT_TO_SAVED),
                        sigc::mem_fun(*this, &mm_window_t::on_menu_file_reload));
  m_refActionGroup->add(Gtk::Action::create("FileSave",Gtk::Stock::SAVE),
                        sigc::mem_fun(*this, &mm_window_t::on_menu_file_save));
  m_refActionGroup->add(Gtk::Action::create("FileSaveAs",Gtk::Stock::SAVE_AS),
                        sigc::mem_fun(*this, &mm_window_t::on_menu_file_saveas));
  m_refActionGroup->add(Gtk::Action::create("FileClose",Gtk::Stock::CLOSE),
                        sigc::mem_fun(*this, &mm_window_t::on_menu_file_close));
  m_refActionGroup->add(Gtk::Action::create("FileQuit", Gtk::Stock::QUIT),
                        sigc::mem_fun(*this, &mm_window_t::on_menu_file_quit));
  //Edit menu:
  //m_refActionGroup->add(Gtk::Action::create("EditMenu", "Edit"));
  //m_refActionGroup->add(Gtk::Action::create("EditCopy", Gtk::Stock::COPY),
  //                      sigc::mem_fun(*this, &mm_window_t::on_menu_others));
  //m_refActionGroup->add(Gtk::Action::create("EditPaste", Gtk::Stock::PASTE),
  //                      sigc::mem_fun(*this, &mm_window_t::on_menu_others));
  //m_refActionGroup->add(Gtk::Action::create("EditSomething", "Something"),
  //                      Gtk::AccelKey("<control><alt>S"),
  //                      sigc::mem_fun(*this, &mm_window_t::on_menu_others));
  m_refUIManager = Gtk::UIManager::create();
  m_refUIManager->insert_action_group(m_refActionGroup);
  add_accel_group(m_refUIManager->get_accel_group());
  //Layout the actions in a menubar and toolbar:
  Glib::ustring ui_info = 
    "<ui>"
    "  <menubar name='MenuBar'>"
    "    <menu action='FileMenu'>"
    "      <menuitem action='FileNew'/>"
    "      <menuitem action='FileOpen'/>"
    "      <menuitem action='FileReload'/>"
    "      <separator/>"
    "      <menuitem action='FileSave'/>"
    "      <menuitem action='FileSaveAs'/>"
    "      <separator/>"
    "      <menuitem action='FileClose'/>"
    "      <menuitem action='FileQuit'/>"
    "    </menu>"
    "  </menubar>"
    "  <toolbar  name='ToolBar'>"
    "    <toolitem action='FileNew'/>"
    "    <toolitem action='FileOpen'/>"
    "    <toolitem action='FileReload'/>"
    "    <toolitem action='FileSave'/>"
    "    <toolitem action='FileSaveAs'/>"
    "    <toolitem action='FileClose'/>"
    "    <toolitem action='FileQuit'/>"
    "  </toolbar>"
    "</ui>";
  try{
    m_refUIManager->add_ui_from_string(ui_info);
  }
  catch(const Glib::Error& ex){
    std::cerr << "building menus failed: " <<  ex.what();
  }
  //Get the menubar and toolbar widgets, and add them to a container widget:
  Gtk::Widget* pMenubar = m_refUIManager->get_widget("/MenuBar");
  if(pMenubar)
    m_Box.pack_start(*pMenubar, Gtk::PACK_SHRINK);
  Gtk::Widget* pToolbar = m_refUIManager->get_widget("/ToolBar") ;
  if(pToolbar)
    m_Box.pack_start(*pToolbar, Gtk::PACK_SHRINK);
  show_all_children();
}

mm_window_t::~mm_window_t()
{
}

void mm_window_t::on_menu_file_quit()
{
  hide(); //Closes the main window to stop the app->run().
}

void mm_window_t::on_menu_file_close()
{
  mm_destroy();
}

void mm_window_t::on_menu_file_new()
{
  mm_destroy();
  Gtk::SpinButton e_inputs;
  Gtk::SpinButton e_outputs;
  e_inputs.set_range(1,128);
  e_outputs.set_range(1,128);
  e_inputs.set_increments(1,10);
  e_outputs.set_increments(1,10);
  e_inputs.set_value(5);
  e_outputs.set_value(5);
  //Gtk::Grid grid;
  //grid.attach(e_inputs,0,0,1,1);
  //grid.attach(e_outputs,0,1,1,1);
  Gtk::Dialog dialog("New mixing matrix",*this);
  Gtk::Box* box(dialog.get_content_area());
  box->add(e_inputs);
  box->add(e_outputs);
  dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
  dialog.show_all();
  int result = dialog.run();
  if( result == Gtk::RESPONSE_OK){
    uint32_t num_in(e_inputs.get_value_as_int());
    uint32_t num_out(e_outputs.get_value_as_int());
    if( mm )
      delete mm;
    mm =  NULL;
    mm = new MM::namematrix_t(num_out,num_in);
    mm_filename = "";
    mm_new();
  }
}

void mm_window_t::on_menu_file_save()
{
  if( mm && (mm_filename.size()>0))
    MM::save(mm,mm_filename);
}

void mm_window_t::on_menu_file_saveas()
{
  if( mm ){
    Gtk::FileChooserDialog dialog("Please choose a destination",
                                  Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_transient_for(*this);
    //Add response buttons the the dialog:
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);
    //Add filters, so that only certain file types can be selected:
    Glib::RefPtr<Gtk::FileFilter> filter_mm = Gtk::FileFilter::create();
    filter_mm->set_name("Matrix mixer files");
    filter_mm->add_pattern("*.mm");
    dialog.add_filter(filter_mm);
    Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);
    //Show the dialog and wait for a user response:
    int result = dialog.run();
    //Handle the response:
    if( result == Gtk::RESPONSE_OK){
      //Notice that this is a std::string, not a Glib::ustring.
      std::string filename = dialog.get_filename();
      if( filename.find(".mm") == std::string::npos )
        filename += ".mm";
      if( filename.size() > 0 ){
        mm_filename = filename;
        MM::save(mm,mm_filename);
      }
    }
  }
}

void mm_window_t::on_menu_file_reload()
{
  if( mm )
    delete mm;
  mm =  NULL;
  try{
    mm = MM::load(mm_filename);
    mm_new();
  }
  catch( const std::exception& e){
    std::cerr << "Error while loading file: " <<  e.what() << std::endl;
  }
}

void mm_window_t::on_menu_file_open()
{
  Gtk::FileChooserDialog dialog("Please choose a file",
                                Gtk::FILE_CHOOSER_ACTION_OPEN);
  dialog.set_transient_for(*this);
  //Add response buttons the the dialog:
  dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);
  //Add filters, so that only certain file types can be selected:
  Glib::RefPtr<Gtk::FileFilter> filter_mm = Gtk::FileFilter::create();
  filter_mm->set_name("Matrix mixer files");
  filter_mm->add_pattern("*.mm");
  dialog.add_filter(filter_mm);
  Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
  filter_any->set_name("Any files");
  filter_any->add_pattern("*");
  dialog.add_filter(filter_any);
  //Show the dialog and wait for a user response:
  int result = dialog.run();
  //Handle the response:
  if( result == Gtk::RESPONSE_OK){
    //Notice that this is a std::string, not a Glib::ustring.
    std::string filename = dialog.get_filename();
    mm_destroy();
    if( mm )
      delete mm;
    mm =  NULL;
    try{
      mm = MM::load(filename);
      mm_new();
      mm_filename = filename;
    }
    catch( const std::exception& e){
      std::cerr << "Error while loading file: " <<  e.what() << std::endl;
    }
  }
}

int main(int argc, char** argv)
{
  Gtk::Main kit(argc, argv);
  
  //Gtk::Window win;
  mm_window_t win;
  win.set_title("RME hdsp matrix mixer");
  win.set_default_size(800,600);
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
