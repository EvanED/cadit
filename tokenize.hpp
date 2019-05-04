#pragma once

#include <string>

struct TokenColorer
{
    virtual std::string render_colors(std::string const & str) const = 0;
};

namespace token_colorers
{
    struct Cpp: TokenColorer
    {
        std::string render_colors(std::string const & str) const override;
    };
}
