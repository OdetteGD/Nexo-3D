#include "lua_syntax_highlighter.h"

#include "scene/gui/text_edit.h"

#include <cctype>

void LuaSyntaxHighlighter::_update_cache() {
    // Colors are deliberately theme-neutral defaults; users can still switch editor themes.
}

Dictionary LuaSyntaxHighlighter::_get_line_syntax_highlighting_impl(int p_line) {
    Dictionary colors;
    if (!text_edit || p_line < 0 || p_line >= text_edit->get_line_count()) {
        return colors;
    }

    const String line = text_edit->get_line(p_line);
    const int length = line.length();
    const Color keyword_color(0.82, 0.55, 1.0);
    const Color string_color(0.88, 0.68, 0.48);
    const Color comment_color(0.48, 0.62, 0.48);
    const Color number_color(0.55, 0.76, 0.96);
    const Color function_color(0.45, 0.82, 0.86);
    const Color builtin_color(0.96, 0.78, 0.43);

    const HashSet<String> keywords = {
        "and", "break", "do", "else", "elseif", "end", "false", "for",
        "function", "goto", "if", "in", "local", "nil", "not", "or",
        "repeat", "return", "then", "true", "until", "while"
    };
    const HashSet<String> builtins = {
        "assert", "collectgarbage", "error", "getmetatable", "ipairs",
        "next", "pairs", "pcall", "print", "rawequal", "rawget", "rawlen",
        "rawset", "select", "setmetatable", "tonumber", "tostring", "type",
        "xpcall", "math", "string", "table", "utf8"
    };

    int i = 0;
    while (i < length) {
        const char32_t ch = line[i];
        if (ch == '-' && i + 1 < length && line[i + 1] == '-') {
            colors[i] = comment_color;
            break;
        }
        if (ch == '"' || ch == '\'') {
            const char32_t quote = ch;
            colors[i] = string_color;
            i++;
            while (i < length) {
                if (line[i] == '\\\\') {
                    i += MIN(2, length - i);
                    continue;
                }
                if (line[i] == quote) {
                    i++;
                    break;
                }
                i++;
            }
            continue;
        }
        if ((ch >= '0' && ch <= '9') || (ch == '.' && i + 1 < length && line[i + 1] >= '0' && line[i + 1] <= '9')) {
            colors[i] = number_color;
            i++;
            while (i < length && ((line[i] >= '0' && line[i] <= '9') || line[i] == '.' ||
                    (line[i] >= 'a' && line[i] <= 'f') || (line[i] >= 'A' && line[i] <= 'F') ||
                    line[i] == 'x' || line[i] == 'X' || line[i] == 'e' || line[i] == 'E' ||
                    line[i] == '+' || line[i] == '-')) {
                i++;
            }
            continue;
        }
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_') {
            const int start = i++;
            while (i < length && ((line[i] >= 'a' && line[i] <= 'z') ||
                    (line[i] >= 'A' && line[i] <= 'Z') ||
                    (line[i] >= '0' && line[i] <= '9') || line[i] == '_')) {
                i++;
            }
            const String word = line.substr(start, i - start);
            if (keywords.has(word)) {
                colors[start] = keyword_color;
            } else if (builtins.has(word)) {
                colors[start] = builtin_color;
            } else {
                int next = i;
                while (next < length && (line[next] == ' ' || line[next] == '\t')) {
                    next++;
                }
                if (next < length && line[next] == '(') {
                    colors[start] = function_color;
                }
            }
            continue;
        }
        i++;
    }
    return colors;
}

Ref<EditorSyntaxHighlighter> LuaSyntaxHighlighter::_create() const {
    Ref<LuaSyntaxHighlighter> highlighter;
    highlighter.instantiate();
    return highlighter;
}
