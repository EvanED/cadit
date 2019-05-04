#pragma once

#include <string>
#include <memory>

struct TokenColorer
{
    virtual std::string render_colors(std::string const & str) const = 0;

    virtual std::unique_ptr<TokenColorer> next() const = 0;
};

namespace token_colorers
{
    struct Null: TokenColorer
    {
        std::string render_colors(std::string const & str) const override {
            return str;
        }

        std::unique_ptr<TokenColorer> next() const override;
    };

    struct Cpp: TokenColorer
    {
        std::string render_colors(std::string const & str) const override;
        std::unique_ptr<TokenColorer> next() const override;
    };

    inline std::unique_ptr<TokenColorer> Null::next() const { return std::make_unique<Cpp>(); }
    inline std::unique_ptr<TokenColorer> Cpp::next() const { return std::make_unique<Null>(); }
}
