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
                                       "/uvm_ctrl.soft");
        if (std::filesystem::exists(softPath) &&
            std::filesystem::exists(hardPath)) {
          data current = {.name = entry.path()};
          std::ifstream softStream(softPath);
          std::ifstream hardStream(hardPath);
          softStream >> current.soft_lim;
          hardStream >> current.hard_lim;
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
    draw_rectangle(getWindow(), 0, 0, nlines, ncols);
    getbegyx(getWindow(), begY, begX);
    wrefresh(getWindow());
  }
  void display() override {
    int maxY, maxX;
    getmaxyx(getWindow(), maxY, maxX);
    populateData();
    
    std::vector<std::string> headers({"CGroup Name", "Soft Limit", "Hard Limit"});
    size_t col0_mwidth = headers[0].size();
    size_t col1_mwidth = headers[1].size();
    size_t col2_mwidth = headers[2].size();

    for(const auto& entry  : dataToDisplay) {
      col0_mwidth = std::max(col0_mwidth, entry.name.size());
      col1_mwidth = std::max(col1_mwidth, entry.soft_lim.size());
      col2_mwidth = std::max(col2_mwidth, entry.hard_lim.size());
    }

    wrefresh(getWindow());
  }

  void handleInput(int ch) override {}
};
