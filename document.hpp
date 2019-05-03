#pragma once

#include <vector>
#include <string>

#include "output-utilities.hpp"
#include "tokenize.hpp"

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

    // LEAVES CURSOR AT END OF DISPLAY
    //
    // ONLY USE AT PROGRAM EXIT
    void render() const
    {
        if (cursor_line > 0)
            fprintf(g_tty_file, "\r\033[%dA", cursor_line);
        else
            fprintf(g_tty_file, "\r");

        for (auto const & line : contents) {
            fprintf(g_tty_file, "%s\n", render_colors(line).c_str());
            if (!isatty(STDOUT_FILENO))
                printf("%s\n", line.c_str());
        }
    }

    void render_current_line() const
    {
        const std::string & s = contents[cursor_line];
        fprintf(g_tty_file, "\r\033[0K"); // move to start and clear line
        fprintf(g_tty_file, "%s", render_colors(s).c_str());
        fprintf(g_tty_file, "\r\033[%dC", cursor_column);
        fflush(g_tty_file);
    }

    /////

    void insert_newline()
    {
        put("\r\n");
        cursor_line++;
        cursor_column = 0;
        if (cursor_line > max_line) {
            contents.emplace_back();
            max_line = cursor_line;
            put("\033[0K");
            put("\r\n");
            put("\033[1A");
        }
    }

    void insert_backspace()
    {
        if (cursor_column >= 1) {
            cursor_column--;
            contents[cursor_line].erase(cursor_column, 1);
            render_current_line();
        }
    }

    void insert_delete()
    {
        if (cursor_column < (int)contents[cursor_line].size()) {
            contents[cursor_line].erase(cursor_column, 1);
            render_current_line();
        }
    }

    void insert_key(Key k)
    {
        std::string & s = contents[cursor_line];
        if (cursor_column >= (int)s.size()) {
            s.push_back(k.k);
        }
        else if (overwrite) {
            s[cursor_column] = k.k;
        }
        else {
            s.insert(cursor_column, 1, k.k);
        }
        cursor_column++;
        render_current_line();
    }
};
