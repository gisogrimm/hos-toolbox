#include <lo/lo.h>
#include <stdlib.h>
#include <unistd.h>
#include "libhos_music.h"
#include <map>

double drand()
{
  return (double)random()/(double)(RAND_MAX+1.0);
}

class pdf_t {
public:
  pdf_t();
  void update();
  double rand();
  void add(double v,double p);
private:
  std::map<double,double> pdf;
  std::map<double,double> icdf;
};

pdf_t::pdf_t()
{
}

void pdf_t::update()
{
  icdf.clear();
  double psum(0);
  for( std::map<double,double>::iterator it=pdf.begin();it!=pdf.end();++it)
    psum += it->second;
  for( std::map<double,double>::iterator it=pdf.begin();it!=pdf.end();++it)
    it->second /= psum;
  double p(0);
  for( std::map<double,double>::iterator it=pdf.begin();it!=pdf.end();++it){
    p+=it->second;
    icdf[p] = it->first;
  }
}

double pdf_t::rand()
{
  if( icdf.empty() )
    return 0;
  std::map<double,double>::iterator it(icdf.lower_bound(drand()));
  if( it == icdf.end() )
    return icdf.rbegin()->second;
  return it->second;
}

void pdf_t::add(double v,double p)
{
  pdf[v] = p;
}



int main(int argc, char** argv)
{
  pdf_t p;
  p.add(0,1);
  p.add(2,1);
  p.add(3,1);
  p.add(4,1);
  p.add(5,1);
  p.add(7,5);
  p.update();
  int hist[8];
  for(unsigned int k=0;k<8;k++)
    hist[k] = 0;
  for(unsigned int k=0;k<4000000;k++){
    double v(p.rand());
    hist[(unsigned int)v]++;
    //std::cout << p.rand() << std::endl;
  }
  std::cout << "----\n";
  int sum(0);
  for(unsigned int k=0;k<8;k++)
    sum += hist[k];
  for(unsigned int k=0;k<8;k++)
    std::cout << k << " " << hist[k] << " " << (double)(hist[k])/(double)sum << std::endl;
  //return 0;
  lo_address lo_addr(lo_address_new_from_url( "osc.udp://239.255.1.7:9877/" ));
  lo_address_set_ttl( lo_addr, 1 );
  double time(0);
  note_t n[5];
  for(unsigned int k=0;k<5;k++){
    n[k].time = 0;
  }
  int c_pitch[5] = {11,11,0,0,-11};
  lo_send(lo_addr,"/clear","");
  while(true){
    usleep(20000);
    time += 0.005;
    lo_send(lo_addr,"/time","f",time);
    for(unsigned int k=0;k<5;k++){
      if( n[k].end_time() <= time+1.0 ){
        n[k].time = n[k].end_time();
        //n[k].length = 7.0*rand()/RAND_MAX;
        n[k].length = 1+3.0*rand()/RAND_MAX;
        n[k].pitch += (12.0*rand()/RAND_MAX-6.0);
        n[k].pitch += 0.3*(c_pitch[k]-n[k].pitch)*rand()/RAND_MAX;
        lo_send(lo_addr,"/note","iiif",k,n[k].pitch,n[k].length,n[k].time);
        //std::cout << "k=" << k << " " << n[k] << std::endl;
      }
    }
  }
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
