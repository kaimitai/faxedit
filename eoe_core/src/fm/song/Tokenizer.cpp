#include <format>
#include <stdexcept>
#include "Tokenizer.h"
#include "mml_constants.h"
#include "fm/fm_util.h"

fm::Tokenizer::Tokenizer(const std::string& p_str) :
	text{ p_str },
	index{ 0 },
	length{ p_str.size() },
	line{ 1 },
	column{ 1 }
{
}

std::vector<fm::Token> fm::Tokenizer::tokenize(void) {
	std::vector<fm::Token> tokens;

	while (!at_end()) {
		skip_whitespace();
		if (at_end()) break;

		char c = peek();

		// Directives (#song, #tempo, #sq1, etc.)
		if (c == '#')
			tokens.push_back(create_directive());
		else if (c == '@')
			tokens.push_back(create_label_or_ref());
		else if (c == '!')
			tokens.push_back(create_identifier());
		else if (c == '$')
			tokens.push_back(create_number_from_constant());
		else if (c == '{' || c == '}')
			tokens.push_back(create_brace());
		else if (c == '[' || c == ']')
			tokens.push_back(create_square_bracket());
		else if (c == 't' || c == 'T')
			tokens.push_back(create_tempo_set());
		else if (c == '<' || c == '>')
			tokens.push_back(create_octave_shift());
		else if ((c == 'o' || c == 'O') && std::isdigit((unsigned char)peek_next()))
			tokens.push_back(create_octave_set());
		else if ((c == 'v' || c == 'V'))
			tokens.push_back(create_volume_set());
		else if ((c == 's' || c == 'S'))
			tokens.push_back(create_song_transpose());
		else if ((c == 'p' || c == 'P'))
			tokens.push_back(create_percussion());
		else if (c == '_')
			tokens.push_back(create_channel_transpose());
		else if (c == '&')
			tokens.push_back(create_tie());
		else if (std::isdigit(static_cast<unsigned char>(c)))
			tokens.push_back(create_number());
		else if (c == '\"')
			tokens.push_back(create_string());
		else if (is_rest_start())
			tokens.push_back(create_rest());
		else if (is_note_start())
			tokens.push_back(create_note());
		else if (is_length_start())
			tokens.push_back(create_length());
		else
			advance();
	}

	fm::Token end_of_file;
	end_of_file.type = fm::TokenType::EndOfFile;
	tokens.push_back(end_of_file);

	return tokens;
}

// token creators
fm::Token fm::Tokenizer::create_directive() {
	fm::Token tok;

	tok.type = fm::TokenType::Directive;
	tok.line = line;
	tok.column = column;

	std::string value = "#";

	advance();

	while (!at_end()) {
		char c = peek();
		if (std::isalnum(static_cast<unsigned char>(c))) {
			value.push_back(std::tolower(c));
			advance();
		}
		else {
			break;
		}
	}

	tok.text = value;
	return tok;
}

fm::Token fm::Tokenizer::create_brace() {
	fm::Token tok;
	tok.type = TokenType::Brace;

	tok.line = line;
	tok.column = column;

	char c = advance(); // consume '{' or '}'

	tok.text = std::string(1, c);

	return tok;
}

fm::Token fm::Tokenizer::create_square_bracket() {
	fm::Token tok;
	tok.type = TokenType::SquareBracket;

	tok.line = line;
	tok.column = column;

	char c = advance(); // consume '[' or ']'

	tok.text = std::string(1, c);

	return tok;
}

fm::Token fm::Tokenizer::create_tempo_set() {
	fm::Token tok;
	tok.type = TokenType::TempoSet;

	tok.line = line;
	tok.column = column;

	advance(); // consume 't' or 'T'

	while (!at_end()) {
		char c = peek();
		if (std::isdigit(static_cast<unsigned char>(c)) ||
			c == '.' || c == '+' || c == '/') {
			tok.text.push_back(c);
			advance();
		}
		else {
			break;
		}
	}

	return tok;
}

fm::Token fm::Tokenizer::create_octave_shift() {
	fm::Token tok;
	tok.type = TokenType::OctaveShift;

	tok.line = line;
	tok.column = column;

	char c = advance(); // consume '<' or '>'

	tok.text = std::string(1, c);

	return tok;
}

fm::Token fm::Tokenizer::create_octave_set() {
	Token tok;
	tok.type = TokenType::OctaveSet;

	tok.line = line;
	tok.column = column; // Consume the 'o' or 'O'

	advance();
	std::string value = "o"; // Read the octave number (must be digits)

	while (!at_end()) {
		char c = peek();
		if (std::isdigit(static_cast<unsigned char>(c))) {
			value.push_back(c);
			advance();
		}
		else {
			break;
		}
	}

	tok.text = value; // Extract the numeric part (skip the 'o')
	tok.number = std::stoi(value.substr(1));

	return tok;
}

fm::Token fm::Tokenizer::create_volume_set() {
	Token tok;
	tok.type = TokenType::VolumeSet;

	tok.line = line;
	tok.column = column; // Consume the 'v' or 'V'

	advance();
	std::string value = "o"; // Read the volume number (must be digits)

	while (!at_end()) {
		char c = peek();
		if (std::isdigit(static_cast<unsigned char>(c))) {
			value.push_back(c);
			advance();
		}
		else {
			break;
		}
	}

	tok.text = value; // Extract the numeric part (skip the 'v')
	tok.number = std::stoi(value.substr(1));

	if (tok.number < 0 || tok.number > 15)
		throw std::runtime_error(std::format("Volume must be in the range 0-15 (line {} col {})", tok.line, tok.column));

	return tok;
}

fm::Token fm::Tokenizer::create_tie() {
	fm::Token tok;
	tok.type = TokenType::Tie;

	tok.line = line;
	tok.column = column;

	char c = advance(); // consume '&'

	tok.text = std::string(1, c);

	return tok;
}

fm::Token fm::Tokenizer::create_number() {
	Token tok;
	tok.type = TokenType::Number;

	tok.line = line;
	tok.column = column;

	std::string value;

	while (!at_end()) {
		char c = peek();
		if (std::isdigit(static_cast<unsigned char>(c))) {
			value.push_back(c);
			advance();
		}
		else {
			break;
		}
	}

	tok.text = value;
	tok.number = std::stoi(value);

	return tok;
}

fm::Token fm::Tokenizer::create_number_from_constant() {
	Token tok;
	tok.type = TokenType::Number;

	tok.line = line;
	tok.column = column;

	std::string value;

	char c = advance(); // consume '$'

	while (!at_end()) {
		char d = peek();
		if (std::isalnum((unsigned char)d) || d == '_') {
			value.push_back(d);
			advance();
		}
		else {
			break;
		}
	}

	tok.text = value;
	tok.number = fm::util::mml_constant_to_int(tok.text);

	return tok;
}

fm::Token fm::Tokenizer::create_note() {
	Token tok;
	tok.type = TokenType::Note;

	tok.line = line;
	tok.column = column;

	std::string value; // 1. Consume the note letter (a-g)

	char letter = peek();
	value.push_back(std::tolower(letter));

	advance(); // 2. Optional accidental (+ or - or #)
	char c = peek();

	// turn # into + for sharp notes
	if (c == '#')
		c = '+';

	if (c == '+' || c == '-') {
		value.push_back(c);
		advance();
	} // 3. Optional duration digits

	if (peek() == c::RAW_DELIM) {
		value.push_back(c::RAW_DELIM);
		advance();
	}

	while (!at_end()) {
		char d = peek();
		if (std::isdigit((unsigned char)d)) {
			value.push_back(d);
			advance();
		}
		else {
			break;
		}
	} // 4. Optional dots (.)

	while (!at_end() && peek() == '.') {
		value.push_back('.');
		advance();
	}

	tok.text = value;
	return tok;
}

fm::Token fm::Tokenizer::create_rest() {
	Token tok;
	tok.type = TokenType::Rest;

	tok.line = line;
	tok.column = column;

	std::string value;

	// 1. Consume the 'r' or 'R'
	char letter = peek();

	value.push_back(std::tolower(letter));
	advance();

	// RAW TICK LITERAL: f.ex. r~13
	if (!at_end() && peek() == c::RAW_DELIM) {
		value.push_back(c::RAW_DELIM);
		advance();

		// digits after '~'
		while (!at_end() && std::isdigit((unsigned char)peek())) {
			value.push_back(peek());
			advance();
		}
		// IMPORTANT: raw ticks do NOT accept dots
		tok.text = value;
		return tok;
	}

	// 2. Optional duration digits
	while (!at_end()) {
		char d = peek();
		if (std::isdigit((unsigned char)d)) {
			value.push_back(d);
			advance();
		}
		else {
			break;
		}
	}

	// 3. Optional dots
	while (!at_end() && peek() == '.') {
		value.push_back('.');
		advance();
	}

	tok.text = value;
	return tok;
}

fm::Token fm::Tokenizer::create_length() {
	Token tok;
	tok.type = TokenType::Length;

	tok.line = line;
	tok.column = column;

	std::string value;

	// 1. Consume the 'l' or 'L'
	advance();

	char letter = peek();

	// RAW TICK LITERAL: f.ex. r~13
	if (letter == c::RAW_DELIM) {
		value.push_back(c::RAW_DELIM);
		advance();

		// digits after '~'
		while (!at_end() && std::isdigit((unsigned char)peek())) {
			value.push_back(peek());
			advance();
		}
		// IMPORTANT: raw ticks do NOT accept dots
		tok.text = value;
		return tok;
	}

	// duration digits
	while (!at_end()) {
		char d = peek();
		if (std::isdigit((unsigned char)d)) {
			value.push_back(d);
			advance();
		}
		else {
			break;
		}
	}

	// 3. Optional dots
	while (!at_end() && peek() == '.') {
		value.push_back('.');
		advance();
	}

	tok.text = value;
	return tok;
}

fm::Token fm::Tokenizer::create_song_transpose() {
	Token tok;
	tok.type = TokenType::SongTranspose;

	tok.line = line;
	tok.column = column;

	std::string value;

	// 1. Consume the 'S' or 's'
	advance();
	// 2. verify that the next is _
	char c = peek();
	if (c != '_')
		throw std::runtime_error(std::format("Song transpose command must start with s_ (line {} col {})", tok.line, tok.column));

	advance(); // consume _

	int factor{ 1 };
	// get sign
	c = peek();
	if (c == '-') {
		factor = -1;
		advance();
	}
	else if (c == '+')
		advance();

	while (!at_end()) {
		char d = peek();
		if (std::isdigit((unsigned char)d)) {
			value.push_back(d);
			advance();
		}
		else {
			break;
		}
	}

	tok.number = factor * atoi(value.c_str());
	return tok;
}

fm::Token fm::Tokenizer::create_channel_transpose() {
	Token tok;
	tok.type = TokenType::ChannelTranspose;

	tok.line = line;
	tok.column = column;

	std::string value;

	advance(); // consume _

	int factor{ 1 };
	// get sign
	char c = peek();
	if (c == '-') {
		factor = -1;
		advance();
	}
	else if (c == '+')
		advance();

	while (!at_end()) {
		char d = peek();
		if (std::isdigit((unsigned char)d)) {
			value.push_back(d);
			advance();
		}
		else {
			break;
		}
	}


	tok.number = factor * atoi(value.c_str());

	return tok;
}

fm::Token fm::Tokenizer::create_percussion() {
	Token tok;
	tok.type = TokenType::Percussion;

	tok.line = line;
	tok.column = column;

	std::string value;

	advance(); // consume 'p' or 'P'

	while (!at_end()) {
		char d = peek();
		if (std::isdigit((unsigned char)d) || d == '*') {
			value.push_back(d);
			advance();
		}
		else {
			break;
		}
	}

	tok.text = value;

	return tok;
}

fm::Token fm::Tokenizer::create_label_or_ref(void) {
	Token tok;

	tok.line = line;
	tok.column = column;

	std::string value;

	// 1. First character: @
	char c = peek();

	value.push_back(std::tolower(c));
	advance();

	// 2. Subsequent characters: letters, digits, underscore

	while (!at_end()) {
		char d = peek();
		if (std::isalnum((unsigned char)d) || d == '_') {
			value.push_back(std::tolower(d));
			advance();
		}
		else {
			break;
		}
	}

	// 3. Label definition?

	if (!at_end() && peek() == ':') {
		advance(); // consume ':'
		tok.type = TokenType::LabelDef;
		tok.text = value; // store identifier without ':'
		return tok;
	}

	// 4. Normal identifier
	tok.type = TokenType::LabelRef;
	tok.text = value;

	return tok;
}

fm::Token fm::Tokenizer::create_string(void) {
	Token tok;

	tok.line = line;
	tok.column = column;

	std::string value;

	// 1. consume First character: "
	advance();

	while (!at_end()) {
		char d = advance();
		if (d == '\"') {
			break;
		}
		else {
			value.push_back(d);
		}
	}

	// 4. Normal identifier
	tok.type = TokenType::String;
	tok.text = value;

	return tok;
}

fm::Token fm::Tokenizer::create_identifier() {
	Token tok;

	tok.line = line;
	tok.column = column;

	std::string value;

	// 1. Discard first character: !
	advance();

	// 2. Subsequent characters: letters, digits, underscore
	while (!at_end()) {
		char d = peek();
		if (std::isalnum((unsigned char)d) || d == '_') {
			value.push_back(std::tolower(d));
			advance();
		}
		else {
			break;
		}
	}

	tok.type = TokenType::Identifier;
	tok.text = value;

	return tok;
}

// token creator helpers
bool fm::Tokenizer::is_note_start(void) const {
	char c = peek();
	return (c >= 'a' && c <= 'g') || (c >= 'A' && c <= 'G');
}

bool fm::Tokenizer::is_rest_start(void) const {
	char c = peek();

	return c == 'r' || c == 'R';
}

bool fm::Tokenizer::is_length_start(void) const {
	char c = peek();

	return c == 'l' || c == 'L';
}

bool fm::Tokenizer::is_whitespace(char c) const {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

char fm::Tokenizer::peek(void) const {
	if (index >= length)
		return '\0';
	else
		return text[index];
}

char fm::Tokenizer::peek_next(void) const {
	if (index + 1 >= length)
		return '\0';
	else
		return text[index + 1];
}

char fm::Tokenizer::advance(void) {
	char c{ peek() };

	if (c == '\n') {
		line++;
		column = 1;
	}
	else {
		column++;
	}

	index++;
	return c;
}

bool fm::Tokenizer::at_end(void) const {
	return index >= length;
}

void fm::Tokenizer::skip_whitespace(void) {
	while (!at_end()) {
		char c = peek();

		// Normal whitespace
		if (c == ' ' || c == '\t' || c == '\r') {
			advance();
			continue;
		}

		// Newlines
		if (c == '\n') {
			advance();
			continue;
		}

		// No more whitespace
		break;
	}
}
