#include "main.hpp"
#include "cgroupInfoWin.hpp"
#include "data_puller.hpp"
#include "menu.hpp"
#include "uvmtopWin.hpp"

#include <cstdlib>
#include <fcntl.h>
#include <getopt.h>
#include <iostream>
#include <memory>
#include <ncurses.h>
#include <string>
#include <sys/ioctl.h>
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

int main(int argc, char *argv[]) {
  int tuiMode = true;
  namespace fs = std::filesystem;
  fs::path outfile;
  const struct option cmd[] = {{"no-tui", no_argument, &tuiMode, 0},
                               {"help", no_argument, nullptr, 'h'},
                               {"outfile", no_argument, nullptr, 'o'},
                               {nullptr, 0, nullptr, 0}};
  int opt;
  int opt_idx;
  while ((opt = getopt_long(argc, argv, "h:o:", cmd, &opt_idx)) != -1) {
    if (opt == 0) {
      // flag is set
      continue;
    }
    switch (opt) {
    case 'h':
      tuiMode = false;
      std::cout << "usage: ./nuvmtop --no-tui --help\n";
      exit(EXIT_SUCCESS);
    case 'o':
      outfile = fs::path(optarg);
      if (!fs::exists(outfile)) {
        std::cout << "Output file " << outfile << " does not exist!\n";
        exit(EXIT_FAILURE);
      }
      break;
    case '?':
      std::cout << "Wrong option\n";
      exit(EXIT_FAILURE);
    }
  }
  if (tuiMode) {
    graphic();
    exit(EXIT_SUCCESS);
  }

  // No tui mode here
  // Currently no checks here but yeah add in future

  std::array<unsigned int, MAX_UVM_PIDS> uvm_pids = {};
  UVM_TOOLS_GET_UVM_PIDS_PARAMS pid_query;
  pid_query.tablePtr = (unsigned long)uvm_pids.data();

  // UVM_TOOLS_GET_UVM_PIDS_PARAMS pid_query;
  // pid_query.tablePtr = (unsigned long)uvm_pids;

  int uvm_tools_fd =
      openat(AT_FDCWD, NVIDIA_UVM_TOOLS_PATH, O_RDWR | O_CLOEXEC);
  // std::cout<<"Opened "<<uvm_tools_fd<<'\n';
  int ret = ioctl(uvm_tools_fd, UVM_TOOLS_GET_UVM_PIDS, (void *)(&pid_query));
  if (ret == -1) {
    std::cout << "Error!\n";
    return 0;
  }
  for(auto&x : uvm_pids) {
    if(x) {
      DataPuller info(x);
      info.printInfo(outfile);
      std::cout<<"---------------\n";
    }
  }
  close(uvm_tools_fd);
  // std::cout<<"Closed "<<uvm_tools_fd<<'\n';

  std::cout << "\n";
}
