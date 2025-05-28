#include <iostream>
#include <ncurses.h>

int main() {
  initscr();
  if (stdscr == NULL) { /* error handling */
    return 1;
  }
  cbreak();
  noecho();
  keypad(stdscr, TRUE);

  // *** ADD COLOR INITIALIZATION ***
  if (has_colors()) { // Check if terminal supports colors
    start_color();    // Start color functionality
    // Define a color pair: 1 = Red foreground on Black background
    init_pair(1, COLOR_RED, COLOR_BLACK);
    // Define another pair: 2 = White foreground on Blue background for the
    // window
    init_pair(2, COLOR_WHITE, COLOR_BLUE);
  }

  int maxY, maxX;
  getmaxyx(stdscr, maxY, maxX);
  int nlines = 5;
  int ncols = maxX / 2;
  int begY = (maxY / 2) - (nlines / 2);
  int begX = (maxX / 2) - (ncols / 2);

  refresh();

  WINDOW *testWindow = newwin(nlines, ncols, begY, begX);
  if (testWindow == NULL) { /* error handling */
    return 1;
  }

  // *** APPLY COLOR TO THE WINDOW AND TEXT ***
  if (has_colors()) {
    wbkgd(testWindow, COLOR_PAIR(2));   // Set background color for the window
    wattron(testWindow, COLOR_PAIR(1)); // Turn on red text for next prints
  }

  box(testWindow, 0,
      0); // Still uses default chars, but window background is blue

  mvwprintw(testWindow, 1, 1, "Hello Ncurses!");
  mvwprintw(testWindow, 2, 1, "Box Test");

  if (has_colors()) {
    wattroff(testWindow, COLOR_PAIR(1)); // Turn off red text
  }

  wrefresh(testWindow);
  // refresh(); // Still recommend this

  getch();
  delwin(testWindow);
  endwin();
  return 0;
}
