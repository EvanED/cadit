#pragma once

#include <vector>
#include <string>
#include <memory>
#include <fstream>

#include "output-utilities.hpp"
#include "tokenize.hpp"

extern int g_num_cols;
extern std::unique_ptr<TokenColorer> g_token_colorer;

struct Document
{
    struct Line {
        std::string s = "";
        bool hard_break = true;

        Line (std::string && _s)
            : s(std::move(_s))
        {}

        Line() {}

        int size() const {
            return (int)s.size();
        }
    };

    int cursor_line, cursor_column;
    bool overwrite;
    std::vector<Line> contents = {Line()};
    int dead_lines = 0;

    void load(const char* filename)
    {
        std::ifstream file(filename);

        contents.clear();

        std::string line;
        while (std::getline(file, line))
            contents.emplace_back(std::move(line));

        render(Stream::TtyOnly);
    }

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

    void cursor_home()
    {
        put("\r");
        cursor_column = 0;
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
        if (cursor_line < (int)contents.size() - dead_lines - 1) {
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
        else if (cursor_line < (int)contents.size() - 1) {
            cursor_line++;
            cursor_column = 0;
            put("\033[A\r");
        }
    }

    void cursor_left()
    {
        if (cursor_column > 0) {
            put("\033[1D");
            cursor_column--;
        }
        else if (cursor_line > 0) {
            cursor_line--;
            cursor_column = current_line_size();
            fprintf(g_tty_file, "\033[A\033[%dC", cursor_column);
            fflush(g_tty_file);
        }
    }

    enum class Stream {
        TtyOnly,
        TtyAndStdout
    };


    void render(Stream s) const
    {
        if (cursor_line > 0)
            fprintf(g_tty_file, "\r\033[%dA\033[J", cursor_line);
        else
            fprintf(g_tty_file, "\r\033[J");

        for (int line_no = 0;
             line_no < (int)contents.size() - dead_lines;
             ++line_no)
        {
            auto const & line = contents[line_no].s;
            std::string render_line = line;
            if ((int)line.size() > g_num_cols) {
                render_line = line.substr(0, g_num_cols-1);
            }
            fprintf(g_tty_file, "%s\r\n", g_token_colorer->render_colors(render_line).c_str());
            if (!isatty(STDOUT_FILENO) and s == Stream::TtyAndStdout)
                printf("%s\n", line.c_str());
        }

        if (s != Stream::TtyAndStdout) {
            fprintf(g_tty_file, "\r\033[%dA", (int)contents.size() - cursor_line - 1);
        }
        fflush(g_tty_file);
    }

    void render_current_line() const
    {
        render_line(cursor_line);
    }

    void render_line(int line) const
    {
        const std::string & s = contents[line].s;
        std::string render_line = s;
        if ((int)s.size() > g_num_cols) {
            render_line = s.substr(0, g_num_cols-1);
        }
        fprintf(g_tty_file, "\r\033[0K"); // move to start and clear line
        fprintf(g_tty_file, "%s", g_token_colorer->render_colors(render_line).c_str());
        fprintf(g_tty_file, "\r\033[%dC", cursor_column);
        fflush(g_tty_file);
    }

    /////

    void insert_newline()
    {
        put("\r\n");

        if (dead_lines >= 1) {
            contents.pop_back();
            dead_lines--;
        }

        if(cursor_column < current_line_size()) {          
            contents.insert(contents.begin() + cursor_line + 1, std::string());

            std::string & current_line = contents[cursor_line].s;
            contents[cursor_line + 1].s = current_line.substr(cursor_column);
            current_line.erase(cursor_column);

            put("\033[A");
            put("\033[0J");

            for (auto line = contents.cbegin() + cursor_line;
                 line != contents.cend();
                 ++line)
            {
                fprintf(g_tty_file, "%s", g_token_colorer->render_colors(line->s).c_str());
                fprintf(g_tty_file, "\r\n");
            }

            fflush(g_tty_file);

            cursor_line++;
            cursor_column = 0;

            return;
        }
        
        cursor_line++;
        cursor_column = 0;
        if (cursor_line > (int)contents.size() - 1) {
            contents.emplace_back();
            put("\033[0K");
            put("\r\n");
            put("\033[1A");
        }
    }

    void insert_backspace()
    {
        if (cursor_column >= 1) {
            cursor_column--;
            contents[cursor_line].s.erase(cursor_column, 1);
            render_current_line();
        }
        else if (cursor_line >= 1) {
            cursor_column = contents[cursor_line - 1].size();
            contents[cursor_line - 1].s += contents[cursor_line].s;
            contents.erase(contents.begin() + cursor_line);
            contents.emplace_back();

            cursor_line--;
            dead_lines++;

            put("\033[A");
            render(Stream::TtyOnly);
        }
    }

    void insert_delete()
    {
        if (cursor_column < (int)contents[cursor_line].size()) {
            contents[cursor_line].s.erase(cursor_column, 1);
            render_current_line();
        }
        else if (cursor_line < (int)contents.size() - 1) {
            contents[cursor_line].s += contents[cursor_line + 1].s;
            contents.erase(contents.begin() + cursor_line + 1);
            contents.emplace_back();

            dead_lines++;

            render(Stream::TtyOnly);
        }
    }

    void insert_key(Key k)
    {
        std::string & s = contents[cursor_line].s;
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
