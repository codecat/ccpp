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
 *   #else
 *   #elif <condition>
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
#include <functional>
#include <cstdint>

namespace ccpp
{
	extern char character;

	class processor
	{
		typedef std::function<bool(const char* path)> include_callback_t;
		typedef std::function<bool(const char* command, const char* value)> command_callback_t;

	private:
		char* m_p;
		char* m_pEnd;

		size_t m_line;
		size_t m_column;

		std::vector<const char*> m_defines;
		std::stack<uint32_t> m_stack;

		include_callback_t m_includeCallback;
		command_callback_t m_commandCallback;

	public:
		processor();
		~processor();

		void add_define(const char* name);
		void remove_define(const char* name);

		bool has_define(const char* name);

		void set_include_callback(const include_callback_t &callback);
		void set_command_callback(const command_callback_t &callback);

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
	String,
};

static const char* lex_type_name(ELexType type)
{
	switch (type) {
	case ELexType::Whitespace: return "WHITESPACE";
	case ELexType::Newline: return "NEWLINE";
	case ELexType::Word: return "WORD";
	case ELexType::Operator: return "OPERATOR";
	case ELexType::String: return "STRING";
	}
	return "NONE";
}

static size_t lex(char* p, char* pEnd, ELexType &type)
{
	type = ELexType::None;

	char* pStart = p;

	if (*p == '\n') {
		type = ELexType::Newline;
		p++;

	} else if (*p == '"') {
		type = ELexType::String;

		while (p < pEnd) {
			p++;
			if (*p == '\\') {
				// Skip next character
				p++;
			} else if (*p == '"') {
				// End of string
				p++;
				break;
			}
		}

	} else {
		while (p < pEnd) {
			char c = *p;
			bool isWhitespace = (c == ' ' || c == '\t' || c == '\r');
			bool isAlphaNum = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
			bool isOperator = (c == '!' || c == '&' || c == '|' || c == '(' || c == ')');

			if (type == ELexType::None) {
				if (isWhitespace) {
					type = ELexType::Whitespace;
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
	}

	return p - pStart;
}

static size_t lex_expect(char* p, char* pEnd, ELexType expected_type)
{
	ELexType type;
	size_t len = lex(p, pEnd, type);

	if (type != expected_type) {
		CCPP_ERROR("Unexpected '%c' of type %s, was expecting a %s", *p, lex_type_name(type), lex_type_name(expected_type));
		return 0;
	}

	return len;
}

char ccpp::character = '#';

enum
{
	// Contents must pass
	Scope_Passing = (1 << 0),

	// Contents must be erased
	Scope_Erasing = (1 << 1),

	// Contents are inside of an else directive
	Scope_Else = (1 << 2),

	// Contents are inside of an else if directive
	Scope_ElseIf = (1 << 3),

	// Contents are deep and should be ignored
	Scope_Deep = (1 << 4),
};

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

void ccpp::processor::set_include_callback(const include_callback_t &callback)
{
	m_includeCallback = callback;
}

void ccpp::processor::set_command_callback(const command_callback_t &callback)
{
	m_commandCallback = callback;
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
		bool isErasing = false;
		bool isDeep = false;

		if (m_stack.size() > 0) {
			const uint32_t &scope = m_stack.top();

			isErasing = (scope & Scope_Erasing);
			isDeep = (scope & Scope_Deep);
		}

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
					// Just consume the line and push erasing at deep level
					m_stack.push(Scope_Erasing | Scope_Deep);
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
						m_stack.push(Scope_Passing);
					} else {
						m_stack.push(Scope_Erasing);
					}
				}

			} else if (!strcmp(wordCommand, "else")) {
				// #else

				if (isErasing && isDeep) {
					// Just consume the line if we're deep
					consume_line();

				} else {
					// Get top of stack
					uint32_t &top = m_stack.top();

					// Error out if we're already in an else directive
					if (top & Scope_Else) {
						CCPP_ERROR("Unexpected #else on line %d", (int)m_line);

					} else {
						if (top & Scope_Passing) {
							// If we're passing, set scope to erasing else
							top = Scope_Erasing | Scope_Else;

						} else if (top & Scope_Erasing) {
							// If we're erasing, set scope to passing else
							top = Scope_Passing | Scope_Else;
						}
					}

					// Expect end of line
					expect_eol();
				}

			} else if (!strcmp(wordCommand, "elif")) {
				// #elif <condition>

				if (isErasing && isDeep) {
					// Just consume the line if we're deep
					consume_line();

				} else {
					// Get top of stack
					uint32_t &top = m_stack.top();

					// Error out if we're already in an else directive
					if (top & Scope_Else) {
						CCPP_ERROR("Unexpected #elif on line %d", (int)m_line);
						consume_line();

					} else {
						if (top & Scope_Passing) {
							// If we're already passing, we'll erase anything below and set the deep flag to ignore the rest
							top = Scope_Erasing | Scope_ElseIf | Scope_Deep;
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

							// Update the scope
							if (conditionPassed) {
								top = Scope_Passing | Scope_ElseIf;
							} else {
								top = Scope_Erasing | Scope_ElseIf;
							}
						}
					}
				}

			} else if (!strcmp(wordCommand, "endif")) {
				// #endif

				// Expect end of line
				expect_eol();

				// Pop from stack
				m_stack.pop();

			} else if (!strcmp(wordCommand, "include")) {
				// #include <path>

				if (isErasing) {
					// Just consume the line if we're erasing
					consume_line();

				} else {
					if (m_includeCallback == nullptr) {
						// If no callback is set up, just consume the line
						CCPP_ERROR("No include callback set up for #include on line %d", (int)m_line);
						consume_line();

					} else {
						// Expect some whitespace
						size_t lenCommandWhitespace = lex_expect(m_p, m_pEnd, ELexType::Whitespace);
						if (lenCommandWhitespace == 0) {
							continue;
						}
						m_p += lenCommandWhitespace;

						// Expect a string
						size_t lenPath = lex_expect(m_p, m_pEnd, ELexType::String);
						if (lenPath == 0) {
							continue;
						}

						char* path = (char*)alloca(lenPath);
						memcpy(path, m_p + 1, lenPath - 2);
						path[lenPath - 2] = '\0';

						m_p += lenPath;

						// Run callback
						if (!m_includeCallback(path)) {
							CCPP_ERROR("Failed to include \"%s\" on line %d", path, (int)m_line);
						}

						// Expect end of line
						expect_eol();
					}
				}

			} else {
				// Unknown command, it can be handled by the callback, or throw an error
				bool commandFound = false;
				int line = (int)m_line;

				char* commandValueStart = m_p;

				// Consume until end of line
				consume_line();

				// Handle if not erasing
				if (!isErasing) {
					// See if there is a custom command callback
					if (m_commandCallback != nullptr) {
						ELexType typeCommandValue;
						size_t lenCommandValue = lex(commandValueStart, m_pEnd, typeCommandValue);

						if (typeCommandValue == ELexType::Whitespace) {
							// Handle potential whitespace
							commandValueStart += lenCommandValue;
							lenCommandValue = lex(commandValueStart, m_pEnd, typeCommandValue);
						}

						if (typeCommandValue == ELexType::Newline) {
							// If end of line, there's no command value
							commandFound = m_commandCallback(wordCommand, nullptr);

						} else {
							// If not end of line yet, there's some value
							char* commandValue = (char*)alloca(lenCommandValue + 1);
							memcpy(commandValue, commandValueStart, lenCommandValue);
							commandValue[lenCommandValue] = '\0';

							commandFound = m_commandCallback(wordCommand, commandValue);
						}
					}

					if (!commandFound) {
						CCPP_ERROR("Unrecognized preprocessor command \"%s\" on line %d", wordCommand, line);
					}
				}
			}

			overwrite(commandStart, m_p - commandStart);
		}
	}
}

bool ccpp::processor::test_condition()
{
	//TODO:
	// !
	// &&
	// ||
	// ()

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
