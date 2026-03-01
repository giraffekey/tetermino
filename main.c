/*
 * Tetrominos in your terminal!
 *
 * Author: giraffekey
 * Date: 2026-02-28
 * License: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

#define max(a,b) \
({ __typeof__ (a) _a = (a); \
__typeof__ (b) _b = (b); \
_a > _b ? _a : _b; })

#define min(a,b) \
({ __typeof__ (a) _a = (a); \
__typeof__ (b) _b = (b); \
_a < _b ? _a : _b; })

#ifndef _WIN32
#include <unistd.h>
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

#ifndef _WIN32
#include <sys/ioctl.h>

void get_winsize(int* cols, int* rows) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    *cols = (int)w.ws_col;
    *rows = (int)w.ws_row;
}
#endif

#ifndef _WIN32
void get_time(struct timespec* tp) {
    clock_gettime(CLOCK_MONOTONIC_RAW, tp);
}
#endif

#define WIDTH 10
#define HEIGHT 20
#define TETROMINO_COUNT 7
#define TETROMINO_SIZE 4

constexpr int GAME_WIDTH = 47;
constexpr int GAME_HEIGHT = 22;

typedef enum {
    running,
    paused,
    terminated,
} game_state_t;

typedef struct {
    unsigned int type, color;
    int x, y;
    unsigned int shape[TETROMINO_SIZE][TETROMINO_SIZE];
} tetromino_t;

typedef struct {
    pthread_mutex_t mutex;
    game_state_t game_state;
    unsigned int board[HEIGHT][WIDTH];
    tetromino_t tetromino;
    bool redraw;
    unsigned int level;
    unsigned int score;
    unsigned int lines;
    unsigned int speed_index;
    bool fast;
    bool clears[HEIGHT];
} game_data_t;

constexpr unsigned int TETROMINOS[TETROMINO_COUNT][TETROMINO_SIZE][TETROMINO_SIZE] = {
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

constexpr unsigned int SCORE_PER_LINES[4] = {40, 100, 300, 1200};

constexpr int SPEEDS[15][2] = {
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

void init(game_data_t* data) {
    data->game_state = running;
    memset(data->board, 0, sizeof(data->board));
    data->redraw = false;
    data->level = 0;
    data->score = 0;
    data->lines = 0;
    data->speed_index = 0;
    data->fast = false;
    memset(data->clears, false, sizeof(data->clears));
}

void display_board(const game_data_t* data) {
    int cols, rows;
    get_winsize(&cols, &rows);
    const int margin_left = (cols - GAME_WIDTH) / 2;
    const int margin_top = (rows - GAME_HEIGHT) / 2;

    const int max1 = max(data->level, 1);
    const int max2 = max(data->score, data->lines);
    const int digits = (int)log10(max(max1, max2));

    clear();
    for (int i = 0; i < margin_top; i++) { printf("\n"); }
    for (int i = 0; i < margin_left; i++) { printf(" "); }
    printf("-----------------------\n");
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < margin_left; j++) { printf(" "); }
        printf("| ");
        for (int j = 0; j < WIDTH; j++) {
            if (data->clears[i] && data->board[i][j] > 0) {
                printf("\033[37m%c\033[0m", '@');
            } else if (data->board[i][j] == 0) {
                printf(".");
            } else if (data->board[i][j] <= 7) {
                printf("\033[3%dm%c\033[0m", data->board[i][j], '@');
            } else {
                printf("\033[38;5;%dm%c\033[0m", data->board[i][j], '@');
            }
            printf(" ");
        }
        printf("|");
        if (i == 8) {
            printf("          ");
            for (int j = 0; j < 14 + digits; j++) { printf("-"); }
        }
        if (i == 9) {
            const int local_digits = (int)log10(max(data->level, 1));
            printf("          |  Level: %d", data->level);
            for (int j = 0; j < 2 + digits - local_digits; j++) { printf(" "); }
            printf("|");
        }
        if (i == 10) {
            const int local_digits = (int)log10(max(data->score, 1));
            printf("          |  Score: %d", data->score);
            for (int j = 0; j < 2 + digits - local_digits; j++) { printf(" "); }
            printf("|");
        }
        if (i == 11) {
            const int local_digits = (int)log10(max(data->lines, 1));
            printf("          |  Lines: %d", data->lines);
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

void spawn_tetromino(game_data_t* data) {
    data->tetromino.type = rand() % TETROMINO_COUNT;
    switch (data->tetromino.type) {
        case 0:
            data->tetromino.color = 6;
            break;
        case 1:
            data->tetromino.color = 3;
            break;
        case 2:
            data->tetromino.color = 5;
            break;
        case 3:
            data->tetromino.color = 4;
            break;
        case 4:
            data->tetromino.color = 214;
            break;
        case 5:
            data->tetromino.color = 2;
            break;
        case 6:
            data->tetromino.color = 1;
            break;
        default:
            break;
    }

    memcpy(data->tetromino.shape, TETROMINOS[data->tetromino.type], sizeof(data->tetromino.shape));

    const int orientation = rand() % 4;
    for (int k = 0; k < orientation; k++) {
        for (int i = 0; i < TETROMINO_SIZE / 2; i++) {
            for (int j = i; j < TETROMINO_SIZE - 1 - i; j++) {
                const unsigned int temp = data->tetromino.shape[i][j];
                data->tetromino.shape[i][j] = data->tetromino.shape[j][TETROMINO_SIZE - 1 - i];
                data->tetromino.shape[j][TETROMINO_SIZE - 1 - i] = data->tetromino.shape[TETROMINO_SIZE - 1 - i][TETROMINO_SIZE - 1 - j];
                data->tetromino.shape[TETROMINO_SIZE - 1 - i][TETROMINO_SIZE - 1 - j] = data->tetromino.shape[TETROMINO_SIZE - 1 - j][i];
                data->tetromino.shape[TETROMINO_SIZE - 1 - j][i] = temp;
            }
        }
    }

    int start_x = TETROMINO_SIZE, start_y = TETROMINO_SIZE;
    int end_x = 0, end_y = 0;
    for (int i = 0; i < TETROMINO_SIZE; i++) {
        for (int j = 0; j < TETROMINO_SIZE; j++) {
            if (data->tetromino.shape[i][j] > 0) {
                if (j < start_x) { start_x = j; }
                if (i < start_y) { start_y = i; }
                if (j > end_x) { end_x = j; }
                if (i > end_y) { end_y = i; }
            }
        }
    }

    data->tetromino.x = rand() % (WIDTH - (end_x - start_x)) - start_x;
    data->tetromino.y = -start_y - (end_y - start_y) - 1;
}

void clear_tetromino(game_data_t* data) {
    for (int i = 0; i < TETROMINO_SIZE; i++) {
        const int y = data->tetromino.y + i;
        if (y < 0 || y >= HEIGHT) continue;
        for (int j = 0; j < TETROMINO_SIZE; j++) {
            const int x = data->tetromino.x + j;
            if (data->tetromino.shape[i][j] > 0) {
                data->board[y][x] = 0;
            }
        }
    }
}

void insert_tetromino(game_data_t* data) {
    for (int i = 0; i < TETROMINO_SIZE; i++) {
        const int y = data->tetromino.y + i;
        if (y < 0 || y >= HEIGHT) continue;
        for (int j = 0; j < TETROMINO_SIZE; j++) {
            const int x = data->tetromino.x + j;
            if (x < 0 || x >= WIDTH) continue;
            if (data->tetromino.shape[i][j] > 0) {
                data->board[y][x] = data->tetromino.color;
            }
        }
    }
}

bool check_loss(const game_data_t* data) {
    for (int i = 0; i < TETROMINO_SIZE; i++) {
        const int y = data->tetromino.y + i;
        for (int j = 0; j < TETROMINO_SIZE; j++) {
            if (data->tetromino.shape[i][j] > 0 && y < 0) {
                bool blocked = false;
                for (int k = 0; k < TETROMINO_SIZE; k++) {
                    const int x = data->tetromino.x + k;
                    if (data->board[0][x] > 0) {
                        blocked = true;
                    }
                }
                if (blocked) return true;
            }
        }
    }
    return false;
}

bool check_lines(game_data_t* data) {
    bool any_cleared = false;
    for (int i = 0; i < HEIGHT; i++) {
        bool cleared = true;
        for (int j = 0; j < WIDTH; j++) {
            if (data->board[i][j] == 0) cleared = false;
        }
        if (cleared) {
            data->clears[i] = true;
            any_cleared = true;
        }
    }
    return any_cleared;
}

void clear_lines(game_data_t* data) {
    int cleared = 0;
    int i = HEIGHT - 1;
    while (i >= 0) {
        if (data->clears[i]) {
            data->clears[i] = false;
            for (int j = i; j > 0; j--) {
                for (int k = 0; k < WIDTH; k++) {
                    data->board[j][k] = data->board[j - 1][k];
                    data->clears[j] = data->clears[j - 1];
                }
            }
            cleared += 1;
        }
        if (!data->clears[i]) {
            i -= 1;
        }
    }

    if (cleared > 0) {
        data->score += SCORE_PER_LINES[cleared - 1] * (data->level + 1);
        data->lines += cleared;
        data->level = data->lines / 10;

        for (unsigned int j = data->speed_index + 1; j < sizeof(SPEEDS) / sizeof(SPEEDS[0]); j++) {
            if (data->level >= SPEEDS[j][0]) {
                data->speed_index = min(j, 3);
            }
        }

        spawn_tetromino(data);
        data->redraw = true;
    }
}

void place_tetromino(game_data_t* data) {
    if (check_loss(data)) {
        init(data);
        spawn_tetromino(data);
    } else {
        insert_tetromino(data);
        if (!check_lines(data)) {
            spawn_tetromino(data);
        }
    };

    data->redraw = true;
}

void drop_tetromino(game_data_t* data) {
    clear_tetromino(data);

    for (int i = 0; i < TETROMINO_SIZE; i++) {
        const int y = data->tetromino.y + i + 1;
        for (int j = 0; j < TETROMINO_SIZE; j++) {
            const int x = data->tetromino.x + j;
            if (data->tetromino.shape[i][j] > 0 && (y >= HEIGHT || data->board[y][x] > 0)) {
                place_tetromino(data);
                return;
            }
        }
    }

    data->tetromino.y += 1;
    insert_tetromino(data);
    data->redraw = true;
}

void instant_drop(game_data_t* data) {
    clear_tetromino(data);

    while (true) {
        bool placed = false;
        for (int i = 0; i < TETROMINO_SIZE; i++) {
            const int y = data->tetromino.y + i + 1;
            for (int j = 0; j < TETROMINO_SIZE; j++) {
                const int x = data->tetromino.x + j;
                if (data->tetromino.shape[i][j] > 0 && (y >= HEIGHT || data->board[y][x] > 0)) {
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
        data->tetromino.y += 1;
    }

    insert_tetromino(data);
    data->fast = true;
    data->redraw = true;
}

void move_tetromino_right(game_data_t* data) {
    clear_tetromino(data);

    for (int i = 0; i < TETROMINO_SIZE; i++) {
        const int y = data->tetromino.y + i;
        for (int j = 0; j < TETROMINO_SIZE; j++) {
            const int x = data->tetromino.x + j + 1;
            if (data->tetromino.shape[i][j] > 0 && (x >= WIDTH || data->board[y][x] > 0)) {
                insert_tetromino(data);
                return;
            }
        }
    }

    data->tetromino.x += 1;
    insert_tetromino(data);
    data->redraw = true;
}

void move_tetromino_left(game_data_t* data) {
    clear_tetromino(data);

    for (int i = 0; i < TETROMINO_SIZE; i++) {
        const int y = data->tetromino.y + i;
        for (int j = 0; j < TETROMINO_SIZE; j++) {
            const int x = data->tetromino.x + j - 1;
            if (data->tetromino.shape[i][j] > 0 && (x < 0 || data->board[y][x] > 0)) {
                insert_tetromino(data);
                return;
            }
        }
    }

    data->tetromino.x -= 1;
    insert_tetromino(data);
    data->redraw = true;
}

void move_tetromino(game_data_t* data, const bool dir) {
    if (dir) {
        move_tetromino_right(data);
    } else {
        move_tetromino_left(data);
    }
}

void rotate_tetromino(game_data_t* data) {
    clear_tetromino(data);

    unsigned int shape[TETROMINO_SIZE][TETROMINO_SIZE] = {0};
    for (int i = 0; i < TETROMINO_SIZE / 2; i++) {
        for (int j = i; j < TETROMINO_SIZE - 1 - i; j++) {
            shape[i][j] = data->tetromino.shape[j][TETROMINO_SIZE - 1 - i];
            shape[j][TETROMINO_SIZE - 1 - i] = data->tetromino.shape[TETROMINO_SIZE - 1 - i][TETROMINO_SIZE - 1 - j];
            shape[TETROMINO_SIZE - 1 - i][TETROMINO_SIZE - 1 - j] = data->tetromino.shape[TETROMINO_SIZE - 1 - j][i];
            shape[TETROMINO_SIZE - 1 - j][i] = data->tetromino.shape[i][j];
        }
    }

    for (int i = 0; i < TETROMINO_SIZE; i++) {
        const int y = data->tetromino.y + i;
        for (int j = 0; j < TETROMINO_SIZE; j++) {
            const int x = data->tetromino.x + j;
            if (shape[i][j] > 0 && (data->board[y][x] > 0 || x < 0 || x >= WIDTH || y >= HEIGHT)) {
                insert_tetromino(data);
                data->redraw = true;
                return;
            }
        }
    }

    memcpy(data->tetromino.shape, shape, sizeof(data->tetromino.shape));
    insert_tetromino(data);
    data->redraw = true;
}

void* keypress_loop(void* arg) {
    game_data_t* data = arg;
    while (true) {
        int ch = getch();

        pthread_mutex_lock(&data->mutex);

        switch (ch) {
            case 'q':
                data->game_state = terminated;
                break;
            case 'w':
                // Up Key
                rotate_tetromino(data);
                break;
            case 's':
                // Down Key
                data->fast = true;
                break;
            case 'd':
                // Right Key
                move_tetromino(data, true);
                break;
            case 'a':
                // Left Key
                move_tetromino(data, false);
                break;
            case ' ':
                // Space Key
                instant_drop(data);
                break;
            default:
                break;
        }

        if (ch == 27 && getch() == 91) {
            ch = getch();
            switch (ch) {
                case 'A':
                    // Up Key
                    rotate_tetromino(data);
                    break;
                case 'B':
                    // Down Key
                    data->fast = true;
                    break;
                case 'C':
                    // Right Key
                    move_tetromino(data, true);
                    break;
                case 'D':
                    // Left Key
                    move_tetromino(data, false);
                    break;
                default:
                    break;
            }
        }

        const bool is_running = data->game_state == running;
        pthread_mutex_unlock(&data->mutex);
        if (!is_running) break;

        usleep(10000);
    }
    return nullptr;
}

int main(void) {
    srand(time(nullptr));

    game_data_t data;
    const pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    data.mutex = mutex;

    init(&data);
    spawn_tetromino(&data);
    display_board(&data);

    pthread_t keypress_thread;
    pthread_create(&keypress_thread, nullptr, keypress_loop, &data);

    unsigned long last_update = 0;
    struct timespec start, end;
    get_time(&start);

    while (true) {
        get_time(&end);
        const unsigned long ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;

        pthread_mutex_lock(&data.mutex);

        int speed = data.fast ? 1 : SPEEDS[data.speed_index][1];
        for (int i = 0; i < HEIGHT; i++) {
            if (data.clears[i]) {
                speed = 15;
                break;
            }
        }

        if (ms - last_update >= speed * 1000 / 60) {
            clear_lines(&data);
            drop_tetromino(&data);
            last_update = ms;
        }

        data.fast = false;

        if (data.redraw) {
            display_board(&data);
            data.redraw = false;
        }

        const bool is_running = data.game_state == running;
        pthread_mutex_unlock(&data.mutex);
        if (!is_running) break;

        usleep(10000);
    }

    pthread_join(keypress_thread, nullptr);
    pthread_mutex_destroy(&data.mutex);

    return 0;
}