/*
 * time-varying phase based parameter looping machine
 */

class parloop_t : public jackc_t {
public:
  parloop_t(const std::string& jname);
  void run();

private:
  double looplength;
  double uwphase;
};

int main(int argc, char** argv)
{
  if(argc < 1)
    throw TASCAR::ErrMsg(std::string("Usage: ") + argv[0] + " ");
  std::string jname("parloop");
  if(argc > 2)
    jname = argv[2];
  parloop_t s("parloop");
  s.run();
}
