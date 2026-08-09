#ifndef FM_TOKENIZER_H
#define FM_TOKENIZER_H

#include <string>
#include <vector>

namespace fm {

	enum class TokenType {
        Directive,          // #song, #tempo, #sq1, #tri, #noise
        Number,             // 1, 100, 16, 4, etc.
        Note,               // c16, g8., a+4, f#2, etc.
        Percussion,         // p0, p1, p1*4 etc
        Rest,               // r4, r8., r16
        Length,             // l4, l8.
        OctaveShift,        // < or >
        OctaveSet,          // o4, o5, etc.
        VolumeSet,          // 0 (silent) - 15 (max)
        Tie,                // &
        TempoSet,           // t120 - quarter notes per minute
        Brace,              // { or },
        SquareBracket,      // [ or ]
        LabelDef,           // @foo:
        LabelRef,           // @foo
        Identifier,         // Labels and label references
        ChannelTranspose,
        SongTranspose,
        String,
        EndOfFile           // sentinel
	};

    struct Token {
        TokenType type = fm::TokenType::EndOfFile;

        // For error reporting
        int line = 0;
        int column = 0;

        // Payload: only one is meaningful depending on type
        std::string text;   // for Directive, Note, Rest, Identifier, LabelDefinition
        int number = 0;     // for Number
    };


	class Tokenizer {

        std::string text;
        std::size_t index, length;
        int line, column;

        char peek(void) const;
        char peek_next(void) const;
        char advance(void);
        bool at_end(void) const;
        void skip_whitespace(void);

        // token creators
        fm::Token create_directive();
        fm::Token create_brace();
        fm::Token create_square_bracket();
        fm::Token create_tempo_set();
        fm::Token create_octave_shift();
        fm::Token create_octave_set();
        fm::Token create_volume_set();
        fm::Token create_tie();
        fm::Token create_number();
        fm::Token create_number_from_constant();
        fm::Token create_rest();
        fm::Token create_note();
        fm::Token create_percussion();
        fm::Token create_length();
        fm::Token create_label_or_ref();
        fm::Token create_identifier();
        fm::Token create_song_transpose();
        fm::Token create_channel_transpose();
        fm::Token create_string();

        // token creator helpers
        bool is_note_start(void) const;
        bool is_rest_start(void) const;
        bool is_length_start(void) const;
        bool is_whitespace(char c) const;

    public:
        Tokenizer(const std::string& p_str);
        std::vector<fm::Token> tokenize(void);
	};

}

#endif
