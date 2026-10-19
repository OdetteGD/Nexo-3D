/**************************************************************************/
/*  lua_syntax_highlighter.cpp                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/
#include "lua_syntax_highlighter.h"

#include "core/templates/hash_set.h"
#include "scene/gui/text_edit.h"

void LuaSyntaxHighlighter::_update_cache() {
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
	const Color default_color(0.86, 0.86, 0.86);

	const HashSet<String> keywords = {
		"and", "break", "do", "else", "elseif", "end", "false", "for",
		"function", "goto", "if", "in", "local", "nil", "not", "or",
		"repeat", "return", "then", "true", "until", "while"
	};
	const HashSet<String> builtins = {
		"assert", "error", "getmetatable", "ipairs", "next", "pairs",
		"pcall", "print", "rawequal", "rawget", "rawlen", "rawset",
		"select", "setmetatable", "tonumber", "tostring", "type", "xpcall",
		"math", "string", "table", "utf8"
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
			const int start = i++;
			while (i < length) {
				if (line[i] == '\\') {
					i += MIN(2, length - i);
					continue;
				}
				if (line[i] == quote) {
					i++;
					break;
				}
				i++;
			}
			colors[start] = string_color;
			colors[i] = default_color;
			continue;
		}
		if ((ch >= '0' && ch <= '9') || (ch == '.' && i + 1 < length && line[i + 1] >= '0' && line[i + 1] <= '9')) {
			const int start = i++;
			while (i < length) {
				const char32_t n = line[i];
				const bool digit = n >= '0' && n <= '9';
				const bool hex = (n >= 'a' && n <= 'f') || (n >= 'A' && n <= 'F');
				const bool exponent_sign = (n == '+' || n == '-') && i > start &&
						(line[i - 1] == 'e' || line[i - 1] == 'E' || line[i - 1] == 'p' || line[i - 1] == 'P');
				if (digit || n == '.' || hex || n == 'x' || n == 'X' || n == 'e' || n == 'E' || n == 'p' || n == 'P' || exponent_sign) {
					i++;
				} else {
					break;
				}
			}
			colors[start] = number_color;
			colors[i] = default_color;
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
			if (colors.has(start)) {
				colors[i] = default_color;
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
