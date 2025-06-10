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
ModeType current_mode = SNAPSHOT_MODE;

int get_uvm_fd(int pid) {
  int nvidia_uvm_fd = -1;

  DIR *d;
  struct dirent *dir;
  char psf_path[2048];
  char proc_dir[1024];
  char *psf_realpath;

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
        if (nvidia_uvm_fd >= 0)
          break;
      }
    }
    closedir(d);
  }
  return nvidia_uvm_fd;
}

int DataPuller::compute_parameters(control_fetch_params *process) {

  // uuid *uuids = (uuid *)calloc(NVIDIA_MAX_PROCESSOR, sizeof(uuid));
  std::unique_ptr<uuid[]> uuids(new uuid[NVIDIA_MAX_PROCESSOR]());

  UVM_TOOLS_INIT_EVENT_TRACKER_PARAMS event_tracker;
  UVM_TOOLS_GET_PROCESSOR_UUID_TABLE_PARAMS ioctl_input;
  UVM_TOOLS_GET_CURRENT_COUNTER_VALUES_PARAMS current_value_fetch;
  pid_fd = syscall(SYS_pidfd_open, process->pid, 0);

  int process_uvm_fd = get_uvm_fd(process->pid);

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
  volatile int process_fd = syscall(SYS_pidfd_getfd, pid_fd, process_uvm_fd, 0);
  ioctl_input.uvmFd = process_fd;
  std::fill(uvm_tools_fd.begin(), uvm_tools_fd.end(), -1);

  uvm_tools_fd[0] = openat(AT_FDCWD, NVIDIA_UVM_TOOLS_PATH, O_RDWR | O_CLOEXEC);
  ioctl(uvm_tools_fd[0], UVM_IOCTL_TOOLS_GET_GPUs_UUID, (void *)&ioctl_input);

  for (int i = 0; i < NVIDIA_MAX_PROCESSOR; i++) {

    if (uuids[i].uuid[0] && !process->is_event_tracker_setup[i]) {
      uvm_tools_fd[i + 1] =
          openat(AT_FDCWD, NVIDIA_UVM_TOOLS_PATH, O_RDWR | O_CLOEXEC);

      process->counterbuffer[i] = make_aligned_array(16);
      current_value_fetch.device_id = i;
      current_value_fetch.tablePtr =
          (unsigned long)process->counterbuffer[i].get();

      event_tracker.allProcessors = 0;
      event_tracker.controlBuffer =
          (unsigned long)process->counterbuffer[i].get();
      event_tracker.queueBufferSize = 0;
      event_tracker.processor = uuids[i];
      event_tracker.uvmFd = process_fd;
      ioctl(uvm_tools_fd[i + 1], UVM_IOCTL_TOOLS_INIT_EVENT_TRACKER,
            (void *)&event_tracker);

      process->is_event_tracker_setup[i] = 1;

      ioctl(uvm_tools_fd[i + 1], UVM_TOOLS_GET_CURRENT_COUNTER_VALUES,
            (void *)&current_value_fetch);

      ioctl(uvm_tools_fd[i + 1], UVM_IOCTL_TOOLS_ENABLE_COUNTERS,
            (void *)&counter_enable);
    }
  }
  close(process_fd);
  return 0;
}

void DataPuller::printInfo(std::filesystem::path outfile) {
  using namespace std;
  fstream outStream(outfile);
  outStream << "Pid: " << proc_fetch.pid << '\n';
  for (int i = 1; i < NVIDIA_MAX_PROCESSOR; i++) {
    if (proc_fetch.is_event_tracker_setup[i]) {
      outStream << "Processor ID: " << i << '\n';
      outStream << "Number of Faults: " << proc_fetch.counterbuffer[i][9]
                << '\n';
      outStream << "Evictions: " << proc_fetch.counterbuffer[i][10] << '\n';
      outStream << "Resident Pages: " << proc_fetch.counterbuffer[i][11]
                << '\n';
      outStream << "Physical Memory Allocated: "
                << proc_fetch.counterbuffer[i][13] << '\n';
      outStream << "Memory Evicted of other Processes: "
                << proc_fetch.counterbuffer[i][14] << '\n';
      outStream << "Thrashed Pages: " << proc_fetch.counterbuffer[i][15]
                << '\n';
    }
  }
}

DataPuller::DataPuller(pid_t pid) {
  proc_fetch.pid = pid;
  proc_fetch.counterbuffer.resize(NVIDIA_MAX_PROCESSOR);
  proc_fetch.is_event_tracker_setup.resize(NVIDIA_MAX_PROCESSOR);

  // Enable the tracker and fetch value once
  compute_parameters(&proc_fetch);
}

DataPuller::~DataPuller() {
  for (auto &x : uvm_tools_fd) {
    if (x != -1) {
      close(x);
    }
  }
  if (pid_fd != -1){
    close(pid_fd);
  }
}
