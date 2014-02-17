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

using namespace HoSGUI;

void lo_err_handler_cb(int num, const char *msg, const char *where) 
{
  std::cerr << "lo error " << num << ": " << msg << " (" << where << ")\n";
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
  void on_menu_file_save();
  void on_menu_file_saveas();
  void on_menu_file_close();
  void on_menu_file_quit();

  //Child widgets:
  Gtk::VPaned vbox;
  Gtk::Box m_Box;
  mixergui_t mixer;
  MM::namematrix_t* mm;
  std::string mm_filename;

  Glib::RefPtr<Gtk::UIManager> m_refUIManager;
  Glib::RefPtr<Gtk::ActionGroup> m_refActionGroup;
  Glib::RefPtr<Gtk::RadioAction> m_refChoiceOne, m_refChoiceTwo;
};

mm_window_t::mm_window_t()
  : m_Box(Gtk::ORIENTATION_VERTICAL),mm(NULL)
{
  set_title("main menu example");
  set_default_size(200, 200);
  mixer.hdspmm_new(NULL);
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
  mixer.hdspmm_destroy();
}

void mm_window_t::on_menu_file_new()
{
  mixer.hdspmm_destroy();
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
    mixer.hdspmm_new(mm);
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
    mixer.hdspmm_destroy();
    if( mm )
      delete mm;
    mm =  NULL;
    try{
      mm = MM::load(filename);
      mixer.hdspmm_new(mm);
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
