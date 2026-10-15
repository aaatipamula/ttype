#include <ctype.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <ncurses.h>

#define BUFSIZE 4096
#define WORDBANK_SUBPATH "/ttype/wordbank.txt"

#define WORDS 10

#define CTRL(c) ((c) & 0x1f)
#define Err(msg) fprintf(stderr, "Error: %s\n", msg); exit(EXIT_FAILURE);

typedef struct string {
    char *value;
    size_t len;
    size_t _bufsize;
} string_t;

char *get_config_dir(void) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char *config_dir;

    if (xdg && xdg[0] != '\0') {
        config_dir = strdup(xdg);
    } else {
        const char *home = getenv("HOME");
        if (!home) {
            Err("HOME not set!")
        }

        size_t len = strlen(home) + strlen("/.config") + 1;
        config_dir = malloc(len);
        if (!config_dir) {
            Err("Failed to allocate memory!")
        }
        snprintf(config_dir, len, "%s/.config", home);
    }

    if (!config_dir) {
        Err("Failed to allocate memory!")
    }
    return config_dir;
}

// Resolves the default wordbank path: <config_dir>/ttype/wordbank.txt
char *default_wordbank_path(void) {
    char *config_dir = get_config_dir();

    size_t len = strlen(config_dir) + strlen(WORDBANK_SUBPATH) + 1;
    char *path = malloc(len);
    if (!path) {
        Err("Failed to allocate memory!")
    }
    snprintf(path, len, "%s%s", config_dir, WORDBANK_SUBPATH);
    free(config_dir);

    return path;
}

char **collect_words(const char *path, size_t *count) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        Err("Failed to open file!")
    }
    
    size_t word_cap = 64, word_count = 0;
    char **lines = malloc(word_cap * sizeof(char *));
    if (!lines) {
        Err("Failed to allocate memory!");
    }

    char *line = NULL;
    size_t line_bufsize = 0;
    ssize_t line_len;

    while((line_len = getline(&line, &line_bufsize, fp)) != -1) {
        if (line_len > 0 && line[line_len - 1] == '\n') {
            line[line_len - 1] = '\0';
        }

        if (word_count == word_cap) {
            word_cap *= 2;
            void *ptr = realloc(lines, word_cap * sizeof(char *));
            if (!ptr) {
                Err("Failed to reallocate memory")
            }
            lines = ptr;
        }

        lines[word_count] = strdup(line);
        word_count++;
    }

    free(line);
    fclose(fp);
    *count = word_count;
    return lines;
}

void clear_words(char **words, size_t word_count) {
    for (size_t i = 0; i < word_count; i++) {
        free(words[i]);
    }
    free(words);
}

void init_string(string_t *string) {
    void *ptr = malloc(BUFSIZE);
    if (!ptr) {
        Err("Failed to allocate memory!");
    }
    memset(ptr, 0, BUFSIZE);
    string->value = (char *)ptr;
    string->_bufsize = BUFSIZE;
    string->len = 0;
}

void init_string_sized(string_t *string, size_t bufsize) {
    void *ptr = malloc(bufsize);
    if (!ptr) {
        Err("Failed to allocate memory!")
    }
    memset(ptr, 0, bufsize);
    string->value = (char *)ptr;
    string->_bufsize = bufsize;
    string->len = 0;
}

void resize_string(string_t *string) {
    size_t newsize = string->_bufsize + BUFSIZE;
    void *ptr = realloc(string->value, newsize);
    if (!ptr) {
        Err("Failed to reallocate memory!");
    }
    string->value = (char *)ptr;
    string->_bufsize = newsize;
}

void gen_phrase(char **words, size_t word_count, string_t *phrase, size_t max_words) {
    init_string(phrase);

    size_t offset = 0;
    for (size_t i = 0; i < max_words; i++) {
        int idx = rand() % word_count;
        const char *word = words[idx];
        size_t wordlen = strlen(word) + 1;

        if (offset + wordlen > phrase->_bufsize) {
            resize_string(phrase);
        }

        snprintf(phrase->value + offset, phrase->_bufsize - offset, "%s ", word);
        offset += wordlen;
    }

    phrase->len = offset - 1;
    phrase->value[phrase->len] = 0;
}

#define TEXT_Y 2
#define TEXT_X 4

typedef struct {
    int row;
    int col;
} pos_t;

// Computes the on-screen (row, col) for every character in text, wrapping
// at word boundaries within `width` columns. coords[len] is also set, for
// positioning the cursor once all characters have been typed.
pos_t *layout_text(const char *text, size_t len, int width) {
    pos_t *coords = malloc((len + 1) * sizeof(pos_t));
    if (!coords) {
        Err("Failed to allocate memory!")
    }

    size_t line_start = 0;
    int row = 0;

    while (1) {
        size_t limit = line_start + (size_t)width;
        size_t line_end;

        if (limit >= len) {
            line_end = len;
        } else {
            size_t brk = limit;
            while (brk > line_start && text[brk] != ' ') brk--;
            // No space in range: word is longer than the line, hard-break it.
            // Otherwise break right after the space, which renders as blank.
            line_end = (brk == line_start) ? limit : brk + 1;
        }

        for (size_t i = line_start; i < line_end; i++) {
            coords[i].row = row;
            coords[i].col = (int)(i - line_start);
        }

        if (line_end == len) {
            coords[len].row = row;
            coords[len].col = (int)(len - line_start);
            break;
        }

        row++;
        line_start = line_end;
    }

    return coords;
}

void render(const string_t *phrase, const string_t *userin, pos_t *coords, int y, int x) {
    for (size_t i = 0; i < phrase->len; i++) {
        chtype c;
        if (i >= userin->len) {
            c = phrase->value[i] | COLOR_PAIR(1);
        } else if (userin->value[i] == phrase->value[i]) {
            c = phrase->value[i] | COLOR_PAIR(2);
        } else {
            c = phrase->value[i] | COLOR_PAIR(3);
        }
        mvaddch(y + coords[i].row, x + coords[i].col, c);
    }
}

void delete_word(string_t *string) {
    size_t idx = string->len;
    if (isspace(string->value[idx - 1])) {
        while (idx > 0 && isspace(string->value[idx - 1])) { 
            string->value[--idx] = 0;
        }
        while (idx > 0 && !isspace(string->value[idx - 1])) {
            string->value[--idx] = 0;
        }

    } else {
        while (idx > 0 && !isspace(string->value[idx - 1])) {
            string->value[--idx] = 0;
        }
    }
    string->len = idx;
}

int main(int argc, char **argv) {
    const char *import_path = NULL;
    size_t max_words = WORDS;

    int opt;
    while ((opt = getopt(argc, argv, "w:n:h")) != -1) {
        switch (opt) {
            case 'w': import_path = optarg; break;
            case 'n': max_words = (size_t)atoi(optarg); break;
            case 'h':
                printf("Usage: %s [-w wordbank_file] [-n word_count]\n", argv[0]);
                return 0;
            default:
                fprintf(stderr, "Usage: %s [-w wordbank_file] [-n word_count]\n", argv[0]);
                return 1;
        }
    }

    srand(time(NULL));

    char *wordbank_path = import_path ? strdup(import_path) : default_wordbank_path();
    if (!wordbank_path) {
        Err("Failed to allocate memory!")
    }
    size_t word_count;
    char **words = collect_words(wordbank_path, &word_count);
    free(wordbank_path);

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(1);

    if (has_colors() == TRUE) {
        use_default_colors();
        start_color();
        init_pair(1, 8, -1);
        init_pair(2, COLOR_GREEN, -1);
        init_pair(3, COLOR_RED, -1);
    }

    string_t phrase;
    gen_phrase(words, word_count, &phrase, max_words);

    int width = getmaxx(stdscr) - TEXT_X - 1;
    if (width < 1) width = 1;
    pos_t *coords = layout_text(phrase.value, phrase.len, width);

    // User input buffer
    string_t userin;
    init_string_sized(&userin, phrase._bufsize);

    // Write ghost text
    mvprintw(TEXT_Y, 2, ">");
    render(&phrase, &userin, coords, TEXT_Y, TEXT_X);
    move(TEXT_Y + coords[userin.len].row, TEXT_X + coords[userin.len].col);
    refresh();

    // User input loop
    size_t mistakes = 0;
    struct timespec start, end;
    bool started = FALSE;
    while (userin.len <= phrase.len - 1) {
        int ch = getch();

        if (started == FALSE) {
            clock_gettime(CLOCK_MONOTONIC, &start);
            started = TRUE;
        }

        if (ch == CTRL('w')) {
            delete_word(&userin);
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (userin.len == 0) continue;
            userin.len--;
            userin.value[userin.len] = phrase.value[userin.len];
        } else {
            if (phrase.value[userin.len] != ch) mistakes++;
            userin.value[userin.len++] = ch;
        }

        render(&phrase, &userin, coords, TEXT_Y, TEXT_X);
        move(TEXT_Y + coords[userin.len].row, TEXT_X + coords[userin.len].col);
        refresh();
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec)
           + (end.tv_nsec - start.tv_nsec) / 1e9;   /* seconds */
    double cps = userin.len / elapsed;

    int maxy = getmaxy(stdscr);
    mvprintw(maxy - 2, 2, "Time: %4.2f sec  CPS: %2.2f  Mistakes: %lu", elapsed, cps, mistakes);

    while(1) {
        int ch = getch();
        if (ch == '\r' || ch == '\n' || ch == KEY_ENTER) break;
    }

    // Cleanup
    free(phrase.value);
    free(userin.value);
    free(coords);
    clear_words(words, word_count);
    endwin();
    return 0;
}
