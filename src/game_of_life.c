#include <ncurses.h>
#include <stdio.h>

#define WIDTH 80
#define HEIGHT 25
#define LIVE_CELL 'o'
#define DEAD_CELL ' '
#define VERT_WALL '-'
#define SIDE_WALL '|'
#define ANGLE '+'
#define START_DELAY 200
#define DELAY_STEP 25
#define MIN_DELAY 25
#define MAX_DELAY 1000
#define LIVE_COLOR_PAIR 1
#define DEAD_COLOR_PAIR 2
#define BORDER_COLOR_PAIR 3

typedef int Field[HEIGHT][WIDTH];

typedef struct {
    SCREEN *screen;
    FILE *input;
} Terminal;

void clear_field(Field field);
int read_field(Field field);
int store_cell(Field field, const int *row, int *column, int symbol);
int finish_input_row(int *row, int *column);
int is_live_symbol(int symbol);
int is_dead_symbol(int symbol);
int is_ignored_symbol(int symbol);

int wrap_coordinate(int coordinate, int limit);
int count_neighbors(const Field field, int row, int column);
int next_cell_state(int current_state, int neighbors);
void calculate_generation(const Field current, Field next);
void copy_field(const Field source, Field destination);

int open_terminal(Terminal *terminal);
void close_terminal(Terminal *terminal);
int configure_terminal(void);
void initialize_colors(void);
void draw_field(const Field field, int colors_enabled);
int handle_input(int *delay);
void change_speed(int key, int *delay);
void run_game(Field field);

int main(void) {
    Field field;
    int result = 0;

    clear_field(field);
    if (!read_field(field)) {
        printf("n/a");
        result = 1;
    } else {
        run_game(field);
    }

    return result;
}

void clear_field(Field field) {
    for (int row = 0; row < HEIGHT; row++) {
        for (int column = 0; column < WIDTH; column++) {
            field[row][column] = 0;
        }
    }
}

int read_field(Field field) {
    int row = 0;
    int column = 0;
    int total_columns = 0;
    int symbol = getchar();
    int valid = 1;
    int has_cells = 0;

    while (symbol != EOF && valid) {
        if (symbol == '\n') {
            if (column > 0) {
                (row)++;
                column = 0;
                valid = row <= HEIGHT;
            }
        } else if (!is_ignored_symbol(symbol)) {
            valid = store_cell(field, &row, &column, symbol);
            total_columns += 1;
            has_cells = 1;
        }
        symbol = getchar();
    }
    return valid && has_cells && row == HEIGHT && (total_columns == HEIGHT * WIDTH);
}

int store_cell(Field field, const int *row, int *column, int symbol) {
    int valid = *row < HEIGHT && *column < WIDTH;

    if (valid && is_live_symbol(symbol)) {
        field[*row][*column] = 1;
        (*column)++;
    } else if (valid && is_dead_symbol(symbol)) {
        field[*row][*column] = 0;
        (*column)++;
    } else {
        valid = 0;
    }

    return valid;
}

int is_live_symbol(int symbol) { return symbol == '1' || symbol == '*' || symbol == '#'; }

int is_dead_symbol(int symbol) { return symbol == '0' || symbol == '.'; }

int is_ignored_symbol(int symbol) { return symbol == ' ' || symbol == '\t' || symbol == '\r'; }

int wrap_coordinate(int coordinate, int limit) {
    if (coordinate < 0) {
        coordinate = limit - 1;
    } else if (coordinate >= limit) {
        coordinate = 0;
    }

    return coordinate;
}

int count_neighbors(const Field field, int row, int column) {
    int neighbors = 0;

    for (int row_shift = -1; row_shift <= 1; row_shift++) {
        for (int column_shift = -1; column_shift <= 1; column_shift++) {
            if (row_shift != 0 || column_shift != 0) {
                int neighbor_row = wrap_coordinate(row + row_shift, HEIGHT);
                int neighbor_column = wrap_coordinate(column + column_shift, WIDTH);
                neighbors += field[neighbor_row][neighbor_column];
            }
        }
    }

    return neighbors;
}

int next_cell_state(int current_state, int neighbors) {
    int next_state = 0;

    if (neighbors == 3 || (current_state && neighbors == 2)) {
        next_state = 1;
    }

    return next_state;
}

void calculate_generation(const Field current, Field next) {
    for (int row = 0; row < HEIGHT; row++) {
        for (int column = 0; column < WIDTH; column++) {
            int neighbors = count_neighbors(current, row, column);
            next[row][column] = next_cell_state(current[row][column], neighbors);
        }
    }
}

void copy_field(const Field source, Field destination) {
    for (int row = 0; row < HEIGHT; row++) {
        for (int column = 0; column < WIDTH; column++) {
            destination[row][column] = source[row][column];
        }
    }
}

int open_terminal(Terminal *terminal) {
    int success = 0;

    terminal->input = fopen("/dev/tty", "r");
    terminal->screen = NULL;
    if (terminal->input != NULL) {
        terminal->screen = newterm(NULL, stdout, terminal->input);
        if (terminal->screen != NULL) {
            set_term(terminal->screen);
            success = 1;
        }
    }

    return success;
}

void close_terminal(Terminal *terminal) {
    endwin();
    if (terminal->screen != NULL) {
        delscreen(terminal->screen);
    }
    if (terminal->input != NULL) {
        fclose(terminal->input);
    }
}

int configure_terminal(void) {
    int colors_enabled = has_colors();

    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    if (colors_enabled) {
        initialize_colors();
    }

    return colors_enabled;
}

void initialize_colors(void) {
    start_color();
    use_default_colors();
    init_pair(LIVE_COLOR_PAIR, COLOR_GREEN, -1);
    init_pair(DEAD_COLOR_PAIR, COLOR_BLACK, -1);
    init_pair(BORDER_COLOR_PAIR, COLOR_RED, -1);
}

void draw_field(const Field field, int colors_enabled) {
    erase();
    for (int row = 0; row < HEIGHT; row++) {
        for (int column = 0; column < WIDTH; column++) {
            char symbol = field[row][column] ? LIVE_CELL : DEAD_CELL;
            int attributes = 0;

            if (colors_enabled && field[row][column]) {
                attributes = COLOR_PAIR(LIVE_COLOR_PAIR) | A_BOLD;
            } else if (colors_enabled) {
                attributes = COLOR_PAIR(DEAD_COLOR_PAIR);
            }
            if ((row == 0 || row == HEIGHT - 1) && (column == 0 || column == WIDTH - 1)) {
                attributes = COLOR_PAIR(BORDER_COLOR_PAIR);
                symbol = ANGLE;
                mvaddch(row, column, symbol | attributes);
            } else if (row == 0 || row == HEIGHT - 1) {
                symbol = VERT_WALL;
                attributes = COLOR_PAIR(BORDER_COLOR_PAIR);
                mvaddch(row, column, symbol | attributes);
            } else if (column == 0 || column == WIDTH - 1) {
                attributes = COLOR_PAIR(BORDER_COLOR_PAIR);
                symbol = SIDE_WALL;
                mvaddch(row, column, symbol | attributes);
            } else {
                mvaddch(row, column, symbol | attributes);
            }
        }
    }
    attron(COLOR_PAIR(LIVE_COLOR_PAIR));
    mvaddstr(HEIGHT, 1, "Control: a - speed up, z - slow down. Space - quit game");
    attroff(COLOR_PAIR(LIVE_COLOR_PAIR));
    refresh();
}

int handle_input(int *delay) {
    int should_exit = 0;
    int key = getch();

    while (key != ERR && !should_exit) {
        if (key == ' ') {
            should_exit = 1;
        } else {
            change_speed(key, delay);
        }
        key = getch();
    }

    return should_exit;
}

void change_speed(int key, int *delay) {
    if ((key == 'a' || key == 'A') && *delay > MIN_DELAY) {
        *delay -= DELAY_STEP;
        if (*delay < MIN_DELAY) {
            *delay = MIN_DELAY;
        }
    } else if ((key == 'z' || key == 'Z') && *delay < MAX_DELAY) {
        *delay += DELAY_STEP;
        if (*delay > MAX_DELAY) {
            *delay = MAX_DELAY;
        }
    }
}

void run_game(Field field) {
    Field next;
    Terminal terminal;
    int delay = START_DELAY;
    int should_exit = 0;
    int colors_enabled = 0;

    if (!open_terminal(&terminal)) {
        printf("n/a");
        return;
    }
    colors_enabled = configure_terminal();
    while (!should_exit) {
        draw_field(field, colors_enabled);
        should_exit = handle_input(&delay);
        if (!should_exit) {
            calculate_generation(field, next);
            copy_field(next, field);
            napms(delay);
        }
    }
    close_terminal(&terminal);
}
