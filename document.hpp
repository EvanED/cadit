#pragma once

#include <vector>
#include <string>

struct Document
{
    int cursor_line, cursor_column;
    int max_line;
    bool overwrite;
    std::vector<std::string> contents = {""};
};
