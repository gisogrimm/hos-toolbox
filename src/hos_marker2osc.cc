/**
   \file hos_marker2osc.cc
   \ingroup apphos
   \brief Send OSC commands at ardour marker positions based on JACK transport.

   \author Giso Grimm
   \date 2011

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

#include <tascar/jackclient.h>
#include <iostream>
#include <libxml++/libxml++.h>
#include <stdlib.h>
#include <lo/lo.h>
#include <getopt.h>
#include <unistd.h>


static bool b_quit;

namespace HoS {

  /**
     \brief Marker representation
  */
  class marker_t {
  public:
    marker_t(unsigned long p,const std::string& n) : pos(p),name(n){};
    unsigned long pos;
    std::string name;
  };

  /**
     \brief Announce Ardour markers via OSC at appropriate positions.
  */
  class oscam_t : public jackc_transport_t{
  public:
    oscam_t(const std::string& oscad,const std::string& op,const std::string& fname, const std::string& jackname, bool markerpath);
    void run();
    int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer,uint32_t tp_frame,bool tp_running);
    void dump_db();
  private:
    //bool b_quit;
    bool b_markerpath;
    lo_address lo_addr;
    std::string path;
    std::vector<marker_t> db;
  };

}

using namespace HoS;

void oscam_t::dump_db()
{
  for(unsigned int k=0;k<db.size();k++)
    std::cout << k << " at " << db[k].pos << " : " << db[k].name << std::endl;
}

/**
   \param oscad OSC target address
   \param op OSC target path
   \param fname Ardour session file name
*/
oscam_t::oscam_t(const std::string& oscad,const std::string& op,const std::string& fname, const std::string& jackname, bool markerpath)
  : jackc_transport_t(jackname),
    //b_quit(false),
    b_markerpath(markerpath),
    lo_addr(lo_address_new_from_url( oscad.c_str() )),
    path(op)
{
  //DEBUG(fname);
  xmlpp::DomParser parser(fname);
  xmlpp::Element* root = parser.get_document()->get_root_node();
  if( root ){
    //xmlpp::Node::NodeList nodesPiece = root->get_children("Location");
    xmlpp::NodeSet nodesPiece = root->find("//Location");
    for(xmlpp::NodeSet::iterator nit=nodesPiece.begin();nit!=nodesPiece.end();++nit){
      xmlpp::Element* loc = dynamic_cast<xmlpp::Element*>(*nit);
      if( loc ){
        marker_t m(atol(loc->get_attribute_value("start").c_str()),loc->get_attribute_value("name"));
        if( b_markerpath )
          m.name = path + m.name;
        db.push_back(m);
      }
    }
  }
  //dump_db();
}

int oscam_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer,uint32_t tp_frame,bool tp_running)
{
  if( tp_running ){
    for(unsigned int k=0;k<db.size();k++) {
      if( (db[k].pos >= tp_frame) && (db[k].pos < tp_frame+nframes) ){
        //DEBUG(db[k].name);
        if( b_markerpath ){
          lo_send(lo_addr,db[k].name.c_str(),"");
        }else
          lo_send(lo_addr,path.c_str(),"s",db[k].name.c_str());
      }
    }
  }
  return 0;
}

void oscam_t::run()
{
  activate();
  while( !b_quit ){
    sleep( 1 );
  }
  deactivate();
}


void usage(struct option * opt)
{
  std::cout << "Usage:\n\nhos_marker2osc [options] ardourfile\n\nOptions:\n\n";
  while( opt->name ){
    std::cout << "  -" << (char)(opt->val) << " " << (opt->has_arg?"#":"") <<
      "\n  --" << opt->name << (opt->has_arg?"=#":"") << "\n\n";
    opt++;
  }
}

static void sighandler(int sig)
{
  b_quit = true;
}


int main(int argc, char** argv)
{
  b_quit = false;
  signal(SIGABRT, &sighandler);
  signal(SIGTERM, &sighandler);
  signal(SIGINT, &sighandler);
  std::string jackname("");
  std::string ardourfile("");
  std::string addr("osc.udp://localhost:6790/");
  std::string path("/marker");
  bool markerpath(false);
  const char *options = "d:p:hn:m";
  struct option long_options[] = { 
    { "dest",       1, 0, 'd' },
    { "pathprefix", 1, 0, 'p' },
    { "jackname",   1, 0, 'n' },
    { "markerpath", 0, 0, 'm' },
    { "help",       0, 0, 'h' },
    { 0, 0, 0, 0 }
  };
  int opt(0);
  int option_index(0);
  while( (opt = getopt_long(argc, argv, options,
                            long_options, &option_index)) != -1){
    switch(opt){
    case 'd':
      addr = optarg;
      break;
    case 'h':
      usage(long_options);
      return -1;
    case 'p':
      path = optarg;
      break;
    case 'm':
      markerpath = true;
      break;
    case 'n':
      jackname = optarg;
      break;
    }
  }
  if( optind < argc )
    ardourfile = argv[optind++];
  if( ardourfile.size() == 0 ){
    usage(long_options);
    return -1;
  }
  if( jackname.size() == 0 ){
    jackname = ardourfile;
  }
  oscam_t S(addr,path,ardourfile,jackname,markerpath);
  S.run();
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
