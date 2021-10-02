/**
   \file mm_file.cc
   \brief Matrix mixer file interface
   \ingroup apphos
   \author Giso Grimm
   \date 2011

  "mm_{hdsp,file,gui,midicc}" is a set of programs to control the matrix
  mixer of an RME hdsp compatible sound card using XML formatted files
  or a MIDI controller, and to visualize the mixing matrix.

  \section license License (GPL)

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
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
  USA.

*/
#include "libhos_gainmatrix.h"
#include <libxml++/libxml++.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void osc_err_handler_cb(int num, const char* msg, const char* where)
{
  std::cerr << "lo error " << num << ": " << msg << " (" << where << ")\n";
}

namespace MM {

  /**
  \ingroup apphos
  */
  class mm_file_t {
  public:
    mm_file_t();
    ~mm_file_t();
    void load(const std::string& name);
    void save(const std::string& name);
    void reset_par();
    void create(unsigned int nout, unsigned int nin);
    void copy_pan(unsigned int src, unsigned int dest);
    void copy_gain(unsigned int src, unsigned int dest);
    void destroy();

  private:
    unsigned int nout;
    unsigned int nin;
    std::string destaddr;
    std::string serveraddr;
    std::string serverport;
    MM::lo_matrix_t* m;
    lo_server_thread lost;
    lo_address addr;
  };

} // namespace MM

std::string strcnv(unsigned int a)
{
  char buff[1024];
  snprintf(buff, 1024, "%d", a);
  return std::string(buff);
}

std::string strcnv(float a)
{
  char buff[1024];
  snprintf(buff, 1024, "%g", a);
  return std::string(buff);
}

std::string strcnv(double a)
{
  char buff[1024];
  snprintf(buff, 1024, "%g", a);
  return std::string(buff);
}

void MM::mm_file_t::destroy()
{
  if(m) {
    lo_send(addr, "/hdspmm/backend_quit", "");
    lo_server_thread_stop(lost);
    delete m;
    lo_server_thread_free(lost);
    lo_address_free(addr);
  }
  m = NULL;
}

void MM::mm_file_t::create(unsigned int nout, unsigned int nin)
{
  destroy();
  if(serveraddr.size())
    lost = lo_server_thread_new_multicast(
        serveraddr.c_str(), serverport.c_str(), osc_err_handler_cb);
  else
    lost = lo_server_thread_new(serverport.c_str(), osc_err_handler_cb);
  addr = lo_address_new_from_url(destaddr.c_str());
  lo_address_set_ttl(addr, 1);
  m = new MM::lo_matrix_t(nout, nin, lost, addr);
  lo_server_thread_start(lost);
  lo_send(addr, "/hdspmm/backend_new", "ii", (int)nout, (int)nin);
}

void MM::mm_file_t::copy_pan(unsigned int src, unsigned int dest)
{
  if(m) {
    for(unsigned int kin = 0; kin < m->get_num_inputs(); kin++) {
      m->set_pan(dest, kin, m->get_pan(src, kin));
      // m->set_gain( dest, kin, m->get_gain( src, kin ) );
    }
  }
}

void MM::mm_file_t::copy_gain(unsigned int src, unsigned int dest)
{
  if(m) {
    for(unsigned int kin = 0; kin < m->get_num_inputs(); kin++) {
      // m->set_pan( dest, kin, m->get_pan( src, kin ) );
      m->set_gain(dest, kin, m->get_gain(src, kin));
    }
  }
}

MM::mm_file_t::mm_file_t() : m(NULL)
{
  reset_par();
}

void MM::mm_file_t::reset_par()
{
  nout = 1;
  nin = 1;
  destaddr = "osc.udp://239.255.1.7:6978/";
  serveraddr = "";
  serverport = "6979";
}

MM::mm_file_t::~mm_file_t()
{
  if(m)
    delete m;
}

void MM::mm_file_t::load(const std::string& fname)
{
  reset_par();
  xmlpp::DomParser parser(fname.c_str());
  xmlpp::Element* root = parser.get_document()->get_root_node();
  if(root) {
    xmlpp::Node::NodeList nodesInput = root->get_children("input");
    xmlpp::Node::NodeList nodesOutput = root->get_children("output");
    nin = nodesInput.size();
    nout = nodesOutput.size();
    xmlpp::Node::NodeList nodesOSC = root->get_children("osc");
    const xmlpp::Element* nodeElement;
    if(nodesOSC.size() && (nodeElement = dynamic_cast<const xmlpp::Element*>(
                               *(nodesOSC.begin())))) {
      destaddr = nodeElement->get_attribute_value("destaddr");
      serveraddr = nodeElement->get_attribute_value("serveraddr");
      serverport = nodeElement->get_attribute_value("serverport");
    }
    create(nout, nin);
    unsigned int k = 0;
    for(xmlpp::Node::NodeList::iterator nit = nodesInput.begin();
        nit != nodesInput.end(); ++nit) {
      const xmlpp::Element* nodeElement =
          dynamic_cast<const xmlpp::Element*>(*nit);
      if(nodeElement) {
        std::string nname(nodeElement->get_attribute_value("name"));
        if(nname.size())
          m->set_name_in(k, nname);
        nname = nodeElement->get_attribute_value("ports");
        if(nname.size())
          m->set_inports(k, str2vuint(nname));
        double mute = atof(nodeElement->get_attribute_value("mute").c_str());
        m->set_mute(k, mute);
      }
      k++;
    }
    k = 0;
    for(xmlpp::Node::NodeList::iterator nit = nodesOutput.begin();
        nit != nodesOutput.end(); ++nit) {
      const xmlpp::Element* nodeElement =
          dynamic_cast<const xmlpp::Element*>(*nit);
      if(nodeElement) {
        std::string nname(nodeElement->get_attribute_value("name"));
        if(nname.size())
          m->set_name_out(k, nname);
        nname = nodeElement->get_attribute_value("ports");
        if(nname.size())
          m->set_outports(k, str2vuint(nname));
        nname = nodeElement->get_attribute_value("gain");
        m->set_out_gain(k, atof(nname.c_str()));
      }
      k++;
    }
    xmlpp::Node::NodeList nodesMatrix = root->get_children("matrix");
    if(nodesMatrix.size()) {
      xmlpp::Node::NodeList nodesME =
          (*nodesMatrix.begin())->get_children("me");
      for(xmlpp::Node::NodeList::iterator nit = nodesME.begin();
          nit != nodesME.end(); ++nit) {
        const xmlpp::Element* me = dynamic_cast<const xmlpp::Element*>(*nit);
        if(me) {
          unsigned int k_out = atoi(me->get_attribute_value("out").c_str());
          unsigned int k_in = atoi(me->get_attribute_value("in").c_str());
          double gain = atof(me->get_attribute_value("gain").c_str());
          double pan = atof(me->get_attribute_value("pan").c_str());
          m->set_gain(k_out, k_in, gain);
          m->set_pan(k_out, k_in, pan);
        }
      }
    }
  }
}

void MM::mm_file_t::save(const std::string& fname)
{
  xmlpp::Document doc;
  xmlpp::Element* root = doc.create_root_node("hdspmm");
  xmlpp::Element* elOSC = root->add_child("osc");
  elOSC->set_attribute("destaddr", destaddr);
  elOSC->set_attribute("serveraddr", serveraddr);
  elOSC->set_attribute("serverport", serverport);
  if(m) {
    for(unsigned int k = 0; k < m->get_num_inputs(); k++) {
      xmlpp::Element* elIN = root->add_child("input");
      elIN->set_attribute("name", m->get_name_in(k));
      elIN->set_attribute("ports", vuint2str(m->get_inports(k)));
      elIN->set_attribute("mute", strcnv(m->get_mute(k)));
    }
    for(unsigned int k = 0; k < m->get_num_outputs(); k++) {
      xmlpp::Element* elOUT = root->add_child("output");
      elOUT->set_attribute("name", m->get_name_out(k));
      elOUT->set_attribute("ports", vuint2str(m->get_outports(k)));
      elOUT->set_attribute("gain", strcnv(m->get_out_gain(k)));
    }
    xmlpp::Element* elMM = root->add_child("matrix");
    for(unsigned int kout = 0; kout < m->get_num_outputs(); kout++) {
      for(unsigned int kin = 0; kin < m->get_num_inputs(); kin++) {
        xmlpp::Element* elME = elMM->add_child("me");
        elME->set_attribute("in", strcnv(kin));
        elME->set_attribute("out", strcnv(kout));
        elME->set_attribute("gain", strcnv(m->get_gain(kout, kin)));
        elME->set_attribute("pan", strcnv(m->get_pan(kout, kin)));
      }
    }
  }
  doc.write_to_file_formatted(fname);
}

int main(int argc, char** argv)
{
  try {
    MM::mm_file_t m;
    if(argc > 1)
      m.load(argv[1]);
    char s[1000];
    s[0] = 0;
    while(strcmp(s, "quit") != 0) {
      fprintf(stdout, "mm_file> ");
      fflush(stdout);
      if(fgets(s, 900, stdin)) {
        if(strlen(s) && (s[strlen(s) - 1] == '\n'))
          s[strlen(s) - 1] = 0;
        if((strncmp(s, "save", 4) == 0) && (strlen(s) > 5)) {
          m.save(&(s[5]));
        }
        if((strncmp(s, "load", 4) == 0) && (strlen(s) > 5)) {
          m.load(&(s[5]));
        }
        if((strncmp(s, "create", 6) == 0) && (strlen(s) > 8)) {
          int num_out(1);
          int num_in(1);
          sscanf(&(s[7]), "%d %d", &num_out, &num_in);
          if((num_out > 0) && (num_in > 0))
            m.create(num_out, num_in);
        }
        if((strncmp(s, "cppan", 5) == 0) && (strlen(s) > 8)) {
          int src(0);
          int dest(0);
          sscanf(&(s[6]), "%d %d", &src, &dest);
          if((src >= 0) && (dest >= 0))
            m.copy_pan(src, dest);
        }
        if((strncmp(s, "cpgain", 6) == 0) && (strlen(s) > 9)) {
          int src(0);
          int dest(0);
          sscanf(&(s[7]), "%d %d", &src, &dest);
          if((src >= 0) && (dest >= 0))
            m.copy_gain(src, dest);
        }
        if((strncmp(s, "destroy", 7) == 0)) {
          m.destroy();
        }
        if((strncmp(s, "help", 4) == 0)) {
          fprintf(
              stdout,
              "valid commands:\n  quit\n  save <filename>\n  load <filename>\n "
              " create <num_outputs> <num_inputs>\n  cppan <src#> <dest#> "
              "(copy output settings, zero base)\n  cpgain <src#> <dest#> "
              "(copy output settings, zero base)\n  destroy\n  help\n");
        }
      }
    }
    m.save("quit.hdspmm");
    return 0;
  }
  catch(const std::exception& e) {
    std::cerr << e.what() << std::endl;
    exit(1);
  }
  catch(const char* e) {
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
