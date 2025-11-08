#include <ncurses.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#define WIDTH 10
#define HEIGHT 20

int board[HEIGHT][WIDTH];
int score = 0;
int total_lines = 0;
int level = 0;

const useconds_t BASE_DELAY = 400000;
const useconds_t LEVEL_STEP = 30000;
const useconds_t MIN_DELAY = 80000;

const int TETROMINO_COUNT = 7;
const int tetrominoes[7][4][4] = {
    /* I */ {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
    /* O */ {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
    /* T */ {{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
    /* S */ {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
    /* Z */ {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
    /* J */ {{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
    /* L */ {{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}}
};

int cur_piece[4][4];
int cur_type = 0;
int next_type = 0;
int cur_x = 3;
int cur_y = 0;

void spawn_piece_from_type(int type) {
    for (int i=0;i<4;i++)
        for (int j=0;j<4;j++)
            cur_piece[i][j] = tetrominoes[type][i][j];
}

void draw_board_and_ui() {
    clear();
    for (int y=0;y<HEIGHT;y++) {
        for (int x=0;x<WIDTH;x++) {
            if (board[y][x]) mvprintw(y,x*2,"[]");
            else mvprintw(y,x*2," .");
        }
    }

    for (int i=0;i<4;i++) {
        for (int j=0;j<4;j++) {
            if (cur_piece[i][j]) {
                int by = cur_y + i;
                int bx = cur_x + j;
                if (by>=0 && by<HEIGHT && bx>=0 && bx<WIDTH)
                    mvprintw(by,bx*2,"[]");
            }
        }
    }

    // Next piece
    mvprintw(1, WIDTH*2 + 4, "Next:");
    for (int i=0;i<4;i++) {
        for (int j=0;j<4;j++) {
            if (tetrominoes[next_type][i][j])
                mvprintw(3+i, WIDTH*2+4+j*2,"[]");
            else mvprintw(3+i, WIDTH*2+4+j*2,"  ");
        }
    }

    mvprintw(9, WIDTH*2+4, "Score: %d", score);
    mvprintw(11, WIDTH*2+4, "Lines: %d", total_lines);
    mvprintw(13, WIDTH*2+4, "Level: %d", level);
    mvprintw(16, WIDTH*2+4, "Controls:");
    mvprintw(17, WIDTH*2+4, "<- ->  move");
    mvprintw(18, WIDTH*2+4, "down  soft drop");
    mvprintw(19, WIDTH*2+4, "up    rotate");
    mvprintw(20, WIDTH*2+4, "space hard drop");
    mvprintw(22, WIDTH*2+4, "q     quit");

    refresh();
}

bool check_collision_matrix(int test_y, int test_x, int mat[4][4]) {
    for (int i=0;i<4;i++) {
        for (int j=0;j<4;j++) {
            if (!mat[i][j]) continue;
            int by = test_y + i;
            int bx = test_x + j;
            if (bx<0 || bx>=WIDTH) return true;
            if (by>=HEIGHT) return true;
            if (by>=0 && board[by][bx]) return true;
        }
    }
    return false;
}

void lock_piece_to_board() {
    for (int i=0;i<4;i++)
        for (int j=0;j<4;j++)
            if (cur_piece[i][j] && cur_y+i>=0 && cur_y+i<HEIGHT && cur_x+j>=0 && cur_x+j<WIDTH)
                board[cur_y+i][cur_x+j] = 1;
}

int clear_full_lines_and_score() {
    int cleared = 0;
    for (int y=HEIGHT-1;y>=0;y--) {
        bool full=true;
        for (int x=0;x<WIDTH;x++)
            if (!board[y][x]) {full=false; break;}
        if (full) {
            for (int ty=y;ty>0;ty--)
                for (int tx=0;tx<WIDTH;tx++)
                    board[ty][tx] = board[ty-1][tx];
            for (int tx=0;tx<WIDTH;tx++) board[0][tx]=0;
            cleared++;
            y++;
        }
    }
    if (cleared>0) {
        int add=0;
        if (cleared==1) add=100;
        else if (cleared==2) add=300;
        else if (cleared==3) add=500;
        else if (cleared>=4) add=800;
        score+=add;
        total_lines+=cleared;
        level = total_lines/10;
    }
    return cleared;
}

void rotate_cw(int in[4][4], int out[4][4]) {
    for (int i=0;i<4;i++)
        for (int j=0;j<4;j++)
            out[j][3-i] = in[i][j];
}

bool try_rotate_with_kick() {
    int rotated[4][4];
    rotate_cw(cur_piece, rotated);
    const int kicks[] = {0,-1,1,-2,2};
    for (int k=0;k<5;k++) {
        int tx = cur_x + kicks[k];
        int ty = cur_y;
        if (!check_collision_matrix(ty, tx, rotated)) {
            for (int i=0;i<4;i++) for (int j=0;j<4;j++) cur_piece[i][j]=rotated[i][j];
            cur_x=tx;
            return true;
        }
    }
    return false;
}

int hard_drop() {
    int drop_y = cur_y;
    while(!check_collision_matrix(drop_y+1, cur_x, cur_piece)) drop_y++;
    cur_y=drop_y;
    lock_piece_to_board();
    return drop_y;
}

bool spawn_next_piece() {
    cur_type = next_type;
    next_type = rand() % TETROMINO_COUNT;
    spawn_piece_from_type(cur_type);
    cur_x = (WIDTH/2)-2;
    cur_y = -2;
    for (int i=0;i<4;i++)
        for (int j=0;j<4;j++)
            if (cur_piece[i][j] && cur_y+i>=0 && board[cur_y+i][cur_x+j]) return false;
    return true;
}

useconds_t current_delay() {
    long d = BASE_DELAY - level*LEVEL_STEP;
    if (d<MIN_DELAY) d=MIN_DELAY;
    return (useconds_t)d;
}

void board_clear() { memset(board,0,sizeof(board)); }

int main() {
    srand(time(NULL));
    board_clear();

    initscr();
    noecho();
    cbreak();
    curs_set(FALSE);
    keypad(stdscr,TRUE);
    nodelay(stdscr,TRUE);

    cur_type = rand()%TETROMINO_COUNT;
    next_type = rand()%TETROMINO_COUNT;
    spawn_piece_from_type(cur_type);
    cur_x=(WIDTH/2)-2;
    cur_y=-2;
    next_type = rand()%TETROMINO_COUNT;

    mvprintw(HEIGHT/2-1,2,"TETRIS in C (ncurses)");
    mvprintw(HEIGHT/2+0,2,"Arrows: move/rotate, space: hard drop, q: quit");
    mvprintw(HEIGHT/2+2,2,"Press any arrow key to start");
    refresh();

    while(1){
        int ch=getch();
        if (ch=='q'){ endwin(); return 0; }
        if (ch==KEY_LEFT||ch==KEY_RIGHT||ch==KEY_DOWN||ch==KEY_UP||ch==' ') break;
        usleep(50000);
    }

    cur_type = rand()%TETROMINO_COUNT;
    next_type = rand()%TETROMINO_COUNT;
    spawn_piece_from_type(cur_type);
    next_type = rand()%TETROMINO_COUNT;

    bool running=true;
    useconds_t delay = current_delay();

    while(running){
        int ch = getch();
        if (ch=='q') break;

        if (ch==KEY_LEFT && !check_collision_matrix(cur_y, cur_x-1, cur_piece)) cur_x--;
        else if (ch==KEY_RIGHT && !check_collision_matrix(cur_y, cur_x+1, cur_piece)) cur_x++;
        else if (ch==KEY_DOWN) {
            if (!check_collision_matrix(cur_y+1, cur_x, cur_piece)) cur_y++;
            else {
                lock_piece_to_board();
                clear_full_lines_and_score();
                if (!spawn_next_piece()) {
                    draw_board_and_ui();
                    mvprintw(HEIGHT/2, WIDTH, "GAME OVER! Score: %d", score);
                    refresh();
                    sleep(2);
                    break;
                }
            }
        }
        else if (ch==KEY_UP) try_rotate_with_kick();
        else if (ch==' ') {
            hard_drop();
            clear_full_lines_and_score();
            delay = current_delay();
            if (!spawn_next_piece()) {
                draw_board_and_ui();
                mvprintw(HEIGHT/2, WIDTH, "GAME OVER! Score: %d", score);
                refresh();
                sleep(2);
                break;
            }
        }

        draw_board_and_ui();
        usleep(30000);
        static int tick=0;
        tick += 30000;
        if (tick >= delay){
            tick=0;
            if(!check_collision_matrix(cur_y+1, cur_x, cur_piece)) cur_y++;
            else {
                lock_piece_to_board();
                clear_full_lines_and_score();
                if(!spawn_next_piece()){
                    draw_board_and_ui();
                    mvprintw(HEIGHT/2, WIDTH, "GAME OVER! Score: %d", score);
                    refresh();
                    sleep(2);
                    break;
                }
            }
        }
    }

    endwin();
    return 0;
}
