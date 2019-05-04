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

#include "tokenize.hpp"

#include <boost/optional.hpp>
#include <utility>
#include <regex>
using std::string;
using std::regex;
using std::string;
using std::vector;
using std::pair;

struct TokenFormat
{
    regex re;
    string token;
    string format;

    TokenFormat(char const * re_, const char * t, const char * fmt)
        : re(re_)
        , token(t)
        , format(fmt)
    {}
};

TokenFormat const token_types[] = {
    // Valid CPP directives. Light magenty color
    { "^#(define|undef|if|elif|else|ifdef|ifndef|endif|line|error|include|pragma|_Pragma)",
      "CPP",
      "\033[38;5;134m"},

    // Invalid CPP directives. Red
    { "^#[^[:space:]]*",
      "BAD-CPP",
      "\033[38;5;160m"},

    // Keywords. Cyany blue thing
    { "^(alignas|alignof|and|and_eq|asm|atomic_cancel|atomic_commit|"
      "atomic_noexcept|auto|bitand|bitor|bool|break|case|catch|char|char16_t|"
      "char32_t|class|co_await|co_return|co_yield|compl|concept|const|"
      "const_cast|constexpr|continue|decltype|default|delete|do|double|"
      "dynamic_cast|else|enum|explicit|export|extern|false|float|for|friend|"
      "goto|if|import|inline|int|long|module|mutable|namespace|new|noexcept|"
      "not|not_eq|nullptr|operator|or|or_eq|private|protected|public|register|"
      "reinterpret_cast|requires|return|short|signed|sizeof|static|"
      "static_assert|static_cast|struct|switch|synchronized|template|this|"
      "thread_local|throw|true|try|typedef|typeid|typename|union|unsigned|using|"
      "virtual|void|volatile|wchar_t|while|xor|xor_eq|"
      "override|final|transaction_safe|transaction_safe_dynamic)",
      "KEY",
      "\033[38;5;86m"},

    // Strings. Greenish
    { "^\"([^\"]|\\\")*\"",
      "STR",
      "\033[38;5;28m"},

    // Chars. Greenish
    { "^'([^\']|\\\')*\'",
      "CHAR",
      "\033[38;5;40m"},

    // Comments. Pale
    { "^//.*",
      "COMM",
      "\033[38;5;95m"},

    // Delimiters. Yellow
    { "^(<|>|\\{|\\}|\\(|\\))",
      "DELIM",
      "\033[38;5;184m"},

    // Numbers. Blue
    { "^-?[.0-9](e[0-9]+)?",
      "NUM",
      "\033[38;5;33m"},

    // Identifiers. Nothing in particular
    { "^[_a-zA-Z][_a-zA-Z0-9]+",
      "IDENT",
      ""},
};


pair<string, string> longest_match(std::string const & str)
{
    boost::optional<std::smatch> best_match;
    string color;
    for (auto const & token : token_types) {
        std::smatch match;
        if (std::regex_search(str, match, token.re)) {
            if (!best_match || match.length() > best_match->length()) {
                best_match = match;
                color = token.format;
            }
        }
    }

    if (best_match) {
        return {color, best_match->str()};
    }
    else {
        string s;
        s.push_back(str[0]);
        return {"", s};
    }
}

vector<string> tokenize(std::string str)
{
    vector<string> ret;
    while (!str.empty()) {
        string color, match;
        std::tie(color, match) = longest_match(str);

        str = str.substr(match.size());
        ret.emplace_back(move(color) + move(match) + "\033[0m");
    }
    return ret;
}

namespace token_colorers
{
string Cpp::render_colors(std::string const & str) const
{
    auto tokens = tokenize(str);
    
    size_t needed = 0;
    for (auto const & token : tokens)
        needed += token.size();

    string ret;
    ret.reserve(needed);
    for (auto && token : tokens)
        ret.append(token);
    
    return ret;
}
}

#if 0
#include <iostream>

int main()
{
    for (auto const & token: tokenize("#include \"he")) {
        std::cout << token << "\n";
    }
}
#endif
