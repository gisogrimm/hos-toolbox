#include <lo/lo.h>
#include <stdlib.h>
#include <unistd.h>
#include "libhos_music.h"
#include "libhos_random.h"

class ambitus_treble_t : public pdf_t {
public:
  ambitus_treble_t(){
    // usable ambitus ranges from g to a'':
    for(int32_t p=-5; p<22;p++)
      set(p,4);
    // prefer empty strings from c on:
    operator[](0) += 1.0;
    operator[](4) += 1.0;
    operator[](9) += 1.0;
    operator[](14) += 1.0;
    update();
  };
};

class ambitus_bass_t : public pdf_t {
public:
  ambitus_bass_t(){
    // usable ambitus ranges from g to a'':
    for(int32_t p=-17; p<10;p++)
      set(p,4);
    // prefer empty strings:
    operator[](-22) += 1.0;//D
    operator[](-17) += 1.0;//G
    operator[](-12) += 1.0;//c
    operator[](-8) += 1.0; //e
    operator[](-3) += 1.0; //a
    operator[](2) += 1.0;  //d'
    update();
  };
};

namespace Ambitus {
  
  const ambitus_treble_t treble;
  const ambitus_bass_t bass;

}

class major_t : public pdf_t {
public:
  major_t();
};

major_t::major_t()
{
  set(0,3);
  set(2,1);
  set(4,2); // major third
  set(5,1);
  set(7,2); // fifth
  set(9,1);
  set(11,1); // major seven
  update();
}

class composer_t {
public:
  composer_t(uint32_t num_voices=5);
private:
  int key;
};

int main(int argc, char** argv)
{
  major_t major;
  pdf_t p(major+0.5*major.vadd(1));
  //p.add(0,1);
  //p.add(2,1);
  //p.add(3,1);
  //p.add(4,1);
  //p.add(5,1);
  //p.add(7,5);
  //p.update();
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
        n[k].length = 1+5.0*rand()/RAND_MAX;
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
