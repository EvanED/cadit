// Copyright 2018 Evan Driscoll
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>
#include <vector>
#include <queue>
#include <cassert>
#include <unistd.h>
#include <termios.h>
#include <boost/optional.hpp>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tokenize.hpp"
#include "document.hpp"

using boost::optional;
using boost::none;
using std::string;
using std::vector;

////////////////////////////

Document g_document;

int g_tty_fd;
FILE* g_tty_file;

termios g_orig_termios;

void restore_termios_mode()
{
    static bool restored = false;
    if (!restored)
        tcsetattr(g_tty_fd, TCSAFLUSH, &g_orig_termios); // TODO: ||die
    restored = true;
}

void raw()
{
    tcgetattr(g_tty_fd, &g_orig_termios); // TODO: ||die
    atexit(restore_termios_mode);

    termios raw = g_orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    //raw.c_cc[VMIN] = 0;
    //raw.c_cc[VTIME] = 1;
   
    tcsetattr(g_tty_fd, TCSAFLUSH, &raw); // TODO: ||die
}

////////////////////////////

enum Keys {
    BACKSPACE = 0x7F,
    ARROW_UP = 1000,
    ARROW_LEFT,
    ARROW_DOWN,
    ARROW_RIGHT,
    INSERT,
    HOME,
    END,
    DELETE,
};


std::queue<int> g_pending_bytes;

enum class AllowPending {
    Yes,
    No,
};

Key read_byte(AllowPending ap = AllowPending::Yes)
{
    Key k;
    if (!g_pending_bytes.empty() && ap == AllowPending::Yes) {
        k.k = g_pending_bytes.front();
        g_pending_bytes.pop();
    }
    else {
        while (read(g_tty_fd, &k.k, 1) != 1)
            ;
    }
    return k;
}

Key read()
{
    Key k = read_byte();
#if 0
    switch(k.k) {
        case ',': return {ARROW_UP};
        case 'o': return {ARROW_DOWN};
        case 'e': return {ARROW_RIGHT};
        case 'a': return {ARROW_LEFT};
    }
#endif
    if (k.k != '\x1B')
        return k;

    k = read_byte();
    if (k.k != '[') {
        assert(false);
        return k;
    }

    k = read_byte();
    switch(k.k) {
        case 'A': return {ARROW_UP};
        case 'B': return {ARROW_DOWN};
        case 'C': return {ARROW_RIGHT};
        case 'D': return {ARROW_LEFT};

        case 'F': return {END};
        case 'H': return {HOME};

        case '2':
            k = read_byte();
            switch(k.k) {
                case '~': return {INSERT};
            }
            break;

        case '3':
            k = read_byte();
            switch(k.k) {
                case '~': return {DELETE};
            }
            break;
    }
    assert(false);
    return k;
}

////////////////////////////////////////////

void move_cursor_to_end_of_line()
{
    g_document.cursor_end();
}

void move_cursor(Key k)
{
    switch (k.k) {
        case ARROW_UP:
            g_document.cursor_up();
            break;
        case ARROW_DOWN:
            g_document.cursor_down();
            break;
        case ARROW_RIGHT:
            g_document.cursor_right();
            break;
        case ARROW_LEFT:
            g_document.cursor_left();
            break;
    };
}

//////////////////////////////////////////

//////////////////////////////////////////

winsize get_window_size() {
    winsize ws;
    if (ioctl(g_tty_fd, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        assert(false);
    }
    return ws;
}

void write_status_bar()
{
    put("\033[6n");

    std::string row_str, col_str;

    Key k;
 expect_esc:
    row_str.clear();
    col_str.clear();
    for (k = read_byte(AllowPending::No); k.k != '\033'; k = read_byte(AllowPending::No))
        g_pending_bytes.push(k.k);

    k = read_byte(AllowPending::No);
    if (k.k != '[') {
        g_pending_bytes.push('\033');
        g_pending_bytes.push(k.k);
        goto expect_esc;
    }

    for (k = read_byte(AllowPending::No); std::isdigit(k.k); k = read_byte(AllowPending::No)) {
        assert(row_str.size() < 10);
        row_str.push_back(k.k);
    }

    if (k.k != ';') {
        g_pending_bytes.push('\033');
        g_pending_bytes.push('[');
        for (char c : row_str)
            g_pending_bytes.push(c);
        g_pending_bytes.push(k.k);
        goto expect_esc;
    }
    
    for (k = read_byte(AllowPending::No); std::isdigit(k.k); k = read_byte(AllowPending::No)) {
        assert(col_str.size() < 10);
        col_str.push_back(k.k);
    }
    
    if (k.k != 'R') {
        g_pending_bytes.push('\033');
        g_pending_bytes.push('[');
        for (char c : row_str)
            g_pending_bytes.push(c);
        g_pending_bytes.push(';');
        for (char c : col_str)
            g_pending_bytes.push(c);
        g_pending_bytes.push(k.k);
        goto expect_esc;
    }

    //////////////////

    int row = std::stoi(row_str);
    int col = std::stoi(col_str);

    winsize size = get_window_size();

    assert(row == size.ws_row - int(g_document.contents.size() - g_document.cursor_line));
    assert(col == g_document.cursor_column + 1);
    
    fprintf(g_tty_file, "\033[%d;%dH", size.ws_row, 0);
    fprintf(g_tty_file, "\033[0K");
    fprintf(g_tty_file, "\033[7m");
    fprintf(g_tty_file, "%d:%d", g_document.cursor_line, g_document.cursor_column);
    if (g_document.overwrite)
        fprintf(g_tty_file, " [overwrite]");
    fprintf(g_tty_file, "  -- row==%d, col==%d,  size==%dx%d, test=%dx%d",
            row, col, size.ws_row, size.ws_col,
            size.ws_row - int(g_document.contents.size() - g_document.cursor_line),        
            g_document.cursor_column + 1
            );
    fprintf(g_tty_file, "\033[m");
    fprintf(g_tty_file, "\033[%d;%dH", row, col);
    fflush(g_tty_file);
}

//////////////////////////////////////////

void print_document()
{
    g_document.render();
}

void render_line()
{
    g_document.render_current_line();
}

constexpr int ctrl(char c)
{
    return c & 0x1F;
}

int main()
{
    g_tty_fd = open("/dev/tty", O_RDWR | O_NOCTTY);
    if (g_tty_fd == -1) {
        perror("open");
        exit(1);
    }
    g_tty_file = fdopen(g_tty_fd, "w+");
    if (g_tty_file == NULL) {
        perror("fdopen");
        exit(1);
    }
    
    raw();
   
    bool should_exit = false;

    put("\n\033[A");
   
    while (!should_exit) {
        write_status_bar();
        Key k = read();
        switch (k.k) {
            case ctrl('D'):
            case ctrl('C'):
                should_exit = true;
                break;

            case '\r':
            case '\n':
                g_document.insert_newline();
                break;

            case HOME:
            case ctrl('A'):
                put("\r");
                break;

            case END:
            case ctrl('E'):
                g_document.cursor_end();
                break;

            case ARROW_UP:
            case ARROW_DOWN:
            case ARROW_LEFT:
            case ARROW_RIGHT:
                move_cursor(k);
                break;

            case INSERT:
                g_document.overwrite = !g_document.overwrite;
                break;

            case BACKSPACE:
                g_document.insert_backspace();
                break;

            case DELETE:
                g_document.insert_delete();
                break;

            default: {
                if (!iscntrl(k.k)) {
                    g_document.insert_key(k);
                }
            }
        }
    }

    restore_termios_mode();
    print_document();
    return 0;
}
