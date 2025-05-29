#pragma once
#include "base_win.hpp"
#include "common.hpp"
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ncurses.h>
#include <string>
#include <vector>

class cgroupInfoWindow : public BaseWindow {
private:
  int begY, begX;
  int maxY, maxX;

  size_t col0_mwidth;
  size_t col1_mwidth;
  size_t col2_mwidth;

  std::vector<std::string> headers;
  // break for scrolling
  int brokeOff = -1;
  typedef struct {
    std::string name;
    std::string soft_lim, hard_lim;
  } data;

  std::vector<data> dataToDisplay;
  std::mutex dataLock;
  std::atomic<bool> isThereAnUpdate = false;

  void populateData() {
    using recursive_directory_iterator =
        std::filesystem::recursive_directory_iterator;

    auto cgroupIter = recursive_directory_iterator("/sys/fs/cgroup/");
    for (auto &entry : cgroupIter) {
      if (cgroupIter.depth() > 1) {
        cgroupIter.disable_recursion_pending();
        continue;
      }
      if (entry.is_directory()) {
        std::filesystem::path softPath(entry.path().string() +
                                       "/uvm_ctrl.soft");
        std::filesystem::path hardPath(entry.path().string() +
                                       "/uvm_ctrl.hard");
        if (std::filesystem::exists(softPath) &&
            std::filesystem::exists(hardPath)) {
          data current = {.name = entry.path().filename()};
          std::ifstream softStream(softPath);
          std::ifstream hardStream(hardPath);

          // TODO : Fix this hacky way to get to the number
          for (int i = 0; i < 3; i++) {
            softStream >> current.soft_lim;
            hardStream >> current.hard_lim;
          }
          std::lock_guard lock(dataLock);
          dataToDisplay.push_back(current);
        }
      }
    }
  }

public:
  cgroupInfoWindow(int nlines, int ncols, int begY, int begX)
      : BaseWindow(nlines, ncols, begY, begX) {
    // printw("%d %d %d %d", nlines, ncols, begY, begX);
    getbegyx(getWindow(), this->begY, this->begX);
    getmaxyx(getWindow(), maxY, maxX);
    populateData();
    headers = {"CGroup Name", "Soft Limit", "Hard Limit"};
    col0_mwidth = headers[0].size();
    col1_mwidth = headers[1].size();
    col2_mwidth = headers[2].size();

    for (const auto &entry : dataToDisplay) {
      col0_mwidth = std::max(col0_mwidth, entry.name.size());
      col1_mwidth = std::max(col1_mwidth, entry.soft_lim.size());
      col2_mwidth = std::max(col2_mwidth, entry.hard_lim.size());
    }
    col0_mwidth += 2;
    col1_mwidth += 2;
    col2_mwidth += 2;
    // scrollok(getWindow(), true);
    wrefresh(getWindow());
  }

  std::string stringMaker(const std::string &first, const std::string &second,
                          const std::string &third, int col0_mwidth,
                          int col2_mwidth, int col1_mwidth, int maxX) {
    std::string toPrint =
        " " + first + std::string(col0_mwidth - first.length(), ' ') + second +
        std::string(col1_mwidth - second.length(), ' ') + third;
    toPrint += std::string(maxX - 2 - toPrint.length(), ' ');
    return toPrint;
  }

  void display() override {
    draw_rectangle(getWindow(), 0, 0, maxY, maxX);

    // Draw rectangle for the parted display
    draw_rectangle(getWindow(), 1, 1, 5, maxX - 2);

    // print headers
    init_pair(1, COLOR_BLACK, COLOR_CYAN);

    wattron(getWindow(), COLOR_PAIR(1));
    mvwprintw(getWindow(), 6, 1, "%s",
              stringMaker(headers[0], headers[1], headers[2], col0_mwidth,
                          col1_mwidth, col2_mwidth, maxX)
                  .c_str());
    wattroff(getWindow(), COLOR_PAIR(1));
    int cnt = 6;
    for (int i = 0; const auto &entry : dataToDisplay) {
      cnt++;
      i++;
      if (cnt == maxY - 1) {
        brokeOff = 0;
        break;
      }
      mvwprintw(getWindow(), cnt, 1, "%s",
                stringMaker(entry.name, entry.soft_lim, entry.hard_lim,
                            col0_mwidth, col1_mwidth, col2_mwidth, maxX)
                    .c_str());
    }

    wrefresh(getWindow());
  }

  void handleInput(int ch) override {
    switch (ch) {
    case KEY_DOWN: {
      if (brokeOff != -1) {
        int cnt = 6; // THIS 6 THIS MAGIC comes from woh box size + 1 + 1 [from
                     // the border]
        for (int i = brokeOff; i < dataToDisplay.size(); i++) {
          cnt++;
          if (cnt == maxY - 1) {
            brokeOff++;
            break;
          }
          const auto &entry = dataToDisplay[i];
          mvwprintw(getWindow(), cnt, 1, "%s",
                    stringMaker(entry.name, entry.soft_lim, entry.hard_lim,
                                col0_mwidth, col1_mwidth, col2_mwidth, maxX)
                        .c_str());
        }
        wrefresh(getWindow());
      }
      break;
    }
    case KEY_UP: {
      if (brokeOff != -1) {
        int cnt = 6;
        for (int i = brokeOff; i < dataToDisplay.size(); i++) {
          cnt++;
          if (cnt == maxY - 1) {
            brokeOff--;
            brokeOff = std::max(brokeOff - 1, 0);
            break;
          }
          const auto &entry = dataToDisplay[i];
          mvwprintw(getWindow(), cnt, 1, "%s",
                    stringMaker(entry.name, entry.soft_lim, entry.hard_lim,
                                col0_mwidth, col1_mwidth, col2_mwidth, maxX)
                        .c_str());
        }
        wrefresh(getWindow());
      }
    } break;
    }
  }
};
