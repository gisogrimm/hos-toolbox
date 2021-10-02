/**
   \file libhos_gainmatrix.cc
   \ingroup apphos
   \brief Matrix implementation for matrix mixer
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
   USA.

*/

/**
 */

#include "libhos_gainmatrix.h"
#include <libxml++/libxml++.h>
#include <math.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

using namespace MM;

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

std::vector<unsigned int> str2vuint(const std::string& s)
{
  std::vector<unsigned int> r;
  // std::string fv;
  std::istringstream tmp(s + std::string(" "));
  unsigned int v;
  while(tmp >> v) {
    // MHAParser::StrCnv::str2val( fv, tmpval );
    r.push_back(v);
  }
  return r;
}

std::string vuint2str(const std::vector<unsigned int>& s)
{
  std::string r;
  // std::string fv;
  std::ostringstream tmp("");
  for(unsigned int k = 0; k < s.size(); k++) {
    if(k)
      tmp << " ";
    tmp << s[k];
  }
  r = tmp.str();
  return r;
}

gainmatrix_t::gainmatrix_t(unsigned int nout, unsigned int nin)
    : size_(nout * nin), n_out_(nout), n_in_(nin),
      G(std::vector<double>(size_, 0.0)), P(std::vector<double>(size_, 0.0)),
      Gout(std::vector<double>(n_out_, 0.0)),
      muted(std::vector<double>(n_in_, 1.0)),
      select_out(std::vector<unsigned int>(n_out_, 0)),
      select_in(std::vector<unsigned int>(n_in_, 0)),
      outports(std::vector<std::vector<unsigned int>>(
          n_out_, std::vector<unsigned int>(1, 0))),
      inports(std::vector<std::vector<unsigned int>>(
          n_in_, std::vector<unsigned int>(1, 0)))
{
  pthread_mutex_init(&mutex, NULL);
}

gainmatrix_t::~gainmatrix_t()
{
  pthread_mutex_destroy(&mutex);
}

void gainmatrix_t::set_mute(unsigned int kin, double mute)
{
  ////DEBUG("set_out_gain");
  bool lmod(false);
  lock();
  if((kin < n_in_) && ((float)(muted[kin]) != (float)mute)) {
    muted[kin] = mute;
    // modified = true;
    modify();
    lmod = true;
  }
  unlock();
  if(lmod)
    modified_mute(kin, mute);
}

void gainmatrix_t::add_observer(observer_t* o)
{
  modified[o] = true;
}

void gainmatrix_t::rm_observer(observer_t* o)
{
  std::map<observer_t*, bool>::iterator i(modified.find(o));
  if(i != modified.end())
    modified.erase(o);
}

void gainmatrix_t::modify()
{
  for(std::map<observer_t*, bool>::iterator i = modified.begin();
      i != modified.end(); ++i)
    i->second = true;
}

void gainmatrix_t::set_out_gain(unsigned int k, double g)
{
  ////DEBUG("set_out_gain");
  bool lmod(false);
  lock();
  if((k < n_out_) && ((float)(Gout[k]) != (float)g)) {
    Gout[k] = g;
    // modified = true;
    modify();
    lmod = true;
  }
  unlock();
  if(lmod)
    modified_gain_out(k, g);
}

// deprecated
double gainmatrix_t::channelgain(unsigned int kout, unsigned int out_sub,
                                 unsigned int kin, unsigned int in_sub)
{
  double g = get_gain(kout, kin);
  if((get_n_in(kin) == 1)) {
    unsigned int n_outports = get_n_out(kout);
    double p = P[index(kout, kin)] * M_PI * 2.0;
    double po = (double)out_sub / (double)n_outports * M_PI * 2.0;
    double zpr = cos(p);
    double zpi = sin(p);
    double zpor = cos(po);
    double zpoi = sin(po);
    double zr = zpr * zpor + zpi * zpoi;
    double zi = zpor * zpi - zpr * zpoi;
    g *= cos(std::min(M_PI * 0.5, 0.25 * n_outports * fabs(atan2(zi, zr))));
  } else {
    g *= (out_sub == in_sub);
  }
  g *= Gout[kout];
  g *= muted[kin];
  return g;
}

void gainmatrix_t::set_gain(unsigned int kout, unsigned int kin, double g)
{
  bool lmod(false);
  lock();
  if((kout < n_out_) && (kin < n_in_) &&
     ((float)(G[index(kout, kin)]) != (float)g)) {
    G[index(kout, kin)] = g;
    // modified = true;
    modify();
    lmod = true;
  }
  unlock();
  if(lmod)
    modified_gain(kout, kin, g);
}

void gainmatrix_t::set_pan(unsigned int kout, unsigned int kin, double p)
{
  // DEBUG("set_pan");
  bool lmod(false);
  lock();
  if((kout < n_out_) && (kin < n_in_) &&
     ((float)(P[index(kout, kin)]) != (float)p)) {
    P[index(kout, kin)] = p;
    // modified = true;
    modify();
    lmod = true;
  }
  unlock();
  if(lmod)
    modified_pan(kout, kin, p);
}

void gainmatrix_t::set_select_out(unsigned int k, unsigned int n)
{
  // DEBUG("set_select_out");
  bool lmod(false);
  lock();
  if((k < n_out_) && (select_out[k] != n)) {
    // DEBUG(select_out.size());
    // DEBUG(k);
    select_out[k] = n;
    // modified = true;
    modify();
    lmod = true;
  }
  unlock();
  if(lmod)
    modified_select_out(k, n);
}

void gainmatrix_t::set_select_in(unsigned int k, unsigned int n)
{
  // DEBUG("set_select_in");
  bool lmod(false);
  lock();
  if((k < n_in_) && (select_in[k] != n)) {
    select_in[k] = n;
    // modified = true;
    modify();
    lmod = true;
  }
  unlock();
  if(lmod)
    modified_select_in(k, n);
}

void gainmatrix_t::set_inports(unsigned int k,
                               const std::vector<unsigned int>& ports)
{
  // DEBUG("set_inports");
  bool lmod(false);
  lock();
  if((k < n_in_) && (inports[k] != ports)) {
    inports[k] = ports;
    // modified = true;
    modify();
    lmod = true;
  }
  unlock();
  if(lmod)
    modified_inports(k, ports);
}

void gainmatrix_t::set_outports(unsigned int k,
                                const std::vector<unsigned int>& ports)
{
  // DEBUG("set_outports");
  bool lmod(false);
  lock();
  if((k < n_out_) && (outports[k] != ports)) {
    outports[k] = ports;
    // modified = true;
    modify();
    lmod = true;
  }
  unlock();
  if(lmod)
    modified_outports(k, ports);
}

namematrix_t::namematrix_t(unsigned int nout, unsigned int nin)
    : gainmatrix_t(nout, nin)
{
  char ctmp[128];
  for(unsigned int k = 0; k < nout; k++) {
    sprintf(ctmp, "out_%d", k);
    name_out.push_back(ctmp);
  }
  for(unsigned int k = 0; k < nin; k++) {
    sprintf(ctmp, "in_%d", k);
    name_in.push_back(ctmp);
  }
}

void namematrix_t::set_name_in(unsigned int kin, const std::string& name)
{
  // DEBUG("set_name_in");
  bool lmod(false);
  lock();
  if((kin < n_in_) && (name_in[kin] != name)) {
    name_in[kin] = name;
    // modified = true;
    modify();
    lmod = true;
  }
  unlock();
  if(lmod)
    modified_name_in(kin, name);
}

void namematrix_t::set_name_out(unsigned int kout, const std::string& name)
{
  // DEBUG("set_name_out");
  bool lmod(false);
  lock();
  if((kout < n_out_) && (name_out[kout] != name)) {
    name_out[kout] = name;
    // modified = true;
    modify();
    lmod = true;
  }
  unlock();
  if(lmod)
    modified_name_out(kout, name);
}

std::string namematrix_t::get_name_in(unsigned int kin)
{
  return name_in[kin];
}

std::string namematrix_t::get_name_out(unsigned int kout)
{
  return name_out[kout];
}

lo_matrix_t::lo_matrix_t(unsigned int nout, unsigned int nin,
                         lo_server_thread& ilost, lo_address& iaddr)
    : namematrix_t(nout, nin), lost(ilost), addr(iaddr)
{
  lo_server_thread_add_method(lost, "/hdspmm/name_in", "is",
                              lo_matrix_t::osc_set_name_in, this);
  lo_server_thread_add_method(lost, "/hdspmm/name_out", "is",
                              lo_matrix_t::osc_set_name_out, this);
  lo_server_thread_add_method(lost, "/hdspmm/gain_out", "if",
                              lo_matrix_t::osc_set_gain_out, this);
  lo_server_thread_add_method(lost, "/hdspmm/mute", "if",
                              lo_matrix_t::osc_set_mute, this);
  lo_server_thread_add_method(lost, "/hdspmm/gain", "iif",
                              lo_matrix_t::osc_set_gain, this);
  lo_server_thread_add_method(lost, "/hdspmm/pan", "iif",
                              lo_matrix_t::osc_set_pan, this);
  lo_server_thread_add_method(lost, "/hdspmm/inports", "is",
                              lo_matrix_t::osc_set_inports, this);
  lo_server_thread_add_method(lost, "/hdspmm/outports", "is",
                              lo_matrix_t::osc_set_outports, this);
  lo_server_thread_add_method(lost, "/hdspmm/select_in", "ii",
                              lo_matrix_t::osc_set_select_in, this);
  lo_server_thread_add_method(lost, "/hdspmm/select_out", "ii",
                              lo_matrix_t::osc_set_select_out, this);
  // lo_send( addr, "/hdspmm/backend_new", "ii", (int)nout, (int)nin);
}

lo_matrix_t::~lo_matrix_t()
{
  lo_server_thread_del_method(lost, "/hdspmm/name_in", "is");
  lo_server_thread_del_method(lost, "/hdspmm/name_out", "is");
  lo_server_thread_del_method(lost, "/hdspmm/gain_out", "if");
  lo_server_thread_del_method(lost, "/hdspmm/gain", "iif");
  lo_server_thread_del_method(lost, "/hdspmm/pan", "iif");
  lo_server_thread_del_method(lost, "/hdspmm/inports", "is");
  lo_server_thread_del_method(lost, "/hdspmm/outports", "is");
  lo_server_thread_del_method(lost, "/hdspmm/select_in", "ii");
  lo_server_thread_del_method(lost, "/hdspmm/select_out", "ii");
  // lo_send( addr, "/hdspmm/backend_quit", "" );
}

void lo_matrix_t::modified_mute(unsigned int kin, double mute)
{
  lo_send(addr, "/hdspmm/mute", "if", (int)kin, (float)mute);
}

void lo_matrix_t::modified_gain(unsigned int kout, unsigned kin, double g)
{
  // hdspmatrix_t::modified_gain(kout,kin,g);
  lo_send(addr, "/hdspmm/gain", "iif", (int)kout, (int)kin, (float)g);
}

void lo_matrix_t::modified_pan(unsigned int kout, unsigned kin, double p)
{
  // hdspmatrix_t::modified_pan(kout,kin,p);
  lo_send(addr, "/hdspmm/pan", "iif", (int)kout, (int)kin, (float)p);
}

void lo_matrix_t::modified_name_out(unsigned int k, const std::string& name)
{
  // hdspmatrix_t::modified_n_out(k,n);
  lo_send(addr, "/hdspmm/name_out", "is", (int)k, name.c_str());
}

void lo_matrix_t::modified_name_in(unsigned int k, const std::string& name)
{
  // hdspmatrix_t::modified_n_out(k,n);
  lo_send(addr, "/hdspmm/name_in", "is", (int)k, name.c_str());
}

void lo_matrix_t::modified_gain_out(unsigned int k, double g)
{
  // hdspmatrix_t::modified_n_out(k,n);
  lo_send(addr, "/hdspmm/gain_out", "if", (int)k, g);
}

void lo_matrix_t::modified_outports(unsigned int k,
                                    const std::vector<unsigned int>& p)
{
  std::string tmp(vuint2str(p));
  lo_send(addr, "/hdspmm/outports", "is", (int)k, tmp.c_str());
}

void lo_matrix_t::modified_inports(unsigned int k,
                                   const std::vector<unsigned int>& p)
{
  std::string tmp(vuint2str(p));
  lo_send(addr, "/hdspmm/inports", "is", (int)k, tmp.c_str());
}

void lo_matrix_t::modified_select_out(unsigned int k, unsigned int n)
{
  lo_send(addr, "/hdspmm/select_out", "ii", (int)k, (int)n);
}

void lo_matrix_t::modified_select_in(unsigned int k, unsigned int n)
{
  lo_send(addr, "/hdspmm/select_in", "ii", (int)k, (int)n);
}

/*******************************************************
 *
 * OSC handler
 *
 *******************************************************/

int lo_matrix_t::osc_set_name_in(const char* path, const char* types,
                                 lo_arg** argv, int argc, lo_message msg,
                                 void* user_data)
{
  if((argc == 2) && (types[0] == 'i') && (types[1] == 's')) {
    ((lo_matrix_t*)user_data)->set_name_in(argv[0]->i, &(argv[1]->s));
    return 0;
  }
  return 1;
};

int lo_matrix_t::osc_set_select_in(const char* path, const char* types,
                                   lo_arg** argv, int argc, lo_message msg,
                                   void* user_data)
{
  if((argc == 2) && (types[0] == 'i') && (types[1] == 'i')) {
    ((lo_matrix_t*)user_data)->set_select_in(argv[0]->i, argv[1]->i);
    return 0;
  }
  return 1;
};

int lo_matrix_t::osc_set_select_out(const char* path, const char* types,
                                    lo_arg** argv, int argc, lo_message msg,
                                    void* user_data)
{
  if((argc == 2) && (types[0] == 'i') && (types[1] == 'i')) {
    ((lo_matrix_t*)user_data)->set_select_out(argv[0]->i, argv[1]->i);
    return 0;
  }
  return 1;
};

int lo_matrix_t::osc_set_name_out(const char* path, const char* types,
                                  lo_arg** argv, int argc, lo_message msg,
                                  void* user_data)
{
  if((argc == 2) && (types[0] == 'i') && (types[1] == 's')) {
    ((lo_matrix_t*)user_data)->set_name_out(argv[0]->i, &(argv[1]->s));
    return 0;
  }
  return 1;
};

int lo_matrix_t::osc_set_gain(const char* path, const char* types,
                              lo_arg** argv, int argc, lo_message msg,
                              void* user_data)
{
  if((argc == 3) && (types[0] == 'i') && (types[1] == 'i') &&
     (types[2] == 'f')) {
    ((lo_matrix_t*)user_data)->set_gain(argv[0]->i, argv[1]->i, argv[2]->f);
    return 0;
  }
  return 1;
};

int lo_matrix_t::osc_set_mute(const char* path, const char* types,
                              lo_arg** argv, int argc, lo_message msg,
                              void* user_data)
{
  if((argc == 2) && (types[0] == 'i') && (types[1] == 'f')) {
    ((lo_matrix_t*)user_data)->set_mute(argv[0]->i, argv[1]->f);
    return 0;
  }
  return 1;
};

int lo_matrix_t::osc_set_pan(const char* path, const char* types, lo_arg** argv,
                             int argc, lo_message msg, void* user_data)
{
  if((argc == 3) && (types[0] == 'i') && (types[1] == 'i') &&
     (types[2] == 'f')) {
    ((lo_matrix_t*)user_data)->set_pan(argv[0]->i, argv[1]->i, argv[2]->f);
    return 0;
  }
  return 1;
};

int lo_matrix_t::osc_set_inports(const char* path, const char* types,
                                 lo_arg** argv, int argc, lo_message msg,
                                 void* user_data)
{
  if((argc == 2) && (types[0] == 'i') && (types[1] == 's')) {
    ((lo_matrix_t*)user_data)
        ->set_inports(argv[0]->i, str2vuint(&(argv[1]->s)));
    return 0;
  }
  return 1;
};

int lo_matrix_t::osc_set_outports(const char* path, const char* types,
                                  lo_arg** argv, int argc, lo_message msg,
                                  void* user_data)
{
  if((argc == 2) && (types[0] == 'i') && (types[1] == 's')) {
    ((lo_matrix_t*)user_data)
        ->set_outports(argv[0]->i, str2vuint(&(argv[1]->s)));
    return 0;
  }
  return 1;
};

int lo_matrix_t::osc_set_gain_out(const char* path, const char* types,
                                  lo_arg** argv, int argc, lo_message msg,
                                  void* user_data)
{
  if((argc == 2) && (types[0] == 'i') && (types[1] == 'f')) {
    ((lo_matrix_t*)user_data)->set_out_gain(argv[0]->i, argv[1]->f);
    return 0;
  }
  return 1;
};

MM::namematrix_t* MM::load(const std::string& fname)
{
  MM::namematrix_t* m(NULL);
  xmlpp::DomParser parser(fname.c_str());
  xmlpp::Element* root = parser.get_document()->get_root_node();
  if(root) {
    xmlpp::Node::NodeList nodesInput = root->get_children("input");
    xmlpp::Node::NodeList nodesOutput = root->get_children("output");
    uint32_t nin = nodesInput.size();
    uint32_t nout = nodesOutput.size();
    // xmlpp::Node::NodeList nodesOSC = root->get_children("osc");
    // const xmlpp::Element* nodeElement;
    // if( nodesOSC.size() && (nodeElement = dynamic_cast<const
    // xmlpp::Element*>(*(nodesOSC.begin()))) ){
    //  destaddr = nodeElement->get_attribute_value("destaddr");
    //  serveraddr = nodeElement->get_attribute_value("serveraddr");
    //  serverport = nodeElement->get_attribute_value("serverport");
    //}
    m = new MM::namematrix_t(nout, nin);
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
  return m;
}

void MM::save(MM::namematrix_t* m, const std::string& fname)
{
  xmlpp::Document doc;
  xmlpp::Element* root = doc.create_root_node("hdspmm");
  // xmlpp::Element* elOSC = root->add_child("osc");
  // elOSC->set_attribute("destaddr",destaddr);
  // elOSC->set_attribute("serveraddr",serveraddr);
  // elOSC->set_attribute("serverport",serverport);
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

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
