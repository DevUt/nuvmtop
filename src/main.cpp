#include "main.hpp"
#include "cgroupInfoWin.hpp"
#include "data_puller.hpp"
#include "menu.hpp"
#include "uvmtopWin.hpp"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <memory>
#include <ncurses.h>
#include <string>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

int check_colors() {
  if (!has_colors()) {
    printw("The terminal doesn't support colors!");
    getch();
    return -1;
  }
  return 0;
}

void exit_prog(int exit_code) {
  endwin();
  exit(exit_code);
}

void graphic() {
  initscr();
  cbreak();
  noecho();

  if (0 > check_colors()) {
    exit_prog(EXIT_FAILURE);
  }
  start_color();
  keypad(stdscr, true);
  refresh();

  int maxY, maxX;
  int begY, begX;
  getmaxyx(stdscr, maxY, maxX);
  getbegyx(stdscr, begY, begX);

  std::unique_ptr<MenuBar> menuBar =
      std::make_unique<MenuBar>(3, maxX, begY, begX);
  std::unique_ptr<cgroupInfoWindow> cIWin =
      std::make_unique<cgroupInfoWindow>(maxY - 3, maxX, begY + 3, begX);
  std::unique_ptr<uvmTopWindow> uvmTopWin =
      std::make_unique<uvmTopWindow>(maxY - 3, maxX, begY + 3, begX);

  // Start the focus with menu bar
  BaseWindow *currentFocusedWindow = menuBar.get();
  BaseWindow *currentContentWindow = uvmTopWin.get();

  const std::vector<std::string> windowNames({"UVMTop", "CGroupInfo"});
  menuBar->addOptions(windowNames);
  menuBar->display();
  currentContentWindow->display();

  int ch;
  while ((ch = getch())) {
    switch (ch) {
    case '\t': {
      if (currentFocusedWindow == menuBar.get()) {
        currentFocusedWindow = currentContentWindow;
      } else {
        currentFocusedWindow = menuBar.get();
      }
      currentFocusedWindow->display();
      break;
    }
    case '\n': {
      if (currentFocusedWindow == menuBar.get()) {
        std::string currentSelected = menuBar->getCurrentSelectedOption();
        if (currentSelected == windowNames[0]) {
          if (currentFocusedWindow == uvmTopWin.get())
            continue;
          wclear(currentContentWindow->getWindow());
          currentContentWindow = uvmTopWin.get();
        } else if (currentSelected == windowNames[1]) {
          if (currentFocusedWindow == cIWin.get())
            continue;
          wclear(currentContentWindow->getWindow());
          currentContentWindow = cIWin.get();
        }
        currentContentWindow->display();
      }
    }
    default:
      currentFocusedWindow->handleInput(ch);
    }
  }
}

volatile std::sig_atomic_t turnOff = false;
void quitNonTui(int signal) { turnOff = true; }

void usage() {
  std::cout << "usage: ./nuvmtop --no-tui [--csv] [--poll-time | p] [--watch | "
               "-w] [--outfile "
               "| -o ] [--only-last]  [--help | h] \n";
  std::cout << "--no-tui\t" << "Don't use the under construction TUI\n";
  std::cout << "--csv\t\t" << "Print in CSV format\n";
  std::cout << "--poll-time\t"
            << "Query the time at THIS millisecond intervals\n";
  std::cout
      << "--outfile\t"
      << "Output to this file. Tip: use /dev/stdout for on TERM display\n";
  std::cout << "--only-last\t" << "Store only the last received value\n";
  std::cout
      << "--watch\t\t"
      << "Watch the process and print to oufile\n \t\t\t Note that this will "
         "automatically save the state just before the process exits\n";
}

int main(int argc, char *argv[]) {
  int tuiMode = true;
  int watchEff = false;
  int onlyLast = false;
  int csvMode = false;
  unsigned int pollTime = 0;
  namespace fs = std::filesystem;
  fs::path outfile;
  const struct option cmd[] = {{"no-tui", no_argument, &tuiMode, 0},
                               {"help", no_argument, nullptr, 'h'},
                               {"poll-time", required_argument, nullptr, 'p'},
                               {"csv", no_argument, &csvMode, 1},
                               {"outfile", required_argument, nullptr, 'o'},
                               {"only-last", no_argument, &onlyLast, 1},
                               {"watch", no_argument, nullptr, 'w'},
                               {nullptr, 0, nullptr, 0}};
  int opt;
  int opt_idx;
  while ((opt = getopt_long(argc, argv, "ho:wp:", cmd, &opt_idx)) != -1) {
    switch (opt) {
    case 'h':
      tuiMode = false;
      usage();
      exit(EXIT_SUCCESS);
    case 'o':
      outfile = fs::path(optarg);
      break;
    case 'w':
      watchEff = true;
      break;
    case 'p':
      try {
        pollTime = std::stoi(optarg);
      } catch (...) {
        std::cerr << "Invalid poll time interval!\n";
        exit(EXIT_FAILURE);
      }
      break;
    case '?':
      // std::cout << "Wrong option\n";
      usage();
      exit(EXIT_FAILURE);
    }
  }
  if (tuiMode) {
    graphic();
    exit(EXIT_SUCCESS);
  }

  // No tui mode here
  // Currently no checks here but yeah add in future
  std::signal(SIGHUP, quitNonTui);
  std::signal(SIGINT, quitNonTui);
  std::signal(SIGTERM, quitNonTui);

  std::array<unsigned int, MAX_UVM_PIDS> uvm_pids = {};
  UVM_TOOLS_GET_UVM_PIDS_PARAMS pid_query;
  pid_query.tablePtr = (unsigned long)uvm_pids.data();

  // UVM_TOOLS_GET_UVM_PIDS_PARAMS pid_query;
  // pid_query.tablePtr = (unsigned long)uvm_pids;

  int uvm_tools_fd =
      openat(AT_FDCWD, NVIDIA_UVM_TOOLS_PATH, O_RDWR | O_CLOEXEC);
  // std::cout<<"Opened "<<uvm_tools_fd<<'\n';
  std::unordered_map<pid_t, std::shared_ptr<DataPuller>> pidMap;
  std::fstream outStream(outfile, std::fstream::out | std::fstream::trunc);
  outStream.flush();

  constexpr std::array attributes{"PID",
                                  "Processor Id",
                                  "Number of Faults",
                                  "Evictions",
                                  "Resident Pages",
                                  "Physical Memory Allocated",
                                  "Memory Evicted of other Processes",
                                  "Thrased Pages"};

  if (csvMode) {
    for (size_t i = 0; i < attributes.size(); i++) {
      outStream << attributes[i] << ",\n"[i == attributes.size() - 1];
    }
  }

  while (true) {
    if (onlyLast) {
      outStream =
          std::fstream(outfile, std::fstream::out | std::fstream::trunc);
      outStream.flush();
      if (csvMode) {
        for (size_t i = 0; i < attributes.size(); i++) {
          outStream << attributes[i] << ",\n"[i == attributes.size() - 1];
        }
      }
    }
    int ret = ioctl(uvm_tools_fd, UVM_TOOLS_GET_UVM_PIDS, (void *)(&pid_query));
    if (ret == -1) {
      std::cerr << "Error!\n"
                << "Couldn't find UVM TOOLS Device\n"
                << "Following maybe the reasons:\n"
                << "\t1. You have not initalized atleast one process, just use "
                   "nvtop and rerun this.\n"
                << "\t2. You are not using the correct patched driver\n"
                << "\t3. The gods are unhappy\n.";
      break;
    }
    for (auto &x : uvm_pids) {
      if (x) {
        if (!pidMap.contains(x)) {
          std::shared_ptr<DataPuller> puller = std::make_shared<DataPuller>(x);
          if (puller->current_mode == NON_USABLE) {
            // std::cerr<<"Nonusable puller\n";
            continue;
          }
          pidMap[x] = std::move(puller);
        }
        if(pidMap[x]->current_mode != DEAD_PROC)
          pidMap[x]->updateValues();
      }
    }
    for (auto &[pid, puller] : pidMap) {
      if(std::find(uvm_pids.begin(), uvm_pids.end(), pid) == uvm_pids.end())
        puller->destruct();
      puller->printInfo(outStream, csvMode);
      if (!csvMode)
        outStream << "---------------\n";
    }
    outStream.flush();
    if (turnOff || (!watchEff)) {
      break;
    }
    if (pollTime)
      std::this_thread::sleep_for(std::chrono::milliseconds(pollTime));
  }
  close(uvm_tools_fd);
  // std::cout<<"Closed "<<uvm_tools_fd<<'\n';

  std::cout << "\n#####################\n";
}
