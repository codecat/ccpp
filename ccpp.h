/* Codecat Preprocessor
 *
 * A general purpose single-header preprocessor library.
 *
 * Example usage:
 *   #define CCPP_IMPL
 *   #include "ccpp.h"
 *
 *   int main()
 *   {
 *     // Read contents of file "SomeFile.txt" into "buffer"
 *     FILE* fh = fopen("SomeFile.txt", "rb");
 *     fseek(fh, 0, SEEK_END);
 *     size_t size = ftell(fh);
 *     fseek(fh, 0, SEEK_SET);
 *
 *     char* buffer = (char*)malloc(size + 1);
 *     fread(buffer, 1, size, fh);
 *     fclose(fh);
 *     buffer[size] = '\0';
 *
 *     // Create a preprocessor
 *     ccpp::processor p;
 *
 *     // Add some definitions
 *     p.add_define("SOME_DEFINE");
 *
 *     // Begin processing
 *     p.process(buffer, size);
 *
 *     // Dump output
 *     printf("%s\n", buffer);
 *
 *     return 0;
 *   }
 *
 * Supported directives:
 *   #define <word>
 *   #undef <word>
 *   #if <condition>
 *   #endif
 *
 *
 * MIT License
 *
 * Copyright (c) 2018 codecat
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <vector>
#include <stack>

namespace ccpp
{
	extern char character;

	class processor
	{
	private:
		char* m_p;
		char* m_pEnd;

		size_t m_line;
		size_t m_column;

		std::vector<const char*> m_defines;
		std::stack<int> m_stack;

	public:
		processor();
		~processor();

		void add_define(const char* name);
		void remove_define(const char* name);

		bool has_define(const char* name);

		void process(char* buffer);
		void process(char* buffer, size_t len);

	private:
		bool test_condition();

		void expect_eol();
		void consume_line();

		void overwrite(char* p, size_t len);
	};
}

#if defined(CCPP_IMPL)

#include <cstring>
#include <malloc.h>

#ifndef CCPP_ERROR
#define CCPP_ERROR(error, ...) printf("[CCPP ERROR] " error "\n", __VA_ARGS__)
#endif

enum class ELexType
{
	None,

	Whitespace,
	Newline,
	Word,
	Operator,
};

static const char* lex_type_name(ELexType type)
{
	switch (type) {
	case ELexType::Whitespace: return "WHITESPACE";
	case ELexType::Newline: return "NEWLINE";
	case ELexType::Word: return "WORD";
	case ELexType::Operator: return "OPERATOR";
	}
	return "NONE";
}

static size_t lex(char* p, char* pEnd, ELexType &type)
{
	type = ELexType::None;

	char* pStart = p;

	while (p < pEnd) {
		char c = *p;
		bool isWhitespace = (c == ' ' || c == '\t' || c == '\r');
		bool isNewline = (c == '\n');
		bool isAlphaNum = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
		bool isOperator = (c == '!' || c == '&' || c == '|' || c == '(' || c == ')');

		if (type == ELexType::None) {
			if (isWhitespace) {
				type = ELexType::Whitespace;
			} else if (isNewline) {
				type = ELexType::Newline;
				p++;
				break;
			} else if (isAlphaNum) {
				type = ELexType::Word;
			} else if (isOperator) {
				type = ELexType::Operator;
			}

		} else {
			if (type == ELexType::Whitespace && !isWhitespace) {
				break;
			} else if (type == ELexType::Word && !isAlphaNum) {
				break;
			} else if (type == ELexType::Operator && !isOperator) {
				break;
			}
		}

		p++;
	}

	return p - pStart;
}

static size_t lex_expect(char* p, char* pEnd, ELexType expected_type)
{
	ELexType type;
	size_t len = lex(p, pEnd, type);

	if (type != expected_type) {
		CCPP_ERROR("Unexpected '%c', was expecting a %s", *p, lex_type_name(expected_type));
		return 0;
	}

	return len;
}

char ccpp::character = '#';

ccpp::processor::processor()
{
}

ccpp::processor::~processor()
{
	for (const char* p : m_defines) {
		free((void*)p);
	}
}

void ccpp::processor::add_define(const char* name)
{
	if (has_define(name)) {
		CCPP_ERROR("Definition \"%s\" already exists!", name);
		return;
	}

	size_t len = strlen(name);
	char* p = (char*)malloc(len + 1);
	memcpy(p, name, len + 1);

	m_defines.emplace_back(p);
}

void ccpp::processor::remove_define(const char* name)
{
	for (size_t i = 0; i < m_defines.size(); i++) {
		if (!strcmp(m_defines[i], name)) {
			m_defines.erase(m_defines.begin() + i);
			return;
		}
	}

	CCPP_ERROR("Couldn't undefine \"%s\" because it does not exist!", name);
}

bool ccpp::processor::has_define(const char* name)
{
	for (const char* p : m_defines) {
		if (!strcmp(p, name)) {
			return true;
		}
	}
	return false;
}

void ccpp::processor::process(char* buffer)
{
	process(buffer, strlen(buffer));
}

void ccpp::processor::process(char* buffer, size_t len)
{
	m_line = 1;
	m_column = 0;

	m_p = buffer;
	m_pEnd = buffer + len;

	while (m_p < m_pEnd) {
		bool isErasing = (m_stack.size() > 0 && m_stack.top() == 0);

		if (*m_p == '\n') {
			m_column = 0;
			m_line++;
			m_p++;
		} else {
			if (m_column++ > 0 || *m_p != character) {
				if (isErasing) {
					*m_p = ' ';
				}

				m_p++;
				continue;
			}

			char* commandStart = m_p++;

			// Expect a command word
			size_t lenCommand = lex_expect(m_p, m_pEnd, ELexType::Word);
			if (lenCommand == 0) {
				continue;
			}

			char* wordCommand = (char*)alloca(lenCommand + 1);
			memcpy(wordCommand, m_p, lenCommand);
			wordCommand[lenCommand] = '\0';

			m_p += lenCommand;

			if (!strcmp(wordCommand, "define")) {
				// #define <word>

				if (isErasing) {
					// Just consume the line if we're erasing
					consume_line();

				} else {
					// Expect some whitespace
					size_t lenCommandWhitespace = lex_expect(m_p, m_pEnd, ELexType::Whitespace);
					if (lenCommandWhitespace == 0) {
						continue;
					}
					m_p += lenCommandWhitespace;

					// Expect a define word
					size_t lenDefine = lex_expect(m_p, m_pEnd, ELexType::Word);
					if (lenDefine == 0) {
						continue;
					}

					char* wordDefine = (char*)alloca(lenDefine + 1);
					memcpy(wordDefine, m_p, lenDefine);
					wordDefine[lenDefine] = '\0';

					m_p += lenDefine;

					// Add define
					add_define(wordDefine);

					// Expect end of line
					expect_eol();
				}

			} else if (!strcmp(wordCommand, "undef")) {
				// #undef <word>

				if (isErasing) {
					// Just consume the line if we're erasing
					consume_line();

				} else {
					// Expect some whitespace
					size_t lenCommandWhitespace = lex_expect(m_p, m_pEnd, ELexType::Whitespace);
					if (lenCommandWhitespace == 0) {
						continue;
					}
					m_p += lenCommandWhitespace;

					// Expect a define word
					size_t lenDefine = lex_expect(m_p, m_pEnd, ELexType::Word);
					if (lenDefine == 0) {
						continue;
					}

					char* wordDefine = (char*)alloca(lenDefine + 1);
					memcpy(wordDefine, m_p, lenDefine);
					wordDefine[lenDefine] = '\0';

					m_p += lenDefine;

					// Undefine
					remove_define(wordDefine);

					// Expect end of line
					expect_eol();
				}

			} else if (!strcmp(wordCommand, "if")) {
				// #if <condition>

				if (isErasing) {
					// Just consume the line and push 0
					m_stack.push(0);
					consume_line();

				} else {
					// Expect some whitespace
					size_t lenCommandWhitespace = lex_expect(m_p, m_pEnd, ELexType::Whitespace);
					if (lenCommandWhitespace == 0) {
						continue;
					}
					m_p += lenCommandWhitespace;

					// Expect a condition
					bool conditionPassed = test_condition();

					// Push to the stack
					if (conditionPassed) {
						m_stack.push(1);
					} else {
						m_stack.push(0);
					}
				}

			} else if (!strcmp(wordCommand, "endif")) {
				// #endif

				// Expect end of line
				expect_eol();

				// Pop from stack
				m_stack.pop();

			} else {
				CCPP_ERROR("Unrecognized preprocessor command \"%s\" on line %d", wordCommand, (int)m_line);

				// Consume until end of line
				consume_line();
			}

			overwrite(commandStart, m_p - commandStart);
		}
	}
}

bool ccpp::processor::test_condition()
{
	//TODO:
	// &&
	// ||

	// Expect defined word
	size_t lenDefine = lex_expect(m_p, m_pEnd, ELexType::Word);

	char* wordDefine = (char*)alloca(lenDefine + 1);
	memcpy(wordDefine, m_p, lenDefine);
	wordDefine[lenDefine] = '\0';

	m_p += lenDefine;

	// Expect end of line
	expect_eol();

	// Check if word is defined
	return has_define(wordDefine);
}

void ccpp::processor::expect_eol()
{
	if (lex_expect(m_p, m_pEnd, ELexType::Newline) == 0) {
		return;
	}

	m_p++;

	m_line++;
	m_column = 0;
}

void ccpp::processor::consume_line()
{
	ELexType type = ELexType::None;
	while (type != ELexType::Newline) {
		m_p += lex(m_p, m_pEnd, type);
	}

	m_line++;
	m_column = 0;
}

void ccpp::processor::overwrite(char* p, size_t len)
{
	char* pEnd = p + len;
	for (; p < pEnd; p++) {
		if (*p != '\n') {
			*p = ' ';
		}
	}
}

#endif
