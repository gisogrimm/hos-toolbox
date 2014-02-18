/**
   \file libhos_gainmatrix.h
   \ingroup apphos
   \brief Matrix mixer gain matrix
   \author Giso Grimm
   \date 2011

   \section license License (GPL)

   Copyright (C) 2011 Giso Grimm

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
#ifndef HOS_GAINMATRIX_H
#define HOS_GAINMATRIX_H

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <lo/lo.h>

std::vector<unsigned int> str2vuint( const std::string& s );
std::string vuint2str( const std::vector<unsigned int>& s );

/**
   \brief Matrix mixer classes
*/
namespace MM {

  class observer_t {
  public:
  };

  class gainmatrix_t {
  public:
    gainmatrix_t(unsigned int nout, unsigned int nin);
    //gainmatrix_t(const gainmatrix_t& src);
    unsigned int index(unsigned int kout,unsigned int kin){ return kout+n_out_*kin; };
    ~gainmatrix_t();
    double channelgain(unsigned int out_main, unsigned int out_sub,
                       unsigned int in_main, unsigned int in_sub);
    void set_mute(unsigned int kin,double mute);
    void set_gain(unsigned int kout, unsigned int kin, double g);
    void set_pan(unsigned int kout, unsigned int kin, double p);
    void set_out_gain(unsigned int k,double g);
    void set_select_out(unsigned int kin, unsigned int n);
    void set_select_in(unsigned int kin, unsigned int n);
    void set_outports(unsigned int k, const std::vector<unsigned int>& ports);
    void set_inports(unsigned int k, const std::vector<unsigned int>& ports);
    double get_out_gain(unsigned int k){ return Gout[k];};
    double get_gain(unsigned int kout, unsigned int kin){ return G[index(kout,kin)];};
    double get_pan(unsigned int kout, unsigned int kin){ return P[index(kout,kin)];};
    unsigned int get_n_out(unsigned int k){ return outports[k].size();};
    unsigned int get_n_in(unsigned int k){ return inports[k].size();};
    unsigned int get_select_out(unsigned int k){ return select_out[k];};
    unsigned int get_select_in(unsigned int k){ return select_in[k];};
    double get_mute(unsigned int kin){ return muted[kin];};
    unsigned int get_num_outputs(){ return n_out_;};
    unsigned int get_num_inputs(){ return n_in_;};
    std::vector<unsigned int> get_outports(unsigned int k){ return outports[k];};
    std::vector<unsigned int> get_inports(unsigned int k){return inports[k];};
    void lock(){pthread_mutex_lock( &mutex );};
    void unlock(){pthread_mutex_unlock( &mutex );};
    bool ismodified(observer_t* o){bool btmp(modified[o]);modified[o]=false;return btmp;};
    void add_observer(observer_t* o);
    void rm_observer(observer_t* o);
    void modify();
  protected:
    virtual void modified_mute(unsigned int kin, double mute) {};
    virtual void modified_gain(unsigned int kout, unsigned out, double g) {};
    virtual void modified_pan(unsigned int kout, unsigned out, double p) {};
    virtual void modified_gain_out(unsigned int k, double g) {};
    virtual void modified_outports(unsigned int k, const std::vector<unsigned int>& p) {};
    virtual void modified_inports(unsigned int k, const std::vector<unsigned int>& p) {};
    virtual void modified_select_out(unsigned int k, unsigned int n) {};
    virtual void modified_select_in(unsigned int k, unsigned int n) {};
    unsigned int size_;
    unsigned int n_out_;
    unsigned int n_in_;
    std::vector<double> G;
    std::vector<double> P;
    std::vector<double> Gout;
    std::vector<double> muted;
    std::vector<unsigned int> select_out;
    std::vector<unsigned int> select_in;
    std::vector<std::vector<unsigned int> > outports;
    std::vector<std::vector<unsigned int> > inports;
    pthread_mutex_t mutex;
    std::map<observer_t*,bool> modified;
    //bool modified;
  };

  class namematrix_t : public gainmatrix_t {
  public:
    namematrix_t(unsigned int nout, unsigned int nin);
    void set_name_in(unsigned int kin, const std::string& name);
    void set_name_out(unsigned int kout, const std::string& name);
    std::string get_name_in(unsigned int kin);
    std::string get_name_out(unsigned int kout);
  protected:
    virtual void modified_name_in(unsigned int k,const std::string& name){};
    virtual void modified_name_out(unsigned int k,const std::string& name){};
    std::vector<std::string> name_in;
    std::vector<std::string> name_out;
  };

  class lo_matrix_t : public namematrix_t 
  {
  public:
    lo_matrix_t(unsigned int nout, unsigned int nin, lo_server_thread& ilost, lo_address& iaddr);
    virtual ~lo_matrix_t();
  protected:
    virtual void modified_mute(unsigned int kin, double mute);
    virtual void modified_gain_out(unsigned int k, double g);
    virtual void modified_gain(unsigned int kout, unsigned out, double g);
    virtual void modified_pan(unsigned int kout, unsigned out, double p);
    virtual void modified_name_in(unsigned int k, const std::string& name);
    virtual void modified_name_out(unsigned int k, const std::string& name);
    virtual void modified_outports(unsigned int k, const std::vector<unsigned int>& p);
    virtual void modified_inports(unsigned int k, const std::vector<unsigned int>& p);
    virtual void modified_select_out(unsigned int k, unsigned int n);
    virtual void modified_select_in(unsigned int k, unsigned int n);
  private:
    static int osc_set_name_in(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    static int osc_set_name_out(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    static int osc_set_gain_out(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    static int osc_set_gain(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    static int osc_set_pan(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    static int osc_set_mute(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    static int osc_set_inports(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    static int osc_set_outports(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    static int osc_set_select_in(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    static int osc_set_select_out(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    lo_server_thread& lost;
    lo_address& addr;
  };

  namematrix_t* load(const std::string& fname);
  void save(namematrix_t* m,const std::string& fname);

};

#endif


/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
