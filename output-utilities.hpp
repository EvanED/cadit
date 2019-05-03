#pragma once

// TODO: Abstract

extern FILE* g_tty_file;
extern int g_tty_fd;

void put(char c)
{
    write(g_tty_fd, &c, 1);
}

template<size_t size>
void put(char const (&str)[size])
{
    if (write(g_tty_fd, str, size - 1) != size - 1)
        perror("write:");
}

struct Key
{
    int k = 0;
};
