#pragma once 

#include "data_puller_internal.hpp"
#include <filesystem>
#include <array>
#include <sched.h>

class DataPuller {
private:
  control_fetch_params proc_fetch; 
  int enable_tracker(control_fetch_params *process);
  int pid_fd = -1;
  std::array<int, NVIDIA_MAX_PROCESSOR+1> uvm_tools_fd;
  void printGPUInfo(std::fstream&, int);
  void printCPUInfo(std::fstream&);

public:
  ModeType current_mode;
  DataPuller(pid_t pid);
  ~DataPuller();
  void printInfo(std::fstream &);
  void updateValues();

};
