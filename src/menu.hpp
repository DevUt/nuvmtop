#include "base_win.hpp"
#include "common.hpp"

#include <ncurses.h>
#include <string>
#include <vector>

class MenuBar : public BaseWindow {
private:
  std::vector<std::string> menuOptions;
  int highlightedOption = 0;
  int begY, begX;

public:
  MenuBar(int nlines, int ncols, int begY, int begX)
      : BaseWindow(nlines, ncols, begY, begX) {
    draw_rectangle(getWindow(), 0, 0, nlines, ncols);
    getbegyx(getWindow(), begY, begX);
    wrefresh(getWindow());
  }

  void addOptions(std::string optionName) { menuOptions.push_back(optionName); }

  void addOptions(std::vector<std::string> vec) {
    menuOptions.insert(menuOptions.end(), vec.begin(), vec.end());
  }

  void display() {
    if (menuOptions.size() == 0) {
      return;
    }

    int choice = 0;
    int xprint = begX;
    for (size_t i = 0; i < menuOptions.size(); i++) {
      xprint++; // For the space;
      if (i == highlightedOption)
        wattron(getWindow(), A_REVERSE);
      mvwprintw(getWindow(), begY + 1, xprint, "%s", menuOptions[i].c_str());
      xprint += menuOptions[i].length();
      wattroff(getWindow(), A_REVERSE);
    }
    wrefresh(getWindow());
  }

  void handleInput(int ch) override {
    switch (ch) {
    case KEY_LEFT:
      // printw("Got key left");
      highlightedOption = std::max(0, highlightedOption - 1);
      break;
    case KEY_RIGHT:
      // printw("got key right");
      highlightedOption =
          std::min(highlightedOption + 1, (int)menuOptions.size() - 1);
      break;
    default:;
      // printw("Got %d", ch);
    }
    display();
  }

  std::string getCurrentSelectedOption() {
    return menuOptions[highlightedOption];
  }
};
