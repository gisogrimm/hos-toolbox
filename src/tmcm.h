#ifndef TMCM_H
#define TMCM_H

#include <exception>
#include <stdint.h>
#include <string>

class tmcm_error : public std::exception {
public:
  tmcm_error(uint8_t err_no);
  const char* what() const throw();

private:
  uint32_t errno_;
};

class str_error : public std::exception {
public:
  str_error(const std::string& msg) : msg_(msg){};
  ~str_error() throw(){};
  const char* what() const throw() { return msg_.c_str(); };

private:
  std::string msg_;
};

class tmcm6110_t {
public:
  enum dir_t { left, right };
  tmcm6110_t();
  ~tmcm6110_t();
  void dev_open(const std::string& devname);
  void dev_close();
  void set_module_addr(uint8_t ma);
  uint32_t write_cmd(uint8_t cmd_no, uint8_t type_no, uint8_t motor_no,
                     uint32_t value);
  void mot_rotate(uint8_t motor, uint32_t vel, dir_t dir);
  void mot_stop(uint8_t motor);
  void mot_rfs(uint8_t motor);
  int32_t mot_get_rfs(uint8_t motor);
  void mot_moveto(uint8_t motor, int32_t pos);
  void mot_moveto_rel(uint8_t motor, int32_t pos);
  void mot_sap(uint8_t motor, uint8_t cmd, uint32_t value);
  int32_t mot_gap(uint8_t motor, uint8_t cmd);
  int32_t mot_get_pos(uint8_t motor);
  bool mot_get_target_reached(uint8_t motor);
  bool get_target_reached();

private:
  int configure_port(int);
  int device;
  uint8_t module_addr;
  pthread_mutex_t mtx;
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
