#pragma once

#include "editor/script/syntax_highlighters.h"

class LuaSyntaxHighlighter : public EditorSyntaxHighlighter {
    GDCLASS(LuaSyntaxHighlighter, EditorSyntaxHighlighter)

public:
    virtual void _update_cache() override;
    virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override;
    virtual String _get_name() const override { return "Lua"; }
    virtual PackedStringArray _get_supported_languages() const override { return PackedStringArray{ "Lua", "lua" }; }
    virtual Ref<EditorSyntaxHighlighter> _create() const override;
};
