#pragma once
#include <array>
#include <ncurses.h>

static void genColorPairs() {
  std::array all_colors = {COLOR_BLACK, COLOR_WHITE,   COLOR_CYAN, COLOR_YELLOW,
                           COLOR_GREEN, COLOR_MAGENTA, COLOR_RED};
  for (int i = 1; auto &color1 : all_colors) {
    for (auto &color2 : all_colors) {
      init_pair(i, color1, color2);
    }
  }
}
