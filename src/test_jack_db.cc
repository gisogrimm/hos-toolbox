#include "hos_defs.h"
#include "jackclient.h"
#include <iostream>
#include <stdio.h>

class test_t : public jackc_db_t {
public:
  test_t();
  virtual int inner_process(jack_nframes_t n,
                            const std::vector<float*>& inBuffer,
                            const std::vector<float*>& outBuffer);
};

test_t::test_t() : jackc_db_t("test", 8192) {}

int test_t::inner_process(jack_nframes_t n, const std::vector<float*>& inBuffer,
                          const std::vector<float*>& outBuffer)
{
  DEBUG("inner");
  for(uint32_t kb = 0; kb < std::min(inBuffer.size(), outBuffer.size()); kb++)
    for(uint32_t k = 0; k < n; k++)
      outBuffer[kb][k] = inBuffer[kb][k];
  return 0;
}

int main(int argc, char** argv)
{
  DEBUG(1);
  test_t t;
  DEBUG(1);
  t.add_input_port("in_1");
  DEBUG(1);
  t.add_output_port("out_1");
  DEBUG(1);
  t.add_input_port("in_2");
  DEBUG(1);
  t.add_output_port("out_2");
  DEBUG(1);
  t.activate();
  DEBUG(1);
  char s[1024];
  gets(s);
  DEBUG(1);
  t.deactivate();
  DEBUG(1);
  return 0;
}
