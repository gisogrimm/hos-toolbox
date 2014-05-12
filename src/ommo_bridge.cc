#include "osc_helper.h"
#include <string>
#include <vector>
#include <libxml++/libxml++.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <alsa/seq_event.h>

#include "libhos_midi_ctl.h"

#define DEBUG(x) std::cerr << __FILE__ << ":" << __LINE__ << " " << #x << "=" << x << std::endl
#define MIDISC 0.0078740157480314959537182f

class midi_client_t : public midi_ctl_t {
public:
  midi_client_t(const std::string& name);
  ~midi_client_t();
  void add_method(uint8_t channel, uint8_t param, lo_method_handler lh, void* data);
protected:
  void emit_event(int channel, int param, int value);
private:
  class handler_t {
  public:
    handler_t(uint8_t ch,uint8_t par,lo_method_handler lh,void* data);
    void emit(uint8_t ch,uint8_t par,uint8_t value);
  private:
    lo_method_handler lh_;
    void* data_;
    uint8_t ch_;
    uint8_t par_;
  };
  std::vector<handler_t> vH;
};

midi_client_t::handler_t::handler_t(uint8_t ch,uint8_t par,lo_method_handler lh,void* data)
  : lh_(lh),data_(data),ch_(ch),par_(par)
{
}

void midi_client_t::handler_t::emit(uint8_t ch,uint8_t par,uint8_t value)
{
  if( (ch==ch_) && (par==par_) ){
    // emit message
    lo_message msg(lo_message_new());
    lo_message_add_float(msg,MIDISC*value);
    lh_("/midicc",lo_message_get_types(msg),lo_message_get_argv(msg),lo_message_get_argc(msg),msg,data_);
    lo_message_free(msg);
  }
}

void midi_client_t::add_method(uint8_t channel, uint8_t param, lo_method_handler lh, void* data)
{
  vH.push_back(midi_client_t::handler_t(channel,param,lh,data));
}

midi_client_t::midi_client_t(const std::string& name)
  : midi_ctl_t(name)
{
}

midi_client_t::~midi_client_t()
{
}

void midi_client_t::emit_event(int channel, int param, int value)
{
  for(std::vector<midi_client_t::handler_t>::iterator it=vH.begin();it!=vH.end();++it)
    it->emit(channel,param,value);
}

class osc_destination_t {
public:
  enum arg_mode_t {
    source, replace, reorder
  };
  osc_destination_t(const std::string& target,const std::string& path, const std::vector<unsigned int>& argmap, arg_mode_t argmode);
  ~osc_destination_t();
  static int event_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  int event_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message in_msg);
  void set_valmap(float v1, float v2);
  void add_float(float f);
  void add_int32(int32_t f);
  void add_string(const std::string& s);
  void set_condition(float val);
  void remove_condition();
  bool lo_message_copy_arg(lo_message dest, lo_message src, int arg, bool x_valmap);
private:
  lo_address target_;
  std::string path_;
  std::vector<unsigned int> argmap_;
  arg_mode_t argmode_;
  lo_message own_msg;
  bool use_condition;
  float condition_val;
  bool use_valmap;
  float val_min;
  float val_scale;
};

void osc_destination_t::set_valmap(float v1, float v2)
{
  val_min = v1;
  //val_scale = std::min(1.0,std::max(0.0,1.0/(v2-v1)));
  val_scale = (v2-v1);
  use_valmap = true;
}

void osc_destination_t::set_condition(float val)
{
  use_condition = true;
  condition_val = val;
}

void osc_destination_t::remove_condition()
{
  use_condition = false;
}

void osc_destination_t::add_float(float f)
{
  lo_message_add_float(own_msg,f);
}

void osc_destination_t::add_int32(int32_t i)
{
  lo_message_add_float(own_msg,i);
}

void osc_destination_t::add_string(const std::string& s)
{
  lo_message_add_string(own_msg,s.c_str());
}

bool osc_destination_t::lo_message_copy_arg(lo_message dest, lo_message src, int arg, bool x_valmap)
{
  int argc(lo_message_get_argc(src));
  const char* types(lo_message_get_types(src));
  lo_arg** argv(lo_message_get_argv(src));
  float val;
  if( (arg >= 0) && (arg < argc) ){
    switch( types[arg] ){
    case 'i' :
      lo_message_add_int32(dest,argv[arg]->i);
      return true;
    case 'f' :
      val = argv[arg]->f;
      if( use_valmap && x_valmap )
        val = val_scale*val+val_min;
      lo_message_add_float(dest,val);
      return true;
    case 's' :
      lo_message_add_string(dest,&(argv[arg]->s));
      return true;
    }
  }
  return false;
}

osc_destination_t::osc_destination_t(const std::string& target,const std::string& path, const std::vector<unsigned int>& argmap, arg_mode_t argmode)
  : target_(lo_address_new_from_url(target.c_str())),
    path_(path),
    argmap_(argmap),
    argmode_(argmode),
    own_msg(lo_message_new()),
    use_condition(false),
    condition_val(0),
    use_valmap(false)
{
  lo_address_set_ttl(target_,1);
}

osc_destination_t::~osc_destination_t()
{
  lo_message_free(own_msg);
  lo_address_free(target_);
}

int osc_destination_t::event_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  return ((osc_destination_t*)user_data)->event_handler(path,types,argv,argc,msg);
}

int osc_destination_t::event_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message in_msg)
{
  if( use_condition ){
    if( (argc > 0) && (types[0] == 'f') )
      if( (argv[0]->f) != condition_val )
        return 1;
  }
  switch( argmode_ ){
  case source :
    {
      lo_send_message( target_, path_.c_str(), in_msg );
    }
    break;
  case replace :
    {
      lo_send_message( target_, path_.c_str(), own_msg );
    }
    break;
  case reorder :
    {
      lo_message msg(lo_message_new());
      int own_msg_arg_index(0);
      for(unsigned int k=0;k<argmap_.size();k++){
        if( (argmap_[k]) == 0 ){
          lo_message_copy_arg( msg, own_msg, own_msg_arg_index, false );
          own_msg_arg_index++;
        }else{
          lo_message_copy_arg( msg, in_msg, argmap_[k]-1, true );
        }
      }
      lo_send_message( target_, path_.c_str(), msg );
      lo_message_free(msg);
    }
    break;
  }
  return 1;
}

class midi_destination_t {
public:
  midi_destination_t(midi_client_t& mc, unsigned int channel, unsigned int param, unsigned argnum);
  ~midi_destination_t();
  static int event_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  int event_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message in_msg);
private:
  midi_client_t& mc_;
  unsigned int channel_;
  unsigned int param_;
  unsigned int argnum_;
};

midi_destination_t::midi_destination_t(midi_client_t& mc, unsigned int channel, unsigned int param, unsigned argnum)
  : mc_(mc),
    channel_(channel),
    param_(param),
    argnum_(argnum)
{
}

midi_destination_t::~midi_destination_t()
{
}

int midi_destination_t::event_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  return ((midi_destination_t*)user_data)->event_handler(path,types,argv,argc,msg);
}

int midi_destination_t::event_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message in_msg)
{
  if( ((int)(argnum_) < argc) && (types[argnum_] == 'f') )
    mc_.send_midi(channel_,param_,127*(argv[argnum_]->f));
  return 1;
}

static int exit_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  *((bool*)user_data) = true;
  return 1;
}

typedef std::vector<TASCAR::osc_server_t*> serverlist_t;
typedef std::vector<TASCAR::osc_server_t*>::iterator serverlist_it_t;

typedef std::vector<osc_destination_t*> destlist_t;
typedef std::vector<osc_destination_t*>::iterator destlist_it_t;

typedef std::vector<midi_destination_t*> midilist_t;
typedef std::vector<midi_destination_t*>::iterator midilist_it_t;

void xml_oscdest_data(osc_destination_t* pDestination,xmlpp::Element* TargetElem)
{
  xmlpp::Node::NodeList DataNodes = TargetElem->get_children("data");
  for(xmlpp::Node::NodeList::iterator DataIt=DataNodes.begin();
      DataIt != DataNodes.end(); ++DataIt){
    xmlpp::Element* DataElem = dynamic_cast<xmlpp::Element*>(*DataIt);
    if( DataElem ){
      std::string s_type(DataElem->get_attribute_value("type"));
      std::string s_value(DataElem->get_attribute_value("value"));
      if( (s_type.size() == 1) ){
        switch( s_type[0] ){
        case 'f':
          pDestination->add_float(atof(s_value.c_str()));
          break;
        case 'i':
          pDestination->add_int32(atoi(s_value.c_str()));
          break;
        case 's':
          pDestination->add_string(s_value);
          break;
        }
      }
    }
  }
}

osc_destination_t* xml_oscdest_alloc(xmlpp::Element* TargetElem)
{
  std::string dest(TargetElem->get_attribute_value("dest"));
  std::string path(TargetElem->get_attribute_value("path"));
  std::string s_argmap(TargetElem->get_attribute_value("argmap"));
  osc_destination_t::arg_mode_t argmode(osc_destination_t::source);
  std::vector<unsigned int> argmap;
  if( s_argmap.size() ){
    argmode = osc_destination_t::replace;
    if( strcmp( s_argmap.c_str(), "replace" ) != 0 ){
      argmode = osc_destination_t::reorder;
      std::stringstream ss(s_argmap);
      unsigned int i;
      while (ss >> i){
        argmap.push_back(i);
        if (ss.peek() == ',')
          ss.ignore();
      }
    }
  }
  osc_destination_t* pDestination(new osc_destination_t(dest,path,argmap,argmode));
  // now set condition data:
  std::string condition(TargetElem->get_attribute_value("condition"));
  if( condition.size() > 0 )
    pDestination->set_condition(atof(condition.c_str()));
  // value mapping:
  std::string s_valmap(TargetElem->get_attribute_value("valmap"));
  if( s_valmap.size() > 0 ){
    std::vector<float> val_map;
    std::stringstream ss(s_valmap);
    float i;
    while (ss >> i){
      val_map.push_back(i);
      if (ss.peek() == ',')
        ss.ignore();
    }
    if( val_map.size() == 2 )
      pDestination->set_valmap( val_map[0], val_map[1] );
  }
  xml_oscdest_data(pDestination,TargetElem);
  return pDestination;
}

int main(int argc, char** argv)
{
  std::string cfgfile;
  if( argc > 1 )
    cfgfile = argv[1];
  else{
    std::cerr << "Usage: " << argv[0] << " <cfgfile> [ <ctlport> ]\n";
    exit(1);
  }
  std::string ctlport("9999");
  if( argc > 2 )
    ctlport = argv[2];
  TASCAR::osc_server_t controller("",ctlport);
  bool b_exit(false);
  controller.add_method("/quit","",exit_handler,&b_exit);
  controller.activate();
  midi_client_t midic("ommo_bridge");
  serverlist_t serverlist;
  destlist_t destlist;
  midilist_t midilist;
  xmlpp::DomParser parser(cfgfile);
  xmlpp::Element* root = parser.get_document()->get_root_node();
  if( root ){
    // scan entries of OSC server:
    xmlpp::Node::NodeList ServerNodes = root->get_children("oscserver");
    for(xmlpp::Node::NodeList::iterator ServerIt=ServerNodes.begin();
        ServerIt != ServerNodes.end(); ++ServerIt){
      xmlpp::Element* ServerElem = dynamic_cast<xmlpp::Element*>(*ServerIt);
      if( ServerElem ){
        std::string mcaddr(ServerElem->get_attribute_value("multicast"));
        std::string port(ServerElem->get_attribute_value("port"));
        TASCAR::osc_server_t* pServer = new TASCAR::osc_server_t(mcaddr,port);
        // scan OSC targets:
        xmlpp::Node::NodeList TargetNodes = ServerElem->get_children("osc");
        for(xmlpp::Node::NodeList::iterator TargetIt=TargetNodes.begin();
            TargetIt != TargetNodes.end(); ++TargetIt){
          xmlpp::Element* TargetElem = dynamic_cast<xmlpp::Element*>(*TargetIt);
          if( TargetElem ){
            std::string serverpath(TargetElem->get_attribute_value("serverpath"));
            std::string types(TargetElem->get_attribute_value("types"));
            osc_destination_t* pDestination(xml_oscdest_alloc(TargetElem));
            pServer->add_method(serverpath,types.c_str(),osc_destination_t::event_handler,pDestination);
            destlist.push_back(pDestination);
          }
        }
        // scan midi targets:
        xmlpp::Node::NodeList MidiNodes = ServerElem->get_children("midicc");
        for(xmlpp::Node::NodeList::iterator MidiIt=MidiNodes.begin();
            MidiIt != MidiNodes.end(); ++MidiIt){
          xmlpp::Element* MidiElem = dynamic_cast<xmlpp::Element*>(*MidiIt);
          if( MidiElem ){
            std::string s_channel(MidiElem->get_attribute_value("channel"));
            int channel(atoi(s_channel.c_str()));
            std::string s_param(MidiElem->get_attribute_value("param"));
            int param(atoi(s_param.c_str()));
            std::string s_argnum(MidiElem->get_attribute_value("argnum"));
            int argnum(atoi(s_argnum.c_str()));
            std::string serverpath(MidiElem->get_attribute_value("serverpath"));
            std::string types(MidiElem->get_attribute_value("types"));
            midi_destination_t* pMidi;
            pMidi = new midi_destination_t(midic,channel,param,argnum);
            pServer->add_method(serverpath,types.c_str(),midi_destination_t::event_handler,pMidi);
            midilist.push_back(pMidi);
          }
        }
        serverlist.push_back(pServer);
      }
    }
    // scan entries of MIDI server:
    xmlpp::Node::NodeList MidiRecNodes = root->get_children("midireceiver");
    for(xmlpp::Node::NodeList::iterator MidiRecIt=MidiRecNodes.begin();
        MidiRecIt != MidiRecNodes.end(); ++MidiRecIt){
      xmlpp::Element* ServerElem = dynamic_cast<xmlpp::Element*>(*MidiRecIt);
      if( ServerElem ){
        // scan OSC targets:
        xmlpp::Node::NodeList TargetNodes = ServerElem->get_children("osc");
        for(xmlpp::Node::NodeList::iterator TargetIt=TargetNodes.begin();
            TargetIt != TargetNodes.end(); ++TargetIt){
          xmlpp::Element* TargetElem = dynamic_cast<xmlpp::Element*>(*TargetIt);
          if( TargetElem ){
            std::string s_cchannel(TargetElem->get_attribute_value("clientchannel"));
            int cchannel(atoi(s_cchannel.c_str()));
            std::string s_cparam(TargetElem->get_attribute_value("clientparam"));
            int cparam(atoi(s_cparam.c_str()));
            osc_destination_t* pDestination(xml_oscdest_alloc(TargetElem));
            midic.add_method(cchannel,cparam,osc_destination_t::event_handler,pDestination);
            destlist.push_back(pDestination);
          }
        }
        // scan midi targets:
        xmlpp::Node::NodeList MidiNodes = ServerElem->get_children("midicc");
        for(xmlpp::Node::NodeList::iterator MidiIt=MidiNodes.begin();
            MidiIt != MidiNodes.end(); ++MidiIt){
          xmlpp::Element* MidiElem = dynamic_cast<xmlpp::Element*>(*MidiIt);
          if( MidiElem ){
            std::string s_cchannel(MidiElem->get_attribute_value("clientchannel"));
            int cchannel(atoi(s_cchannel.c_str()));
            std::string s_cparam(MidiElem->get_attribute_value("clientparam"));
            int cparam(atoi(s_cparam.c_str()));
            std::string s_channel(MidiElem->get_attribute_value("channel"));
            int channel(atoi(s_channel.c_str()));
            std::string s_param(MidiElem->get_attribute_value("param"));
            int param(atoi(s_param.c_str()));
            midi_destination_t* pMidi;
            pMidi = new midi_destination_t(midic,channel,param,0);
            midic.add_method(cchannel,cparam,midi_destination_t::event_handler,pMidi);
            midilist.push_back(pMidi);
          }
        }
      }
    }
  }
  for( serverlist_it_t srv=serverlist.begin();srv!=serverlist.end();++srv)
    (*srv)->activate();
  midic.start_service();
  while(!b_exit){
    usleep(1000);
  }
  midic.stop_service();
  for( serverlist_it_t srv=serverlist.begin();srv!=serverlist.end();++srv)
    (*srv)->deactivate();
  controller.deactivate();
  // clean up:
  for( serverlist_it_t srv=serverlist.begin();srv!=serverlist.end();++srv)
    delete (*srv);
  for( midilist_it_t it=midilist.begin();it!=midilist.end();++it)
    delete (*it);
  for( destlist_it_t it=destlist.begin();it!=destlist.end();++it)
    delete (*it);
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
