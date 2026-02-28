/*
 * Tetrominos in your terminal!
 *
 * Author: giraffekey
 * Date: 2026-02-28
 * License: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <sys/ioctl.h>

#define max(a,b) \
({ __typeof__ (a) _a = (a); \
__typeof__ (b) _b = (b); \
_a > _b ? _a : _b; })

#ifndef _WIN32
#include <termios.h>

int getch(void) {
    struct termios old_t, new_t;
    tcgetattr(STDIN_FILENO, &old_t);
    new_t = old_t;
    new_t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_t);
    const int ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
    return ch;
}
#endif

void clear() {
#ifdef _WIN32
    system("cls");
#else
    if (system("clear")) {
        return;
    }
    printf("\033[2J\033[H");
    fflush(stdout);
#endif
}

#define WIDTH 10
#define HEIGHT 20
#define TETROMINO_COUNT 7
#define TETROMINO_SIZE 4

typedef enum {
    running,
    paused,
    terminated,
} game_state_t;

typedef struct {
    unsigned int type, color;
    int x, y;
    bool newly_spawned;
    unsigned int shape[TETROMINO_SIZE][TETROMINO_SIZE];
} tetromino_t;

const unsigned int TETROMINOS[TETROMINO_COUNT][TETROMINO_SIZE][TETROMINO_SIZE] = {
    {
        {0, 0, 0, 0},
        {1, 1, 1, 1},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    },
    {
        {0, 0, 0, 0},
        {0, 1, 1, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0}
    },
    {
        {0, 0, 1, 0},
        {0, 1, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 0}
    },
{
        {0, 1, 0, 0},
        {0, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0}
    },
{
        {0, 0, 1, 0},
        {0, 0, 1, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0}
    },
{
        {0, 0, 1, 0},
        {0, 1, 1, 0},
        {0, 1, 0, 0},
        {0, 0, 0, 0}
    },
{
        {0, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 0}
    }
};

const unsigned int SCORE_PER_LINES[4] = {40, 100, 300, 1200};

const int SPEEDS[15][2] = {
    {0, 48},
    {1, 43},
    {2, 38},
    {3, 33},
    {4, 28},
    {5, 23},
    {6, 18},
    {7, 13},
    {8, 8},
    {9, 6},
    {10, 5},
    {13, 4},
    {16, 3},
    {19, 2},
    {29, 1},
};

game_state_t game_state;
unsigned int board[HEIGHT][WIDTH];
tetromino_t tetromino;
bool redraw = false;
unsigned int score;
unsigned int lines;
unsigned int level;
bool speed_index;
bool fast = false;
bool clears[HEIGHT];

void init() {
    memset(board, 0, sizeof(board));
    level = 0;
    score = 0;
    lines = 0;
    speed_index = 0;
    memset(clears, false, sizeof(clears));
}

void display_board() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    const int margin_left = (w.ws_col - 47) / 2;
    const int margin_top = (w.ws_row - 22) / 2;

    const int max1 = max(level, 1);
    const int max2 = max(score, lines);
    const int digits = (int)log10(max(max1, max2));

    clear();
    for (int i = 0; i < margin_top; i++) { printf("\n"); }
    for (int i = 0; i < margin_left; i++) { printf(" "); }
    printf("-----------------------\n");
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < margin_left; j++) { printf(" "); }
        printf("| ");
        for (int j = 0; j < WIDTH; j++) {
            if (clears[i] && board[i][j] > 0) {
                printf("\033[37m%c\033[0m", '@');
            } else if (board[i][j] == 0) {
                printf(".");
            } else if (board[i][j] <= 7) {
                printf("\033[3%dm%c\033[0m", board[i][j], '@');
            } else {
                printf("\033[38;5;%dm%c\033[0m", board[i][j], '@');
            }
            printf(" ");
        }
        printf("|");
        if (i == 8) {
            printf("          ");
            for (int j = 0; j < 14 + digits; j++) { printf("-"); }
        }
        if (i == 9) {
            const int local_digits = (int)log10(max(level, 1));
            printf("          |  Level: %d", level);
            for (int j = 0; j < 2 + digits - local_digits; j++) { printf(" "); }
            printf("|");
        }
        if (i == 10) {
            const int local_digits = (int)log10(max(score, 1));
            printf("          |  Score: %d", score);
            for (int j = 0; j < 2 + digits - local_digits; j++) { printf(" "); }
            printf("|");
        }
        if (i == 11) {
            const int local_digits = (int)log10(max(lines, 1));
            printf("          |  Lines: %d", lines);
            for (int j = 0; j < 2 + digits - local_digits; j++) { printf(" "); }
            printf("|");
        }
        if (i == 12) {
            printf("          ");
            for (int j = 0; j < 14 + digits; j++) { printf("-"); }
        }
        printf("\n");
    }
    for (int i = 0; i < margin_left; i++) { printf(" "); }
    printf("-----------------------\n");
}

void add_tetromino() {
    tetromino.type = rand() % TETROMINO_COUNT;
    switch (tetromino.type) {
        case 0:
            tetromino.color = 6;
            break;
        case 1:
            tetromino.color = 3;
            break;
        case 2:
            tetromino.color = 5;
            break;
        case 3:
            tetromino.color = 4;
            break;
        case 4:
            tetromino.color = 214;
            break;
        case 5:
            tetromino.color = 2;
            break;
        case 6:
            tetromino.color = 1;
            break;
        default:
            break;
    }

    tetromino.newly_spawned = true;
    memcpy(tetromino.shape, TETROMINOS[tetromino.type], sizeof(TETROMINOS[tetromino.type]));

    const int orientation = rand() % 4;
    for (int k = 0; k < orientation; k++) {
        for (int i = 0; i < TETROMINO_SIZE / 2; i++) {
            for (int j = i; j < TETROMINO_SIZE - 1 - i; j++) {
                const unsigned int temp = tetromino.shape[i][j];
                tetromino.shape[i][j] = tetromino.shape[j][TETROMINO_SIZE - 1 - i];
                tetromino.shape[j][TETROMINO_SIZE - 1 - i] = tetromino.shape[TETROMINO_SIZE - 1 - i][TETROMINO_SIZE - 1 - j];
                tetromino.shape[TETROMINO_SIZE - 1 - i][TETROMINO_SIZE - 1 - j] = tetromino.shape[TETROMINO_SIZE - 1 - j][i];
                tetromino.shape[TETROMINO_SIZE - 1 - j][i] = temp;
            }
        }
    }

    int start_x = TETROMINO_SIZE, start_y = TETROMINO_SIZE;
    int end_x = 0, end_y = 0;
    for (int i = 0; i < TETROMINO_SIZE; i++) {
        for (int j = 0; j < TETROMINO_SIZE; j++) {
            if (tetromino.shape[i][j] > 0) {
                if (j < start_x) { start_x = j; }
                if (i < start_y) { start_y = i; }
                if (j > end_x) { end_x = j; }
                if (i > end_y) { end_y = i; }
            }
        }
    }

    tetromino.x = rand() % (WIDTH - (end_x - start_x)) - start_x;
    tetromino.y = -start_y - (end_y - start_y) - 1;
}

void clear_tetromino() {
    for (int i = 0; i < TETROMINO_SIZE; i++) {
        const int y = tetromino.y + i;
        if (y < 0 || y >= HEIGHT) continue;
        for (int j = 0; j < TETROMINO_SIZE; j++) {
            const int x = tetromino.x + j;
            if (tetromino.shape[i][j] > 0) {
                board[y][x] = 0;
            }
        }
    }
}

void draw_tetromino() {
    for (int i = 0; i < TETROMINO_SIZE; i++) {
        const int y = tetromino.y + i;
        if (y < 0 || y >= HEIGHT) continue;
        for (int j = 0; j < TETROMINO_SIZE; j++) {
            const int x = tetromino.x + j;
            if (x < 0 || x >= WIDTH) continue;
            if (tetromino.shape[i][j] > 0) {
                board[y][x] = tetromino.color;
            }
        }
    }
}

bool check_loss() {
    for (int i = 0; i < TETROMINO_SIZE; i++) {
        const int y = tetromino.y + i;
        for (int j = 0; j < TETROMINO_SIZE; j++) {
            if (tetromino.shape[i][j] > 0 && y < 0) {
                bool blocked = false;
                for (int k = 0; k < TETROMINO_SIZE; k++) {
                    const int x = tetromino.x + k;
                    if (board[0][x] > 0) {
                        blocked = true;
                    }
                }
                if (blocked) return true;
            }
        }
    }
    return false;
}

bool check_lines() {
    bool any_cleared = false;
    for (int i = 0; i < HEIGHT; i++) {
        bool cleared = true;
        for (int j = 0; j < WIDTH; j++) {
            if (board[i][j] == 0) cleared = false;
        }
        if (cleared) {
            clears[i] = true;
            any_cleared = true;
        }
    }
    return any_cleared;
}

void clear_lines() {
    int cleared = 0;
    int i = HEIGHT - 1;
    while (i >= 0) {
        if (clears[i]) {
            clears[i] = false;
            for (int j = i; j > 0; j--) {
                for (int k = 0; k < WIDTH; k++) {
                    board[j][k] = board[j - 1][k];
                    clears[j] = clears[j - 1];
                }
            }
            if (clears[i]) {
                i += 1;
            }
            cleared += 1;
        }
        i -= 1;
    }
    if (cleared > 0) {
        score += SCORE_PER_LINES[cleared - 1] * (level + 1);
        lines += cleared;
        level = lines / 10;

        for (int i = speed_index + 1; i < sizeof(SPEEDS) / sizeof(SPEEDS[0]); i++) {
            if (level >= SPEEDS[i][0]) {
                speed_index = i;
            }
        }
        add_tetromino();
        redraw = true;
    }
}

void place_tetromino() {
    if (check_loss()) {
        init();
        add_tetromino();
    } else {
        draw_tetromino();
        if (!check_lines()) {
            add_tetromino();
        }
    };
    redraw = true;
}

void drop_tetromino() {
    clear_tetromino();

    for (int i = 0; i < TETROMINO_SIZE; i++) {
        const int y = tetromino.y + i + 1;
        for (int j = 0; j < TETROMINO_SIZE; j++) {
            const int x = tetromino.x + j;
            if (tetromino.shape[i][j] > 0 && (y >= HEIGHT || board[y][x] > 0)) {
                place_tetromino();
                return;
            }
        }
    }

    tetromino.y += 1;
    draw_tetromino();
    redraw = true;
}

void instant_drop() {
    clear_tetromino();

    while (true) {
        bool placed = false;
        for (int i = 0; i < TETROMINO_SIZE; i++) {
            const int y = tetromino.y + i + 1;
            for (int j = 0; j < TETROMINO_SIZE; j++) {
                const int x = tetromino.x + j;
                if (tetromino.shape[i][j] > 0 && (y >= HEIGHT || board[y][x] > 0)) {
                    placed = true;
                    break;
                }
            }
            if (placed) {
                break;
            }
        }
        if (placed) {
            break;
        }
        tetromino.y += 1;
    }

    place_tetromino();
}

void move_tetromino(const bool dir) {
    if (dir) {
        clear_tetromino();

        for (int i = 0; i < TETROMINO_SIZE; i++) {
            const int y = tetromino.y + i;
            for (int j = 0; j < TETROMINO_SIZE; j++) {
                const int x = tetromino.x + j + 1;
                if (tetromino.shape[i][j] > 0 && (x >= WIDTH || board[y][x] > 0)) {
                    draw_tetromino();
                    display_board();
                    return;
                }
            }
        }

        tetromino.x += 1;
        draw_tetromino();
        redraw = true;
    } else {
        clear_tetromino();

        for (int i = 0; i < TETROMINO_SIZE; i++) {
            const int y = tetromino.y + i;
            for (int j = 0; j < TETROMINO_SIZE; j++) {
                const int x = tetromino.x + j - 1;
                if (tetromino.shape[i][j] > 0 && (x < 0 || board[y][x] > 0)) {
                    draw_tetromino();
                    display_board();
                    return;
                }
            }
        }

        tetromino.x -= 1;
        draw_tetromino();
        redraw = true;
    }
}

void rotate_tetromino() {
    clear_tetromino();

    unsigned int shape[TETROMINO_SIZE][TETROMINO_SIZE];
    for (int i = 0; i < TETROMINO_SIZE / 2; i++) {
        for (int j = i; j < TETROMINO_SIZE - 1 - i; j++) {
            shape[i][j] = tetromino.shape[j][TETROMINO_SIZE - 1 - i];
            shape[j][TETROMINO_SIZE - 1 - i] = tetromino.shape[TETROMINO_SIZE - 1 - i][TETROMINO_SIZE - 1 - j];
            shape[TETROMINO_SIZE - 1 - i][TETROMINO_SIZE - 1 - j] = tetromino.shape[TETROMINO_SIZE - 1 - j][i];
            shape[TETROMINO_SIZE - 1 - j][i] = tetromino.shape[i][j];
        }
    }

    for (int i = 0; i < TETROMINO_SIZE; i++) {
        const int y = tetromino.y + i;
        for (int j = 0; j < TETROMINO_SIZE; j++) {
            const int x = tetromino.x + j;
            if (shape[i][j] > 0 && (board[y][x] > 0 || x < 0 || x >= WIDTH || y >= HEIGHT)) {
                draw_tetromino();
                redraw = true;
                return;
            }
        }
    }

    memcpy(tetromino.shape, shape, sizeof(shape));
    draw_tetromino();
    redraw = true;
}

void* keypress_loop(void* arg) {
    while (game_state == running) {
        int ch = getch();

        switch (ch) {
            case 'q':
                game_state = terminated;
                break;
            case 'w':
                // Up Key
                rotate_tetromino();
                break;
            case 's':
                // Down Key
                fast = true;
                break;
            case 'd':
                // Right Key
                move_tetromino(true);
                break;
            case 'a':
                // Left Key
                move_tetromino(false);
                break;
            case ' ':
                // Space Key
                instant_drop();
                break;
            default:
                break;
        }

        if (ch == 27 && getch() == 91) {
            ch = getch();
            switch (ch) {
                case 'A':
                    // Up Key
                    rotate_tetromino();
                    break;
                case 'B':
                    // Down Key
                    fast = true;
                    break;
                case 'C':
                    // Right Key
                    move_tetromino(true);
                    break;
                case 'D':
                    // Left Key
                    move_tetromino(false);
                    break;
                default:
                    break;
            }
        }
        usleep(10000);
    }
    return NULL;
}

int main(void) {
    srand(time(NULL));

    init();
    add_tetromino();
    display_board();

    pthread_t keypress_thread;
    pthread_create(&keypress_thread, NULL, keypress_loop, NULL);

    unsigned long last_update = 0;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);

    while (game_state == running) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        unsigned long ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;

        int speed = fast ? 1 : SPEEDS[speed_index][1];
        for (int i = 0; i < HEIGHT; i++) {
            if (clears[i]) {
                speed = 15;
                break;
            }
        }

        if (ms - last_update >= speed * 1000 / 60) {
            clear_lines();
            drop_tetromino();
            last_update = ms;
        }
        fast = false;

        if (redraw) {
            display_board();
            redraw = false;
        }

        usleep(10000);
    }

    return 0;
}