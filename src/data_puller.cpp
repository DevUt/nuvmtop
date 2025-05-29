#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
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

void run() { UVM_TOOLS_GET_UVM_PIDS_PARAMS ioctl_input; }

class DataPuller {
private:
  /**
   * @brief This computes all the parameters for a GIVEN process
   *
   * @param[inout] arg Expected to provide pid of the process to compute the
   * values
   * @return -1 on error else arg will contain the param values
   */
  int compute_parameters() {

    control_fetch_params *process = (control_fetch_params *)arg;
    uuid *uuids = (uuid *)calloc(NVIDIA_MAX_PROCESSOR, sizeof(uuid));

    UVM_TOOLS_INIT_EVENT_TRACKER_PARAMS event_tracker;
    UVM_TOOLS_GET_PROCESSOR_UUID_TABLE_PARAMS ioctl_input;
    UVM_TOOLS_GET_CURRENT_COUNTER_VALUES_PARAMS current_value_fetch;
    int pid = process->pid;
    int pid_fd = syscall(SYS_pidfd_open, process->pid, 0);

    int process_uvm_fd = get_uvm_fd(process->pid);

    if (process_uvm_fd == -1) {
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

    ioctl_input.tablePtr = (unsigned long)uuids;
    volatile int process_fd =
        syscall(SYS_pidfd_getfd, pid_fd, process_uvm_fd, 0);
    ioctl_input.uvmFd = process_fd;
    int uvm_tools_fd[NVIDIA_MAX_PROCESSOR + 1];

    uvm_tools_fd[0] =
        openat(AT_FDCWD, NVIDIA_UVM_TOOLS_PATH, O_RDWR | O_CLOEXEC);
    ioctl(uvm_tools_fd[0], UVM_IOCTL_TOOLS_GET_GPUs_UUID, (void *)&ioctl_input);

    for (int i = 0; i < NVIDIA_MAX_PROCESSOR; i++) {

      if (uuids[i].uuid[0] && !process->is_event_tracker_setup[i]) {
        uvm_tools_fd[i + 1] =
            openat(AT_FDCWD, NVIDIA_UVM_TOOLS_PATH, O_RDWR | O_CLOEXEC);

        process->counterbuffer[i] = (unsigned long int *)aligned_alloc(
            4096, 16 * sizeof(unsigned long int));
        current_value_fetch.device_id = i;
        current_value_fetch.tablePtr = (unsigned long)process->counterbuffer[i];

        event_tracker.allProcessors = 0;
        event_tracker.controlBuffer = (unsigned long)process->counterbuffer[i];
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
    return 0;
  }

public:
  enum { USABLE, IN_ERROR } currentStatus;
  DataPuller() {}
};
