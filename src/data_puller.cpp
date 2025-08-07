#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "data_puller.hpp"

std::array<unsigned int, MAX_UVM_PIDS> uvm_pids;

int get_uvm_fd(int pid, int pid_fd) {
  int nvidia_uvm_fd = -1;
  int tool_fd = openat(AT_FDCWD,NVIDIA_UVM_TOOLS_PATH, O_RDWR|O_CLOEXEC);

  DIR *d;
  struct dirent *dir;
  char psf_path[2048];
  char proc_dir[1024];
  char *psf_realpath;
  int fds[100];
  UVM_TOOLS_VALIDATE_FD_PARAMS validateFd;

  sprintf(proc_dir, "/proc/%d/fd", pid);
  d = opendir(proc_dir);
  if (d) {
    while ((dir = readdir(d)) != NULL) { // printf("%d\n",dir->d_type);
      if (dir->d_type == 10) {
        sprintf(psf_path, "%s/%s", proc_dir, dir->d_name);
        // printf("%s\n",psf_path);
        psf_realpath = realpath(psf_path, NULL);
        // printf("%s\n",psf_realpath);
        if (psf_realpath && strcmp(psf_realpath, NVIDIA_UVM_PATH) == 0) {
          nvidia_uvm_fd = atoi(dir->d_name);
        }
        free(psf_realpath);
        if (nvidia_uvm_fd >= 0) {
          validateFd.uvmFd = syscall(SYS_pidfd_getfd, pid_fd, nvidia_uvm_fd, 0);
          ioctl(tool_fd, UVM_TOOLS_VALIDATE_FD, (void *)&validateFd);

          if (validateFd.rmStatus == 0) {
            break;
          }
        }
      }
    }
    closedir(d);
  }
  close(tool_fd);
  return validateFd.uvmFd;
}

int DataPuller::enable_tracker(control_fetch_params *process) {

  // uuid *uuids = (uuid *)calloc(NVIDIA_MAX_PROCESSOR, sizeof(uuid));
  std::unique_ptr<uuid[]> uuids(new uuid[NVIDIA_MAX_PROCESSOR]());

  UVM_TOOLS_INIT_EVENT_TRACKER_PARAMS event_tracker;
  UVM_TOOLS_GET_PROCESSOR_UUID_TABLE_PARAMS ioctl_input;
  pid_fd = syscall(SYS_pidfd_open, process->pid, 0);

  int process_uvm_fd = get_uvm_fd(process->pid, pid_fd);

  if (process_uvm_fd == -1) {
    std::cout << "Couln't get fd\n";
    return -1;
  }

  UVM_TOOLS_ENABLE_COUNTERS_PARAMS counter_enable;

  counter_enable.counterTypeFlags =
      UVM_COUNTER_NAME_FLAG_BYTES_XFER_HTD |
      UVM_COUNTER_NAME_FLAG_BYTES_XFER_DTH |
      UVM_COUNTER_NAME_FLAG_CPU_PAGE_FAULT_COUNT |
      UVM_COUNTER_NAME_FLAG_GPU_PAGE_FAULT_COUNT |
      UVM_COUNTER_NAME_FLAG_GPU_EVICTION_COUNT |
      UVM_COUNTER_NAME_FLAG_GPU_RESIDENT_COUNT |
      UVM_COUNTER_NAME_FLAG_CPU_RESIDENT_COUNT |
      UVM_COUNTER_NAME_FLAG_GPU_MEMORY_ALLOCATED |
      UVM_COUNTER_NAME_FLAG_OTHER_PROCESS_GPU_MEMORY_EVICTED |
      UVM_COUNTER_NAME_THRASHING_PAGES;

  ioctl_input.tablePtr = (unsigned long)uuids.get();
  volatile int process_fd = process_uvm_fd;
  ioctl_input.uvmFd = process_fd;
  std::fill(uvm_tools_fd.begin(), uvm_tools_fd.end(), -1);

  uvm_tools_fd[0] = openat(AT_FDCWD, NVIDIA_UVM_TOOLS_PATH, O_RDWR | O_CLOEXEC);
  ioctl(uvm_tools_fd[0], UVM_IOCTL_TOOLS_GET_GPUs_UUID, (void *)&ioctl_input);

  int cnt = 0;
  for (int i = 0; i < NVIDIA_MAX_PROCESSOR; i++) {

    if (uuids[i].uuid[0] && !process->is_event_tracker_setup[i]) {
      uvm_tools_fd[i + 1] =
          openat(AT_FDCWD, NVIDIA_UVM_TOOLS_PATH, O_RDWR | O_CLOEXEC);

      process->counterbuffer[i] = make_aligned_array(16);

      event_tracker.allProcessors = 0;
      event_tracker.controlBuffer =
          (unsigned long)process->counterbuffer[i].get();
      event_tracker.queueBufferSize = 0;
      event_tracker.processor = uuids[i];
      event_tracker.uvmFd = process_fd;
      ioctl(uvm_tools_fd[i + 1], UVM_IOCTL_TOOLS_INIT_EVENT_TRACKER,
            (void *)&event_tracker);

      process->is_event_tracker_setup[i] = 1;

      ioctl(uvm_tools_fd[i + 1], UVM_IOCTL_TOOLS_ENABLE_COUNTERS,
            (void *)&counter_enable);
      cnt++;
    }
  }
  close(process_fd);
  return cnt;
}

void DataPuller::updateValues() {
  UVM_TOOLS_GET_CURRENT_COUNTER_VALUES_PARAMS current_value_fetch;
  for (int i = 0; i < NVIDIA_MAX_PROCESSOR; i++) {
    current_value_fetch.device_id = i;
    current_value_fetch.tablePtr =
        (unsigned long)proc_fetch.counterbuffer[i].get();
    ioctl(uvm_tools_fd[i + 1], UVM_TOOLS_GET_CURRENT_COUNTER_VALUES,
          (void *)&current_value_fetch);
  }
}

void DataPuller::printGPUInfo(std::fstream &outStream, int i,
                              bool onlyData = false) {
  if (onlyData) {
    outStream << proc_fetch.pid << ",";
    outStream << i << ",";
    outStream << proc_fetch.counterbuffer[i][9] << ',';
    outStream << proc_fetch.counterbuffer[i][10] << ',';
    outStream << proc_fetch.counterbuffer[i][11] << ',';
    outStream << proc_fetch.counterbuffer[i][13] << ',';
    outStream << proc_fetch.counterbuffer[i][14] << ',';
    outStream << proc_fetch.counterbuffer[i][15];
    outStream << "\n";
    return;
  }
  outStream << "Processor ID: " << i << '\n';
  outStream << "Number of Faults: " << proc_fetch.counterbuffer[i][9] << '\n';
  outStream << "Evictions: " << proc_fetch.counterbuffer[i][10] << '\n';
  outStream << "Resident Pages: " << proc_fetch.counterbuffer[i][11] << '\n';
  outStream << "Physical Memory Allocated: " << proc_fetch.counterbuffer[i][13]
            << '\n';
  outStream << "Memory Evicted of other Processes: "
            << proc_fetch.counterbuffer[i][14] << '\n';
  outStream << "Thrashed Pages: " << proc_fetch.counterbuffer[i][15] << '\n';
  outStream << "\n\n";
}

void DataPuller::printCPUInfo(std::fstream &outStream, bool onlyData = false) {
  int i = 0;
  if (onlyData) {
    outStream << proc_fetch.pid << ",";
    outStream << i << ',';
    outStream << proc_fetch.counterbuffer[i][2] << ',';
    outStream << 0 << ',';
    outStream << proc_fetch.counterbuffer[i][12] << ',';
    outStream << proc_fetch.counterbuffer[i][13] << ',';
    outStream << proc_fetch.counterbuffer[i][14] << ',';
    outStream << proc_fetch.counterbuffer[i][15];
    outStream << "\n";
    return;
  }
  outStream << "Processor ID: " << i << '\n';
  outStream << "Number of Faults: " << proc_fetch.counterbuffer[i][2] << '\n';
  outStream << "Evictions: " << 0 << '\n';
  outStream << "Resident Pages: " << proc_fetch.counterbuffer[i][12] << '\n';
  outStream << "Physical Memory Allocated: " << proc_fetch.counterbuffer[i][13]
            << '\n';
  outStream << "Memory Evicted of other Processes: "
            << proc_fetch.counterbuffer[i][14] << '\n';
  outStream << "Thrashed Pages: " << proc_fetch.counterbuffer[i][15] << '\n';
  outStream << "\n\n";
}

void DataPuller::printInfo(std::fstream &outStream, bool onlyData = false) {
  using namespace std;
  if (!onlyData)
    outStream << "Pid: " << proc_fetch.pid << "\n,"[onlyData];
  for (int i = 0; i < NVIDIA_MAX_PROCESSOR; i++) {
    if (proc_fetch.is_event_tracker_setup[i]) {
      if (i) {
        printGPUInfo(outStream, i, onlyData);
      } else {
        printCPUInfo(outStream, onlyData);
      }
    }
  }
}

DataPuller::DataPuller(pid_t pid) {
  proc_fetch.pid = pid;
  proc_fetch.counterbuffer.resize(NVIDIA_MAX_PROCESSOR);
  proc_fetch.is_event_tracker_setup.resize(NVIDIA_MAX_PROCESSOR);

  // Enable the tracker and fetch value once
  int ret = enable_tracker(&proc_fetch);

  // We have atleast one gpu
  current_mode = (ret > 1) ? USABLE : NON_USABLE;
}

void DataPuller::destruct() {
  for (auto &x : uvm_tools_fd) {
    if (x != -1) {
      close(x);
    }
  }
  if (pid_fd != -1) {
    close(pid_fd);
  }
  current_mode = DEAD_PROC;

}

DataPuller::~DataPuller() {
  destruct();
}
