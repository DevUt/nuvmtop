#pragma once
#include <ncurses.h>

/**
 * @brief Draws a rectangle in a window. This does not refresh.
 *
 * @param[in] win The window to draw the rectangle
 * @param[in] begY Y position to start.
 * @param[in] begX X position to start.
 * @param[in] nlines The number lines in the rectangle
 * @param[in] ncols The number of columns in the rectangle
 */
static void draw_rectangle(WINDOW *win, unsigned int begY, unsigned int begX,
                           unsigned int nlines, unsigned int ncols) {
    // Draw the top and bottom
    // -2 for the two corners, +1 for the same
    mvwhline(win, begY, begX+1, 0, ncols-2);
    mvwhline(win, begY+nlines-1,begX+1, 0, ncols-2);

    mvwvline(win,begY+1,begX, 0, nlines-2);
    mvwvline(win,begY+1,begX+ncols-1, 0, nlines-2);
    
    mvwaddch(win, begY, begX, ACS_ULCORNER);
    mvwaddch(win, begY, begX+ncols-1,ACS_URCORNER);

    mvwaddch(win, begY+nlines-1, begX, ACS_LLCORNER);
    mvwaddch(win, begY+nlines-1, begX+ncols-1, ACS_LRCORNER);
}
