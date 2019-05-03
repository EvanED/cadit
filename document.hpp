#pragma once

#include <vector>
#include <string>

#include "output-utilities.hpp"

struct Document
{
    int cursor_line, cursor_column;
    int max_line;
    bool overwrite;
    std::vector<std::string> contents = {""};

    void constrain_cursor()
    {
        if (cursor_column > current_line_size())
            cursor_end();
    }

    int current_line_size() const
    {
        // TODO: asserting_cast
        return (int)contents[cursor_line].size();
    }

    //////

    void cursor_end()
    {
        fprintf(g_tty_file, "\r\033[%dC", current_line_size());
        fflush(g_tty_file);
        cursor_column = current_line_size();
    }

    void cursor_up()
    {
        if (cursor_line >= 1) {
            put("\033[1A");
            cursor_line--;
            constrain_cursor();
        }
    }

    void cursor_down()
    {
        if (cursor_line < max_line) {
            put("\033[1B");
            cursor_line++;
            constrain_cursor();
        }
    }

    void cursor_right()
    {
        if (cursor_column < current_line_size()) {
            put("\033[1C");
            cursor_column++;
        }
    }

    void cursor_left()
    {
        // TODO: why >0? Are we 0-based?
        if (cursor_column > 0) {
            put("\033[1D");
            cursor_column--;
        }
    }
};
