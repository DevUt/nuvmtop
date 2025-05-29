#include "main.hpp"
#include "cgroupInfoWin.hpp"
#include "menu.hpp"
#include "uvmtopWin.hpp"
#include <cstdlib>
#include <memory>
#include <ncurses.h>
#include <string>
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

int main() {
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
