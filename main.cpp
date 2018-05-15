#include <string>
#include <queue>
#include <cassert>
#include <unistd.h>
#include <termios.h>
#include <boost/optional.hpp>
#include <sys/ioctl.h>

using boost::optional;
using boost::none;

////////////////////////////

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
};

struct Key
{
    int k = 0;
};

std::queue<int> g_pending_bytes;

Key read_byte()
{
    Key k;
    if (!g_pending_bytes.empty()) {
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
        case ARROW_UP:    put("\033[1A"); break;
        case ARROW_DOWN:  put("\033[1B"); break;
        case ARROW_RIGHT: put("\033[1C"); break;
        case ARROW_LEFT:  put("\033[1D"); break;
    };
}

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

    std::string pos_str;

    Key k = read_byte();
    assert(k.k == '\033');
    k = read_byte();
    assert(k.k == '[');
    for (k = read_byte(); k.k != ';'; k = read_byte()) {
        pos_str.push_back(k.k);
        assert(pos_str.size() < 10);
    }
    int row = std::stoi(pos_str);
    pos_str.clear();
    for (k = read_byte(); k.k != 'R'; k = read_byte()) {
        pos_str.push_back(k.k);
        assert(pos_str.size() < 10);
    }
    int col = std::stoi(pos_str);

    winsize size = get_window_size();
    printf("\033[%d;%dH", size.ws_row, 0);
    printf("\033[7m");
    printf("%d:%d", row, col);
    printf("\033[m");
    printf("\033[%d;%dH", row, col);
    fflush(stdout);
}

//////////////////////////////////////////

int main()
{
    raw();
   
    bool should_exit = false;
    std::string document;

    put("\n\033[A");
   
    while (!should_exit) {
        //write_status_bar();
        Key k = read();
        switch (k.k) {
            case 'q':
                should_exit = true;
                break;

            case ARROW_UP:
            case ARROW_DOWN:
            case ARROW_LEFT:
            case ARROW_RIGHT:
                move_cursor(k);
                break;

            default: {
                if (!iscntrl(k.k)) {
                    put(k.k);
                    document.push_back(k.k);
                }

                if (k.k == '\r' || k.k == '\n') {
                    put("\r\n");
                    document += "\r\n";
                }
            }
        }
    }

    //printf("\033[%dA\r", n_lines);
    //printf("%s", document.c_str());
    return 0;
}
