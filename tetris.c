#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

#define WIDTH 10
#define HEIGHT 20

int board[HEIGHT][WIDTH] = {0};

// ---- Block shapes (4x4 each) ----
int blocks[3][4][4] = {
    {   // Square
        {1,1,0,0},
        {1,1,0,0},
        {0,0,0,0},
        {0,0,0,0}
    },
    {   // Line
        {1,1,1,1},
        {0,0,0,0},
        {0,0,0,0},
        {0,0,0,0}
    },
    {   // L-shape
        {1,0,0,0},
        {1,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
    }
};

// ---- Draw the board ----
void draw_board() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (board[y][x])
                mvprintw(y, x * 2, "[]");
            else
                mvprintw(y, x * 2, " .");
        }
    }
}

// ---- Draw current block ----
void draw_block(int y, int x, int block[4][4]) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (block[i][j])
                mvprintw(y + i, (x + j) * 2, "[]");
        }
    }
}

// ---- Collision detection ----
bool check_collision(int y, int x, int block[4][4]) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (block[i][j]) {
                if (y + i >= HEIGHT) return true;           // bottom
                if (x + j < 0 || x + j >= WIDTH) return true; // sides
                if (board[y + i][x + j]) return true;        // other blocks
            }
        }
    }
    return false;
}

// ---- Merge block into board ----
void merge_block(int y, int x, int block[4][4]) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (block[i][j])
                board[y + i][x + j] = 1;
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

    srand(time(NULL));

    int posX = WIDTH / 2 - 2;
    int posY = 0;
    int frame = 0;
    int current = rand() % 3;
    bool running = true;

    // Wait for key to start
    mvprintw(HEIGHT / 2, (WIDTH - 10), "Tetris in C\nPress any arrow key to start (q to quit)");
    refresh();
    while (1) {
        int start_ch = getch();
        if (start_ch == 'q') { endwin(); return 0; }
        if (start_ch == KEY_LEFT || start_ch == KEY_RIGHT || start_ch == KEY_DOWN || start_ch == KEY_UP)
            break;
        usleep(50000);
    }

    while (running) {
        int ch = getch();
        if (ch == 'q') break;

        // Movement controls
        if (ch == KEY_LEFT && !check_collision(posY, posX - 1, blocks[current])) posX--;
        if (ch == KEY_RIGHT && !check_collision(posY, posX + 1, blocks[current])) posX++;
        if (ch == KEY_DOWN) posY++;

        // Gravity
        frame++;
        if (frame % 10 == 0) posY++;

        // Check collision with bottom or blocks
        if (check_collision(posY + 1, posX, blocks[current])) {
            merge_block(posY, posX, blocks[current]);
            posY = 0;
            posX = WIDTH / 2 - 2;
            current = rand() % 3;

            // Game over check
            if (check_collision(posY, posX, blocks[current])) {
                mvprintw(HEIGHT / 2, (WIDTH - 5), "GAME OVER!");
                refresh();
                sleep(2);
                running = false;
                continue;
            }
        }

        clear();
        draw_board();
        draw_block(posY, posX, blocks[current]);
        refresh();

        usleep(50000); // ~20 FPS
    }

    endwin();
    return 0;
}
