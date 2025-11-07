#include <ncurses.h>
#include <unistd.h>

#define WIDTH 10
#define HEIGHT 20

int board[HEIGHT][WIDTH] = {0};

// Draw the board grid
void draw_board(int board[HEIGHT][WIDTH]) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (board[y][x])
                mvprintw(y, x * 2, "[]");
            else
                mvprintw(y, x * 2, " .");
        }
    }
}

// Draw a block at given position
void draw_block(int y, int x, int block[2][2]) {
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            if (block[i][j])
                mvprintw(y + i, (x + j) * 2, "[]");
        }
    }
}

int main() {
    initscr();
    noecho();
    cbreak();
    curs_set(FALSE);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    // Block shape (2x2 square)
    int block[2][2] = {
        {1, 1},
        {1, 1}
    };

    int posX = WIDTH / 2 - 1;
    int posY = 0;
    int frame = 0;

    while (1) {
        int ch = getch();
        if (ch == 'q') break;
        if (ch == KEY_LEFT && posX > 0) posX--;
        if (ch == KEY_RIGHT && posX < WIDTH - 2) posX++;
        if (ch == KEY_DOWN) posY++;

        // gravity
        frame++;
        if (frame % 10 == 0) posY++;

        // stop at bottom
        if (posY >= HEIGHT - 2)
            posY = HEIGHT - 2;

        clear();
        mvprintw(0, 0, "Tetris in C — press 'q' to quit");
        draw_board(board);
        draw_block(posY, posX, block);
        refresh();

        usleep(50000); // control speed (20 FPS)
    }

    endwin();
    return 0;
}
