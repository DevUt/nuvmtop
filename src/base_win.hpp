#pragma once
#include <ncurses.h>

class BaseWindow {
private:
  WINDOW *baseWindow;
  int nlines, ncols, begY, begX;
public:
  BaseWindow(int nlines, int ncols, int begY, int begX, WINDOW *win = NULL)
      : baseWindow(win), nlines(nlines), ncols(ncols), begY(begY), begX(begX) {
    baseWindow = newwin(nlines, ncols, begY, begX);
    keypad(baseWindow, true);
  }

  virtual ~BaseWindow() {
    if (baseWindow)
      delwin(baseWindow);
  }

  virtual void display() = 0;
  virtual void handleInput(int ch) = 0;
  WINDOW *getWindow() const { return baseWindow; }
};
