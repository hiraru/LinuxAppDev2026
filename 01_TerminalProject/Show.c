#include <stdio.h>
#include <curses.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define ESC 27

static volatile sig_atomic_t g_resize = 0;

typedef enum {
    ST_INIT,
    ST_VIEW,
    ST_RESIZE,
    ST_SCROLL_UP,
    ST_SCROLL_DOWN,
    ST_SCROLL_LEFT,
    ST_SCROLL_RIGHT,
    ST_PAGE_UP,
    ST_PAGE_DOWN,
    ST_EXIT
} State;

typedef enum {
    EV_UP,
    EV_DOWN,
    EV_LEFT,
    EV_RIGHT,
    EV_PGUP,
    EV_PGDN,
    EV_ESC,
    EV_NONE
} Event;

typedef struct {
    State state;
    WINDOW *win;
    const char *filename;
    char **lines;
    int total;
    int offset;
    int col_offset;
    int max_len;
} App;

static Event read_event(WINDOW *w) {
    switch (wgetch(w)) {
        case ' ':
        case KEY_DOWN:
            return EV_DOWN;
        case KEY_UP:
            return EV_UP;
        case KEY_LEFT:
            return EV_LEFT;
        case KEY_RIGHT:
            return EV_RIGHT;
        case KEY_NPAGE:
            return EV_PGDN;
        case KEY_PPAGE:
            return EV_PGUP;
        case ESC:
            return EV_ESC;
        default:
            return EV_NONE;
    }
}

static State transition(State s, Event e) {
    switch (s) {
        case ST_VIEW:
            switch (e) {
                case EV_UP:
                    return ST_SCROLL_UP;
                case EV_DOWN:
                    return ST_SCROLL_DOWN;
                case EV_LEFT:
                    return ST_SCROLL_LEFT;
                case EV_RIGHT:
                    return ST_SCROLL_RIGHT;
                case EV_PGUP:
                    return ST_PAGE_UP;
                case EV_PGDN:
                    return ST_PAGE_DOWN;
                case EV_ESC:
                    return ST_EXIT;
                default:
                    return ST_VIEW;
            }
                case ST_RESIZE:
                case ST_SCROLL_UP:
                case ST_SCROLL_DOWN:
                case ST_SCROLL_LEFT:
                case ST_SCROLL_RIGHT:
                case ST_PAGE_UP:
                case ST_PAGE_DOWN:
                    return ST_VIEW;
                default:
                    return s;
    }
}

static void on_winch(int sig) {
    (void)sig;
    g_resize = 1;
}

static int load_file(const char *name, App *app) {
    FILE *f = fopen(name, "r");
    if(f == NULL) {
        perror("Не удалось открыть файл!\n");
        return 1;
    }
    int cap = 64;
    app->total = 0;
    app->lines = malloc(cap * sizeof(char *));

    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\n")] = '\0';
        if (app->total == cap) {
            cap *= 2;
            app->lines = realloc(app->lines, cap * sizeof(char *));
        }
        app->lines[app->total] = strdup(buf);
        int len = (int)strlen(buf);
        if (len > app->max_len) app->max_len = len;
        app->total++;
    }
    fclose(f);
    return 0;
}

static void do_init(App *app) {
    initscr();
    noecho();
    cbreak();

    struct sigaction sa = {0};
    sa.sa_handler = on_winch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, NULL);

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    // оставляем по 1 строке рамки сверху и снизу + отступ
    int win_h = rows - 2;
    int win_w = cols - 2;
    if (win_h < 3) {
        win_h = 3;
    }
    if (win_w < 3) {
        win_w = 3;
    }

    app->win = newwin(win_h, win_w, 1, 1);
    if (!app->win) {
        endwin();
        fprintf(stderr, "Не удалось создать окно\n");
        exit(1);
    }
    keypad(app->win, TRUE);
    app->offset = 0;
    app->col_offset = 0;
    g_resize = 0;
}

static void render(const App *app) {
    int h, w;
    getmaxyx(app->win, h, w);
    int visible = h - 2;
    if (visible < 1) visible = 1;
    int inner = w - 2;
    if (inner < 1) inner = 1;

    werase(app->win);
    box(app->win, 0, 0);
    mvwprintw(app->win, 0, 2, "%s [%d/%d]",
              app->filename, app->offset + 1, app->total);

    for (int i = 0; i < visible; i++) {
        int idx = app->offset + i;
        if (idx >= app->total) break;

        const char *line = app->lines[idx];
        int len = (int)strlen(line);

        const char *shown = (app->col_offset < len)
        ? line + app->col_offset
        : "";

        mvwprintw(app->win, i + 1, 1, "%-*.*s", inner, inner, shown);
    }
    wrefresh(app->win);
}

static void do_resize(App *app) {
    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1 ||
        ws.ws_row == 0 || ws.ws_col == 0) {
        return;
        }

        if (resizeterm(ws.ws_row, ws.ws_col) == ERR) {
            return;
        }

        int rows, cols;
    getmaxyx(stdscr, rows, cols);

    if (rows < 5 || cols < 5) {
        return;
    }

    delwin(app->win);
    app->win = NULL;

    clearok(stdscr, TRUE);
    clear();
    refresh();

    app->win = newwin(rows - 2, cols - 2, 1, 1);
    if (!app->win) { endwin(); exit(1); }
    keypad(app->win, TRUE);

    int visible = rows - 4;
    if (visible < 1) {
        visible = 1;
    }
    if (app->offset > app->total - visible) {
        app->offset = app->total - visible;
    }
    if (app->offset < 0) {
        app->offset = 0;
    }

    int inner = cols - 4;
    if (inner < 1) inner = 1;
    if (app->col_offset + inner > app->max_len) {
        app->col_offset = app->max_len - inner;
    }
    if (app->col_offset < 0) {
        app->col_offset = 0;
    }
}

static void do_scroll_down(App *app) {
    int h, w; getmaxyx(app->win, h, w);
    int visible = h - 2;
    if (visible < 1) {
        visible = 1;
    }
    if (app->offset + visible < app->total) {
        app->offset++;
    }
}

static void do_scroll_up(App *app) {
    if (app->offset > 0) {
        app->offset--;
    }
}

static void do_scroll_right(App *app) {
    int h, w; getmaxyx(app->win, h, w);
    int inner = w - 2;
    if (inner < 1) {
        inner = 1;
    }
    if (app->col_offset + inner < app->max_len) {
        app->col_offset++;
    }
}

static void do_scroll_left(App *app) {
    if (app->col_offset > 0) app->col_offset--;
}

static void do_page_down(App *app) {
    int h, w; getmaxyx(app->win, h, w);
    int visible = h - 2;
    if (visible < 1) {
        visible = 1;
    }
    app->offset += visible;
    if (app->offset + visible > app->total) {
        app->offset = app->total - visible;
    }
    if (app->offset < 0) {
        app->offset = 0;
    }
}

static void do_page_up(App *app) {
    int h, w; getmaxyx(app->win, h, w);
    int visible = h - 2;
    if (visible < 1) {
        visible = 1;
    }
    app->offset -= visible;
    if (app->offset < 0) {
        app->offset = 0;
    }
}

static void do_exit(App *app) {
    endwin();
    if (app->win) delwin(app->win);
    for (int i = 0; i < app->total; i++) free(app->lines[i]);
    free(app->lines);
}

//очень длинная строка zzz................................................................................................................................................................................................................................................................................................................................................................

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");

    if (argc < 2) {
        fprintf(stderr, "Использование: %s имя_файла\n", argv[0]);
        return 1;
    }

    App app = { .state = ST_INIT, .filename = argv[1] };

    while (app.state != ST_EXIT) {
        switch (app.state) {
            case ST_INIT:
                if (load_file(argv[1], &app) != 0) {
                    fprintf(stderr, "Не удалось открыть %s\n", argv[1]);
                    return 1;
                }
                do_init(&app);
                app.state = ST_VIEW;
                break;

            case ST_VIEW:
                if (g_resize) {
                    g_resize = 0;
                    app.state = ST_RESIZE;
                    break;
                }
                render(&app);
                Event ev = read_event(app.win);
                if (ev == EV_NONE && g_resize) {
                    g_resize = 0;
                    app.state = ST_RESIZE;
                    break;
                }
                app.state = transition(ST_VIEW, ev);
                break;

            case ST_RESIZE:
                do_resize(&app);
                app.state = transition(ST_RESIZE, EV_NONE);
                break;

            case ST_SCROLL_DOWN:
                do_scroll_down(&app);
                app.state = transition(ST_SCROLL_DOWN, EV_NONE);
                break;

            case ST_SCROLL_UP:
                do_scroll_up(&app);
                app.state = transition(ST_SCROLL_UP, EV_NONE);
                break;

            case ST_SCROLL_LEFT:
                do_scroll_left(&app);
                app.state = transition(ST_SCROLL_LEFT, EV_NONE);
                break;

            case ST_SCROLL_RIGHT:
                do_scroll_right(&app);
                app.state = transition(ST_SCROLL_RIGHT, EV_NONE);
                break;

            case ST_PAGE_UP:
                do_page_up(&app);
                app.state = transition(ST_PAGE_UP, EV_NONE);
                break;

            case ST_PAGE_DOWN:
                do_page_down(&app);
                app.state = transition(ST_PAGE_DOWN, EV_NONE);
                break;

            case ST_EXIT:
                break;
        }
    }

    do_exit(&app);
    return 0;
}
