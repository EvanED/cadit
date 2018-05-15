#include <string>
#include <vector>
#include <queue>
#include <cassert>
#include <unistd.h>
#include <termios.h>
#include <boost/optional.hpp>
#include <sys/ioctl.h>

using boost::optional;
using boost::none;
using std::string;
using std::vector;

////////////////////////////

int g_cursor_line, g_cursor_column;
int g_max_line;
bool g_overwrite;
vector<string> g_document = {""};    

termios g_orig_termios;

void restore_termios_mode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios); // TODO: ||die
}

void raw()
{
    tcgetattr(STDIN_FILENO, &g_orig_termios); // TODO: ||die
    atexit(restore_termios_mode);

    termios raw = g_orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    //raw.c_cc[VMIN] = 0;
    //raw.c_cc[VTIME] = 1;
   
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // TODO: ||die
}

////////////////////////////

enum Keys {
    ARROW_UP = 1000,
    ARROW_LEFT,
    ARROW_DOWN,
    ARROW_RIGHT,
    INSERT,
};

struct Key
{
    int k = 0;
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
        while (read(STDIN_FILENO, &k.k, 1) != 1)
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
    }
    assert(k.k == '2');
    
    k = read_byte();
    switch(k.k) {
        case '~': return {INSERT};
    }
    assert(false);
    return k;
}

////////////////////////////////////////////

void put(char c)
{
    write(STDOUT_FILENO, &c, 1);
}

template<size_t size>
void put(char const (&str)[size])
{
    if (write(STDOUT_FILENO, str, size - 1) != size - 1)
        perror("write:");
}

void move_cursor(Key k)
{
    switch (k.k) {
        case ARROW_UP:
            if (g_cursor_line >= 1) {
                put("\033[1A");
                g_cursor_line--;
                if (g_cursor_column > (int)g_document[g_cursor_line].size()) {
                    printf("\r\033[%dC", (int)g_document[g_cursor_line].size());
                    fflush(stdout);
                    g_cursor_column = g_document[g_cursor_line].size();
                }
            }
            break;
        case ARROW_DOWN:
            if (g_cursor_line < g_max_line) {
                put("\033[1B");
                g_cursor_line++;
                if (g_cursor_column > (int)g_document[g_cursor_line].size()) {
                    printf("\r\033[%dC", (int)g_document[g_cursor_line].size());
                    fflush(stdout);
                    g_cursor_column = g_document[g_cursor_line].size();
                }
            }
            break;
        case ARROW_RIGHT:
            if (g_cursor_column < (int)g_document[g_cursor_line].size()) {
                put("\033[1C");
                g_cursor_column++;
            }
            break;
        case ARROW_LEFT:
            if (g_cursor_column > 0) {
                put("\033[1D");
                g_cursor_column--;
            }
            break;
    };
}

//////////////////////////////////////////

//////////////////////////////////////////

winsize get_window_size() {
    winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
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
    printf("\033[%d;%dH", size.ws_row, 0);
    printf("\033[0K");
    printf("\033[7m");
    printf("%d:%d", g_cursor_line, g_cursor_column);
    if (g_overwrite)
        printf(" [overwrite]");
    printf("\033[m");
    printf("\033[%d;%dH", row, col);
    fflush(stdout);
}

//////////////////////////////////////////

int main()
{
    raw();
   
    bool should_exit = false;

    put("\n\033[A");
   
    while (!should_exit) {
        write_status_bar();
        Key k = read();
        switch (k.k) {
            case 'q':
                should_exit = true;
                break;

            case '\r':
            case '\n':
                put("\r\n");
                g_cursor_line++;
                g_cursor_column = 0;
                if (g_cursor_line > g_max_line) {
                    g_document.emplace_back();
                    g_max_line = g_cursor_line;
                    put("\033[0K");
                    put("\r\n");
                    put("\033[1A");
                }
                break;

            case ARROW_UP:
            case ARROW_DOWN:
            case ARROW_LEFT:
            case ARROW_RIGHT:
                move_cursor(k);
                break;

            case INSERT:
                g_overwrite = !g_overwrite;
                break;

            default: {
                if (!iscntrl(k.k)) {
                    string & s = g_document[g_cursor_line];
                    if (g_cursor_column >= (int)s.size()) {
                        s.push_back(k.k);
                    }
                    else if (g_overwrite) {
                        s[g_cursor_column] = k.k;
                    }
                    else {
                        s.insert(g_cursor_column, 1, k.k);
                    }
                    g_cursor_column++;

                    printf("\r%s", s.c_str());
                    printf("\r\033[%dC", g_cursor_column);
                    fflush(stdout);
                }
            }
        }
    }
    return 0;
}
