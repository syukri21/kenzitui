#include <ncurses.h>
#include <stdlib.h>

int main() {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();

    printw("Welcome to Kenzitui! (Press any key to exit)");

    refresh();
    getch();
    endwin();

    return EXIT_SUCCESS;
}
