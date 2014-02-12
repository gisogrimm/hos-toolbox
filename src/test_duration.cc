#include "libhos_music.h"

int main(int argc, char** argv)
{
  for(uint32_t k=0;k<13;k++)
    std::cout << k << " " << duration(k) << std::endl;
  return 0;
}
