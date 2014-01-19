#include <lo/lo.h>
#include <stdlib.h>
#include <unistd.h>

#include "libhos_music.h"

int main(int argc, char** argv)
{
  lo_address lo_addr(lo_address_new_from_url( "osc.udp://239.255.1.7:9877/" ));
  lo_address_set_ttl( lo_addr, 1 );
  double time(0);
  note_t n[5];
  for(unsigned int k=0;k<5;k++){
    n[k].time = 0;
  }
  int c_pitch[5] = {11,11,0,0,-11};
  while(true){
    usleep(20000);
    time += 0.005;
    lo_send(lo_addr,"/time","f",time);
    for(unsigned int k=0;k<5;k++){
      if( n[k].end_time() <= time+2.0 ){
        n[k].time = n[k].end_time();
        n[k].length = 7.0*rand()/RAND_MAX;
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
