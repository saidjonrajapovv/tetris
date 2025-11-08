/* main.c - Terminal Tetris in C using ncurses
 *
 * Features:
 *  - 7 tetrominoes (I,O,T,S,Z,J,L) using 4x4 matrices
 *  - Rotation with fallback wall-kick attempts (no full SRS)
 *  - Collision detection, locking, spawning
 *  - Line clearing with scoring (100,300,500,800)
 *  - Score, lines, level, speed scaling
 *  - Next-piece preview
 *
 * Compile:
 *   gcc main.c -o tetris -lncurses
 *
 * Run:
 *   ./tetris
 *
 * Controls:
 *   Left/Right arrows - move
 *   Down arrow - soft drop
 *   Up arrow - rotate clockwise
 *   Space - hard drop
 *   q - quit
 *
 */

#include <ncurses.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#define WIDTH 10
#define HEIGHT 20

/* Game state */
int board[HEIGHT][WIDTH];
int score = 0;
int total_lines = 0;
int level = 0;

/* Timing control (microseconds). Will decrease as level increases. */
const useconds_t BASE_DELAY = 400000; // base fall delay (0.4s)
const useconds_t LEVEL_STEP = 30000;  // decrease per level
const useconds_t MIN_DELAY = 80000;   // minimum delay (fastest)

/* 4x4 tetromino templates [7 pieces][4 rows][4 cols] */
const int TETROMINO_COUNT = 7;
const int tetrominoes[7][4][4] = {
    /* I */
    {
        {0,0,0,0},
        {1,1,1,1},
        {0,0,0,0},
        {0,0,0,0}
    },
    /* O */
    {
        {0,1,1,0},
        {0,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
    },
    /* T */
    {
        {0,1,0,0},
        {1,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
    },
    /* S */
    {
        {0,1,1,0},
        {1,1,0,0},
        {0,0,0,0},
        {0,0,0,0}
    },
    /* Z */
    {
        {1,1,0,0},
        {0,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
    },
    /* J */
    {
        {1,0,0,0},
        {1,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
    },
    /* L */
    {
        {0,0,1,0},
        {1,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
    }
};

/* Current falling piece */
int cur_piece[4][4];
int cur_type = 0;   // index 0..6
int next_type = 0;
int cur_x = 3;      // piece top-left x in board coordinates (0..WIDTH-1)
int cur_y = 0;      // piece top-left y in board coordinates

/* Utility: copy template into cur_piece */
void spawn_piece_from_type(int type) {
    for (int i=0; i<4; ++i)
        for (int j=0; j<4; ++j)
            cur_piece[i][j] = tetrominoes[type][i][j];
}

/* Draw functions */
void draw_board_and_ui() {
    clear();
    // draw board
    for (int y=0; y<HEIGHT; ++y) {
        for (int x=0; x<WIDTH; ++x) {
            if (board[y][x]) mvprintw(y, x*2, "[]");
            else mvprintw(y, x*2, " .");
        }
    }
    // draw current piece on top
    for (int i=0; i<4; ++i) {
        for (int j=0; j<4; ++j) {
            if (cur_piece[i][j]) {
                int by = cur_y + i;
                int bx = cur_x + j;
                if (by >= 0 && by < HEIGHT && bx >= 0 && bx < WIDTH)
                    mvprintw(by, bx*2, "[]");
            }
        }
    }

    // UI: show next piece
    mvprintw(1, WIDTH*2 + 4, "Next:");
    for (int i=0;i<4;++i) {
        for (int j=0;j<4;++j) {
            if (tetrominoes[next_type][i][j]) mvprintw(3 + i, WIDTH*2 + 4 + j*2, "[]");
            else mvprintw(3 + i, WIDTH*2 + 4 + j*2, "  ");
        }
    }

    // Score / lines / level
    mvprintw(9, WIDTH*2 + 4, "Score: %d", score);
    mvprintw(11, WIDTH*2 + 4, "Lines: %d", total_lines);
    mvprintw(13, WIDTH*2 + 4, "Level: %d", level);
    mvprintw(16, WIDTH*2 + 4, "Controls:");
    mvprintw(17, WIDTH*2 + 4, "<- ->  move");
    mvprintw(18, WIDTH*2 + 4, "down  soft drop");
    mvprintw(19, WIDTH*2 + 4, "up    rotate");
    mvprintw(20, WIDTH*2 + 4, "space hard drop");
    mvprintw(22, WIDTH*2 + 4, "q     quit");

    refresh();
}

/* Collision detection: returns true if placing block at (y,x) collides (out of bounds or hitting locked block) */
bool check_collision_matrix(int test_y, int test_x, int mat[4][4]) {
    for (int i=0;i<4;++i) {
        for (int j=0;j<4;++j) {
            if (!mat[i][j]) continue;
            int by = test_y + i;
            int bx = test_x + j;
            if (by < 0) continue; // allow top overflow (piece spawns slightly above)
            if (bx < 0 || bx >= WIDTH || by >= HEIGHT) return true;
            if (board[by][bx]) return true;
        }
    }
    return false;
}

/* Merge current piece into board (lock it) */
void lock_piece_to_board() {
    for (int i=0;i<4;++i) {
        for (int j=0;j<4;++j) {
            if (!cur_piece[i][j]) continue;
            int by = cur_y + i;
            int bx = cur_x + j;
            if (by >= 0 && by < HEIGHT && bx >= 0 && bx < WIDTH) {
                board[by][bx] = 1;
            }
        }
    }
}

/* Clear full lines. Return number of lines cleared in this call. */
int clear_full_lines_and_score() {
    int cleared = 0;
    for (int y = HEIGHT-1; y >= 0; --y) {
        bool full = true;
        for (int x=0;x<WIDTH;++x) {
            if (!board[y][x]) { full = false; break; }
        }
        if (full) {
            // shift rows above down
            for (int ty = y; ty > 0; --ty) {
                for (int tx = 0; tx < WIDTH; ++tx) {
                    board[ty][tx] = board[ty-1][tx];
                }
            }
            // clear top row
            for (int tx=0; tx<WIDTH; ++tx) board[0][tx] = 0;
            cleared++;
            ++y; // recheck this row index after shift
        }
    }

    if (cleared > 0) {
        // scoring table (classic-ish)
        int add = 0;
        if (cleared == 1) add = 100;
        else if (cleared == 2) add = 300;
        else if (cleared == 3) add = 500;
        else if (cleared >= 4) add = 800;
        score += add;
        total_lines += cleared;
        // update level every 10 lines
        level = total_lines / 10;
    }
    return cleared;
}

/* Rotate matrix clockwise into out[4][4] */
void rotate_cw(int in[4][4], int out[4][4]) {
    for (int i=0;i<4;++i)
        for (int j=0;j<4;++j)
            out[j][3-i] = in[i][j];
}

/* Try rotate with simple wall-kick: attempt no shift, left, right */
bool try_rotate_with_kick() {
    int rotated[4][4];
    rotate_cw(cur_piece, rotated);

    // attempts: 0 (no shift), -1 (left), +1 (right), -2, +2 (extra)
    const int kicks[] = {0, -1, 1, -2, 2};
    for (int k = 0; k < (int)(sizeof(kicks)/sizeof(kicks[0])); ++k) {
        int tx = cur_x + kicks[k];
        int ty = cur_y;
        if (!check_collision_matrix(ty, tx, rotated)) {
            // apply rotated into cur_piece and adjust x
            for (int i=0;i<4;++i) for (int j=0;j<4;++j) cur_piece[i][j] = rotated[i][j];
            cur_x = tx;
            return true;
        }
    }
    return false;
}

/* Hard drop: move piece down until it collides, lock and return final y */
int hard_drop() {
    int drop_y = cur_y;
    while (!check_collision_matrix(drop_y + 1, cur_x, cur_piece)) {
        ++drop_y;
    }
    cur_y = drop_y;
    lock_piece_to_board();
    return drop_y;
}

/* Spawn next piece: set cur_piece, cur_type, position. Return false if spawn collides immediately (game over). */
bool spawn_next_piece() {
    cur_type = next_type;
    next_type = rand() % TETROMINO_COUNT;
    spawn_piece_from_type(cur_type);
    // initial spawn position: center-ish
    cur_x = (WIDTH / 2) - 2;
    cur_y = -1; // start slightly above visible area (so tall pieces can enter)
    // If immediate collision -> game over
    if (check_collision_matrix(cur_y, cur_x, cur_piece)) return false;
    return true;
}

/* Compute current fall delay based on level */
useconds_t current_delay() {
    long d = (long)BASE_DELAY - (long)level * (long)LEVEL_STEP;
    if (d < (long)MIN_DELAY) d = MIN_DELAY;
    return (useconds_t)d;
}

/* Initialize board to empty */
void board_clear() {
    memset(board, 0, sizeof(board));
}

int main() {
    /* init */
    srand((unsigned int)time(NULL));
    board_clear();

    /* ncurses init */
    initscr();
    noecho();
    cbreak();
    curs_set(FALSE);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    /* initial pieces */
    cur_type = rand() % TETROMINO_COUNT;
    next_type = rand() % TETROMINO_COUNT;
    spawn_piece_from_type(cur_type);
    cur_x = (WIDTH / 2) - 2;
    cur_y = -1;
    // set next_type for spawn function
    next_type = rand() % TETROMINO_COUNT;

    /* start screen */
    mvprintw(HEIGHT/2 - 1, 2, "TETRIS in C (ncurses)");
    mvprintw(HEIGHT/2 + 0, 2, "Arrows: move/rotate, space: hard drop, q: quit");
    mvprintw(HEIGHT/2 + 2, 2, "Press any arrow key to start");
    refresh();

    // wait for key to start (arrow or q)
    while (1) {
        int ch = getch();
        if (ch == 'q') { endwin(); return 0; }
        if (ch == KEY_LEFT || ch == KEY_RIGHT || ch == KEY_DOWN || ch == KEY_UP || ch == ' ') break;
        usleep(50000);
    }

    // set up first current and next correctly
    cur_type = rand() % TETROMINO_COUNT;
    next_type = rand() % TETROMINO_COUNT;
    spawn_piece_from_type(cur_type);
    next_type = rand() % TETROMINO_COUNT;

    bool running = true;
    useconds_t delay = current_delay();
    unsigned int tick = 0;

    while (running) {
        int ch = getch();

        if (ch == 'q') break;

        // movement controls
        if (ch == KEY_LEFT) {
            if (!check_collision_matrix(cur_y, cur_x - 1, cur_piece)) cur_x--;
        } else if (ch == KEY_RIGHT) {
            if (!check_collision_matrix(cur_y, cur_x + 1, cur_piece)) cur_x++;
        } else if (ch == KEY_DOWN) {
            if (!check_collision_matrix(cur_y + 1, cur_x, cur_piece)) cur_y++;
            else {
                // lock if can't move down
                lock_piece_to_board();
                int cleared = clear_full_lines_and_score();
                // increase speed after clearing lines (level updated in clear func)
                delay = current_delay();
                // spawn next
                cur_type = next_type;
                next_type = rand() % TETROMINO_COUNT;
                spawn_piece_from_type(cur_type);
                // if spawn collides -> game over
                if (check_collision_matrix(cur_y, cur_x, cur_piece)) {
                    // show game over
                    draw_board_and_ui();
                    mvprintw(HEIGHT/2, WIDTH, "GAME OVER! Score: %d", score);
                    refresh();
                    sleep(2);
                    break;
                }
            }
        } else if (ch == KEY_UP) {
            try_rotate_with_kick();
        } else if (ch == ' ') {
            // hard drop
            hard_drop();
            int cleared = clear_full_lines_and_score();
            delay = current_delay();
            // spawn next
            cur_type = next_type;
            next_type = rand() % TETROMINO_COUNT;
            spawn_piece_from_type(cur_type);
            if (check_collision_matrix(cur_y, cur_x, cur_piece)) {
                draw_board_and_ui();
                mvprintw(HEIGHT/2, WIDTH, "GAME OVER! Score: %d", score);
                refresh();
                sleep(2);
                break;
            }
        }

        // gravity by time
        // we measure with usleep loops; move down every 'delay' microseconds
        draw_board_and_ui();

        // wait in small slices so input remains responsive
        useconds_t step = 30000; // 30ms slices
        useconds_t waited = 0;
        while (waited < delay) {
            usleep(step);
            waited += step;
            int ch2 = getch();
            if (ch2 == 'q') { running = false; break; }
            if (ch2 == KEY_LEFT) {
                if (!check_collision_matrix(cur_y, cur_x - 1, cur_piece)) cur_x--;
                draw_board_and_ui();
            } else if (ch2 == KEY_RIGHT) {
                if (!check_collision_matrix(cur_y, cur_x + 1, cur_piece)) cur_x++;
                draw_board_and_ui();
            } else if (ch2 == KEY_UP) {
                try_rotate_with_kick();
                draw_board_and_ui();
            } else if (ch2 == KEY_DOWN) {
                if (!check_collision_matrix(cur_y + 1, cur_x, cur_piece)) cur_y++;
                draw_board_and_ui();
            } else if (ch2 == ' ') {
                hard_drop();
                int cleared = clear_full_lines_and_score();
                delay = current_delay();
                // spawn next
                cur_type = next_type;
                next_type = rand() % TETROMINO_COUNT;
                spawn_piece_from_type(cur_type);
                if (check_collision_matrix(cur_y, cur_x, cur_piece)) {
                    draw_board_and_ui();
                    mvprintw(HEIGHT/2, WIDTH, "GAME OVER! Score: %d", score);
                    refresh();
                    sleep(2);
                    running = false;
                    break;
                }
            }
        }

        if (!running) break;

        // after delay elapsed, try to move piece down
        if (!check_collision_matrix(cur_y + 1, cur_x, cur_piece)) {
            cur_y++;
        } else {
            // lock piece
            lock_piece_to_board();
            int cleared = clear_full_lines_and_score();
            delay = current_delay();
            // spawn next
            cur_type = next_type;
            next_type = rand() % TETROMINO_COUNT;
            spawn_piece_from_type(cur_type);
            if (check_collision_matrix(cur_y, cur_x, cur_piece)) {
                draw_board_and_ui();
                mvprintw(HEIGHT/2, WIDTH, "GAME OVER! Score: %d", score);
                refresh();
                sleep(2);
                break;
            }
        }
    }

    endwin();
    return 0;
}
