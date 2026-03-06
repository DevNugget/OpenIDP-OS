#include <libidp/stdio.h>
#include <libidp/syscall.h>
#include <libidp/ansi.h>
#include <stdint.h>
#include <stddef.h>

#define EDITOR_ROWS (E.editor_rows)
#define EDITOR_COLS (E.editor_cols)
#define STATUS_ROW  (E.editor_rows - 1)
#define VIEW_ROWS   (E.editor_rows - 2)

#define MAX_LINES 2048
#define MAX_LINE_LEN 256
#define MAX_FILENAME 192
#define READ_CHUNK 256

#define KEY_NULL 0
#define KEY_ARROW_UP 1001
#define KEY_ARROW_DOWN 1002
#define KEY_ARROW_RIGHT 1003
#define KEY_ARROW_LEFT 1004
#define KEY_DELETE 1005
#define KEY_HOME 1006
#define KEY_END 1007
#define KEY_PAGE_UP 1008
#define KEY_PAGE_DOWN 1009

#define CTRL_KEY(k) ((k) & 0x1f)

typedef struct {
    char text[MAX_LINE_LEN];
    int len;
} editor_line_t;

typedef struct {
    int editor_rows;
    int editor_cols;
    editor_line_t lines[MAX_LINES];
    int line_count;

    int cursor_x;
    int cursor_y;
    int row_offset;
    int col_offset;

    int dirty;
    int running;
    int quit_confirm_pending;

    char filename[MAX_FILENAME];
    int has_filename;

    char status[120];
    int status_is_error;
} editor_state_t;

static editor_state_t E;

static size_t str_len(const char* s) {
    size_t n = 0;
    while (s[n] != '\0') n++;
    return n;
}

static int copy_str(char* dst, size_t cap, const char* src) {
    size_t i = 0;
    if (cap == 0) return -1;
    while (src[i] != '\0') {
        if (i + 1 >= cap) return -1;
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return 0;
}

static void set_status(const char* msg, int is_error) {
    if (copy_str(E.status, sizeof(E.status), msg) != 0) {
        copy_str(E.status, sizeof(E.status), "status message too long");
    }
    E.status_is_error = is_error;
}

static void clear_line(editor_line_t* line) {
    line->len = 0;
    line->text[0] = '\0';
}

static int get_window_size(int* rows, int* cols) {
    printf("\x1b[18t");
    fflush();
    
    int c = getchar();
    if (c != '\x1b') return -1;
    c = getchar(); if (c != '[') return -1;
    c = getchar(); if (c != '8') return -1;
    c = getchar(); if (c != ';') return -1;
    
    int r = 0;
    while (1) {
        c = getchar();
        if (c == ';') break;
        if (c < '0' || c > '9') return -1;
        r = r * 10 + (c - '0');
    }
    
    int col = 0;
    while (1) {
        c = getchar();
        if (c == 't') break;
        if (c < '0' || c > '9') return -1;
        col = col * 10 + (c - '0');
    }
    
    *rows = r;
    *cols = col;
    return 0;
}

static void editor_init(void) {
    if (get_window_size(&E.editor_rows, &E.editor_cols) == -1) {
        E.editor_rows = 24;
        E.editor_cols = 80;
    }
    E.line_count = 1;
    for (int i = 0; i < MAX_LINES; i++) clear_line(&E.lines[i]);

    E.cursor_x = 0;
    E.cursor_y = 0;
    E.row_offset = 0;
    E.col_offset = 0;
    E.dirty = 0;
    E.running = 1;
    E.quit_confirm_pending = 0;
    E.has_filename = 0;
    E.filename[0] = '\0';
    set_status("Ctrl+S save | Ctrl+Q quit | Ctrl+F find", 0);
}

static void append_line(const char* data, int len) {
    if (E.line_count >= MAX_LINES) {
        set_status("file too large: max lines reached", 1);
        return;
    }

    editor_line_t* line = &E.lines[E.line_count - 1];
    if (line->len == 0 && E.line_count > 1) {
        line = &E.lines[E.line_count - 1];
    }

    int line_index = E.line_count - 1;
    int pos = E.lines[line_index].len;

    for (int i = 0; i < len; i++) {
        char ch = data[i];
        if (ch == '\r') continue;

        if (ch == '\n') {
            E.lines[line_index].text[pos] = '\0';
            E.lines[line_index].len = pos;

            if (E.line_count >= MAX_LINES) {
                set_status("file truncated: too many lines", 1);
                return;
            }

            line_index = E.line_count;
            E.line_count++;
            clear_line(&E.lines[line_index]);
            pos = 0;
            continue;
        }

        if (pos + 1 < MAX_LINE_LEN) {
            E.lines[line_index].text[pos++] = ch;
        }
    }

    E.lines[line_index].text[pos] = '\0';
    E.lines[line_index].len = pos;
}

static int load_file(const char* path) {
    uint64_t fd = sys_open(path, IDP_O_RDONLY);
    if (fd == (uint64_t)ERR_FAIL) {
        set_status("new file", 0);
        return 0;
    }

    for (int i = 0; i < MAX_LINES; i++) clear_line(&E.lines[i]);
    E.line_count = 1;

    char chunk[READ_CHUNK];
    uint64_t rd = 0;
    while (sys_read(fd, chunk, sizeof(chunk), &rd) == ERR_SUCCESS && rd > 0) {
        append_line(chunk, (int)rd);
    }

    sys_close(fd);

    if (E.line_count == 0) {
        E.line_count = 1;
        clear_line(&E.lines[0]);
    }

    E.dirty = 0;
    set_status("opened file", 0);
    return 0;
}

static int save_file(void) {
    if (!E.has_filename) {
        set_status("no filename", 1);
        return -1;
    }

    uint64_t fd = sys_open(E.filename, IDP_O_WRONLY | IDP_O_CREATE);
    if (fd == (uint64_t)ERR_FAIL) {
        set_status("save failed: unable to open output file", 1);
        return -1;
    }

    for (int i = 0; i < E.line_count; i++) {
        uint64_t wr = 0;
        if (E.lines[i].len > 0) {
            if (sys_write(fd, E.lines[i].text, (uint64_t)E.lines[i].len, &wr) != ERR_SUCCESS || wr != (uint64_t)E.lines[i].len) {
                sys_close(fd);
                set_status("save failed: short write", 1);
                return -1;
            }
        }

        if (i + 1 < E.line_count) {
            const char nl = '\n';
            if (sys_write(fd, &nl, 1, &wr) != ERR_SUCCESS || wr != 1) {
                sys_close(fd);
                set_status("save failed: newline write error", 1);
                return -1;
            }
        }
    }

    sys_close(fd);
    E.dirty = 0;
    set_status("saved", 0);
    return 0;
}

static void move_cursor_to(int row, int col) {
    printf("\x1b[%d;%dH", row, col);
}

static void render_rows(void) {
    for (int screen_y = 0; screen_y < VIEW_ROWS; screen_y++) {
        int file_row = E.row_offset + screen_y;
        move_cursor_to(screen_y + 1, 1);
        printf(ANSI_CLEAR_LINE_END);

        if (file_row >= E.line_count) {
            printf(ANSI_FG_MAGENTA "~" ANSI_RESET);
            continue;
        }

        editor_line_t* line = &E.lines[file_row];
        int available = line->len - E.col_offset;
        if (available < 0) available = 0;
        if (available > EDITOR_COLS) available = EDITOR_COLS;

        for (int i = 0; i < available; i++) {
            char ch = line->text[E.col_offset + i];
            if (ch == '\t') {
                putchar(' ');
                putchar(' ');
                putchar(' ');
                putchar(' ');
            } else {
                putchar(ch);
            }
        }
    }
}

static void render_status(void) {
    move_cursor_to(STATUS_ROW, 1);
    printf(ANSI_CLEAR_LINE_END ANSI_REVERSE);

    char dirty_mark = E.dirty ? '*' : '-';
    if (E.has_filename) {
        printf(ANSI_BG_YELLOW ANSI_FG_BLACK" %s %c  Ln %d, Col %d", E.filename, dirty_mark, E.cursor_y + 1, E.cursor_x + 1);
    } else {
        printf(ANSI_BG_YELLOW ANSI_FG_BLACK" [No Name] %c  Ln %d, Col %d", dirty_mark, E.cursor_y + 1, E.cursor_x + 1);
    }

    printf(ANSI_RESET);

    move_cursor_to(EDITOR_ROWS, 1);
    printf(ANSI_CLEAR_LINE_END);
    if (E.status_is_error) {
        printf(ANSI_FG_RED "%s" ANSI_RESET, E.status);
    } else {
        printf(ANSI_FG_BLUE "%s" ANSI_RESET, E.status);
    }
}

static void scroll_if_needed(void) {
    if (E.cursor_y < E.row_offset) E.row_offset = E.cursor_y;
    if (E.cursor_y >= E.row_offset + VIEW_ROWS) E.row_offset = E.cursor_y - VIEW_ROWS + 1;

    if (E.cursor_x < E.col_offset) E.col_offset = E.cursor_x;
    if (E.cursor_x >= E.col_offset + EDITOR_COLS) E.col_offset = E.cursor_x - EDITOR_COLS + 1;
}

static void editor_refresh_screen(void) {
    scroll_if_needed();
    printf(ANSI_CURSOR_HIDE);
    render_rows();
    render_status();

    int screen_x = (E.cursor_x - E.col_offset) + 1;
    int screen_y = (E.cursor_y - E.row_offset) + 1;

    if (screen_x < 1) screen_x = 1;
    if (screen_x > EDITOR_COLS) screen_x = EDITOR_COLS;
    if (screen_y < 1) screen_y = 1;
    if (screen_y > VIEW_ROWS) screen_y = VIEW_ROWS;

    move_cursor_to(screen_y, screen_x);
    printf(ANSI_CURSOR_SHOW);
}

static int read_key(void) {
    int c = getchar();
    if (c != '\x1b') return c;

    int c1 = getchar();
    if (c1 != '[') return '\x1b';

    int c2 = getchar();
    if (c2 >= '0' && c2 <= '9') {
        int c3 = getchar();
        if (c3 == '~') {
            if (c2 == '3') return KEY_DELETE;
            if (c2 == '5') return KEY_PAGE_UP;
            if (c2 == '6') return KEY_PAGE_DOWN;
            if (c2 == '1' || c2 == '7') return KEY_HOME;
            if (c2 == '4' || c2 == '8') return KEY_END;
        }
        return KEY_NULL;
    }

    if (c2 == 'A') return KEY_ARROW_UP;
    if (c2 == 'B') return KEY_ARROW_DOWN;
    if (c2 == 'C') return KEY_ARROW_RIGHT;
    if (c2 == 'D') return KEY_ARROW_LEFT;
    if (c2 == 'H') return KEY_HOME;
    if (c2 == 'F') return KEY_END;

    return KEY_NULL;
}

static void line_insert_char(editor_line_t* line, int at, char ch) {
    if (line->len + 1 >= MAX_LINE_LEN) {
        set_status("line too long", 1);
        return;
    }

    if (at < 0) at = 0;
    if (at > line->len) at = line->len;

    for (int i = line->len; i >= at; i--) {
        line->text[i + 1] = line->text[i];
    }

    line->text[at] = ch;
    line->len++;
}

static void insert_char(char ch) {
    if (E.cursor_y < 0 || E.cursor_y >= E.line_count) return;
    line_insert_char(&E.lines[E.cursor_y], E.cursor_x, ch);
    E.cursor_x++;
    E.dirty = 1;
    E.quit_confirm_pending = 0;
}

static void insert_newline(void) {
    if (E.line_count >= MAX_LINES) {
        set_status("cannot insert line: file limit reached", 1);
        return;
    }

    editor_line_t* line = &E.lines[E.cursor_y];

    for (int i = E.line_count; i > E.cursor_y + 1; i--) {
        E.lines[i] = E.lines[i - 1];
    }
    E.line_count++;
    clear_line(&E.lines[E.cursor_y + 1]);

    int tail_len = line->len - E.cursor_x;
    if (tail_len < 0) tail_len = 0;

    for (int i = 0; i < tail_len; i++) {
        E.lines[E.cursor_y + 1].text[i] = line->text[E.cursor_x + i];
    }
    E.lines[E.cursor_y + 1].len = tail_len;
    E.lines[E.cursor_y + 1].text[tail_len] = '\0';

    line->len = E.cursor_x;
    line->text[E.cursor_x] = '\0';

    E.cursor_y++;
    E.cursor_x = 0;
    E.dirty = 1;
    E.quit_confirm_pending = 0;
}

static void delete_char_at_cursor(void) {
    if (E.cursor_y < 0 || E.cursor_y >= E.line_count) return;

    editor_line_t* line = &E.lines[E.cursor_y];
    if (E.cursor_x >= line->len) {
        if (E.cursor_y + 1 >= E.line_count) return;

        editor_line_t* next = &E.lines[E.cursor_y + 1];
        if (line->len + next->len >= MAX_LINE_LEN) {
            set_status("cannot merge lines: line too long", 1);
            return;
        }

        for (int i = 0; i < next->len; i++) {
            line->text[line->len + i] = next->text[i];
        }
        line->len += next->len;
        line->text[line->len] = '\0';

        for (int i = E.cursor_y + 1; i + 1 < E.line_count; i++) {
            E.lines[i] = E.lines[i + 1];
        }
        E.line_count--;
        E.dirty = 1;
        E.quit_confirm_pending = 0;
        return;
    }

    for (int i = E.cursor_x; i < line->len; i++) {
        line->text[i] = line->text[i + 1];
    }
    line->len--;
    E.dirty = 1;
    E.quit_confirm_pending = 0;
}

static void backspace_char(void) {
    if (E.cursor_y < 0 || E.cursor_y >= E.line_count) return;

    if (E.cursor_x == 0) {
        if (E.cursor_y == 0) return;

        int prev_idx = E.cursor_y - 1;
        editor_line_t* prev = &E.lines[prev_idx];
        editor_line_t* cur = &E.lines[E.cursor_y];

        if (prev->len + cur->len >= MAX_LINE_LEN) {
            set_status("cannot merge lines: line too long", 1);
            return;
        }

        int old_prev_len = prev->len;
        for (int i = 0; i < cur->len; i++) {
            prev->text[old_prev_len + i] = cur->text[i];
        }
        prev->len = old_prev_len + cur->len;
        prev->text[prev->len] = '\0';

        for (int i = E.cursor_y; i + 1 < E.line_count; i++) {
            E.lines[i] = E.lines[i + 1];
        }
        E.line_count--;

        E.cursor_y--;
        E.cursor_x = old_prev_len;
        E.dirty = 1;
        E.quit_confirm_pending = 0;
        return;
    }

    E.cursor_x--;
    delete_char_at_cursor();
}

static void clamp_cursor_x(void) {
    if (E.cursor_y < 0) E.cursor_y = 0;
    if (E.cursor_y >= E.line_count) E.cursor_y = E.line_count - 1;

    int len = E.lines[E.cursor_y].len;
    if (E.cursor_x < 0) E.cursor_x = 0;
    if (E.cursor_x > len) E.cursor_x = len;
}

static int is_printable(int c) {
    return c >= 32 && c <= 126;
}

static int line_contains(editor_line_t* line, const char* needle) {
    int nlen = (int)str_len(needle);
    if (nlen == 0 || nlen > line->len) return -1;

    for (int i = 0; i + nlen <= line->len; i++) {
        int ok = 1;
        for (int j = 0; j < nlen; j++) {
            if (line->text[i + j] != needle[j]) {
                ok = 0;
                break;
            }
        }
        if (ok) return i;
    }
    return -1;
}

static void find_text_prompt(void) {
    char query[64];
    int qlen = 0;
    query[0] = '\0';

    set_status("Find: type query and press Enter (Esc to cancel)", 0);

    while (1) {
        editor_refresh_screen();
        int key = read_key();

        if (key == '\x1b') {
            set_status("find canceled", 0);
            return;
        }
        if (key == '\r' || key == '\n') {
            break;
        }
        if (key == 127 || key == '\b') {
            if (qlen > 0) {
                qlen--;
                query[qlen] = '\0';
            }
        } else if (is_printable(key)) {
            if (qlen + 1 < (int)sizeof(query)) {
                query[qlen++] = (char)key;
                query[qlen] = '\0';
            }
        }

        char temp[120] = "Find: ";
        int pos = 6;
        for (int i = 0; i < qlen && pos + 1 < (int)sizeof(temp); i++) temp[pos++] = query[i];
        temp[pos] = '\0';
        set_status(temp, 0);
    }

    if (qlen == 0) {
        set_status("empty query", 1);
        return;
    }

    int start_row = E.cursor_y;
    int found_row = -1;
    int found_col = -1;

    for (int pass = 0; pass < E.line_count; pass++) {
        int row = (start_row + pass) % E.line_count;
        int col = line_contains(&E.lines[row], query);
        if (col >= 0) {
            found_row = row;
            found_col = col;
            break;
        }
    }

    if (found_row >= 0) {
        E.cursor_y = found_row;
        E.cursor_x = found_col;
        set_status("match found", 0);
    } else {
        set_status("no matches", 1);
    }
}

static void move_cursor(int key) {
    editor_line_t* line = &E.lines[E.cursor_y];

    switch (key) {
        case KEY_ARROW_LEFT:
            if (E.cursor_x > 0) {
                E.cursor_x--;
            } else if (E.cursor_y > 0) {
                E.cursor_y--;
                E.cursor_x = E.lines[E.cursor_y].len;
            }
            break;

        case KEY_ARROW_RIGHT:
            if (E.cursor_x < line->len) {
                E.cursor_x++;
            } else if (E.cursor_y + 1 < E.line_count) {
                E.cursor_y++;
                E.cursor_x = 0;
            }
            break;

        case KEY_ARROW_UP:
            if (E.cursor_y > 0) E.cursor_y--;
            break;

        case KEY_ARROW_DOWN:
            if (E.cursor_y + 1 < E.line_count) E.cursor_y++;
            break;
    }

    clamp_cursor_x();
}

static void editor_process_keypress(void) {
    int key = read_key();

    if (key != CTRL_KEY('q')) {
        E.quit_confirm_pending = 0;
    }

    switch (key) {
        case CTRL_KEY('q'):
            if (E.dirty) {
                if (!E.quit_confirm_pending) {
                    set_status("unsaved changes; press Ctrl+Q again to quit", 1);
                    E.quit_confirm_pending = 1;
                    return;
                }
            }
            E.running = 0;
            return;

        case CTRL_KEY('s'):
            save_file();
            E.quit_confirm_pending = 0;
            return;

        case CTRL_KEY('f'):
            find_text_prompt();
            return;

        case CTRL_KEY('l'):
            get_window_size(&E.editor_rows, &E.editor_cols);
            return;

        case KEY_HOME:
            E.cursor_x = 0;
            return;

        case KEY_END:
            E.cursor_x = E.lines[E.cursor_y].len;
            return;

        case KEY_PAGE_UP:
            E.cursor_y -= VIEW_ROWS;
            if (E.cursor_y < 0) E.cursor_y = 0;
            clamp_cursor_x();
            return;

        case KEY_PAGE_DOWN:
            E.cursor_y += VIEW_ROWS;
            if (E.cursor_y >= E.line_count) E.cursor_y = E.line_count - 1;
            clamp_cursor_x();
            return;

        case KEY_ARROW_UP:
        case KEY_ARROW_DOWN:
        case KEY_ARROW_LEFT:
        case KEY_ARROW_RIGHT:
            move_cursor(key);
            return;

        case KEY_DELETE:
            delete_char_at_cursor();
            return;

        case 127:
        case '\b':
            backspace_char();
            return;

        case '\r':
        case '\n':
            insert_newline();
            return;

        case '\t':
            insert_char(' ');
            insert_char(' ');
            insert_char(' ');
            insert_char(' ');
            return;

        default:
            if (is_printable(key)) {
                insert_char((char)key);
            } else {
                E.quit_confirm_pending = 0;
            }
            return;
    }
}

void main(int argc, char** argv) {
    editor_init();

    if (argc > 1) {
        if (copy_str(E.filename, sizeof(E.filename), argv[1]) == 0) {
            E.has_filename = 1;
            load_file(E.filename);
        } else {
            set_status("filename too long", 1);
        }
    }

    printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME);
    while (E.running) {
        editor_refresh_screen();
        editor_process_keypress();
        clamp_cursor_x();
    }

    printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME ANSI_CURSOR_SHOW);
    sys_exit(0);
}
