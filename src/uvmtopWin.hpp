#pragma once
#include "base_win.hpp"
#include "common.hpp"
#include <ncurses.h>
#include <string>

class uvmTopWindow : public BaseWindow {
public:
  uvmTopWindow(int nlines, int ncols, int begY, int begX)
      : BaseWindow(nlines, ncols, begY, begX) {
    // printw("%d %d %d %d", nlines, ncols, begY, begX);
    wrefresh(getWindow());
  }
  void display() override {
    int maxY, maxX;
    getmaxyx(getWindow(), maxY, maxX);
    draw_rectangle(getWindow(), 0, 0, maxY, maxX);
    std::string focusString = "Currently in work";
    mvwprintw(getWindow(), maxY/2, maxX/2 - focusString.length()/2, "%s", focusString.c_str());
    wrefresh(getWindow());
  }

  void handleInput(int ch) override { ; }
};
