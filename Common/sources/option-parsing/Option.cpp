/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Option.h"

#include <mutex>
#include <thread>

#include "DataWithLock.h"
#include "OptionList.h"
#include "OptionMap.h"
#include "OptionSequence.h"
#include "Tokenizer.h"
#include "expressions/ASTParser.h"
#include "expressions/ASTSolver.h"
#include "expressions/GrammarBuilder.h"
#include "rainmeter/Logger.h"

using namespace common::options;
using StringUtils = utils::StringUtils;

sview Option::asString(sview defaultValue) const & {
	sview view = getView();
	if (view.empty()) {
		return defaultValue;
	}
	return view;
}

isview Option::asIString(isview defaultValue) const & {
	sview view = getView();
	if (view.empty()) {
		return defaultValue;
	}
	return view % ciView();

}

double Option::asFloat(double defaultValue) const {
	sview view = getView();
	if (view.empty()) {
		return defaultValue;
	}

	return parseNumber(view);
}

bool Option::asBool(bool defaultValue) const {
	const isview view = getView() % ciView();
	if (view.empty()) {
		return defaultValue;
	}
	if (view == L"true") {
		return true;
	}
	if (view == L"false") {
		return false;
	}
	return asFloat() != 0.0;
}

std::pair<GhostOption, GhostOption> Option::breakFirst(wchar_t separator) const & {
	sview view = getView();

	const auto delimiterPlace = view.find_first_of(separator);
	if (delimiterPlace == sview::npos) {
		return { GhostOption{ view }, {} };
	}

	auto first = GhostOption{ StringUtils::trim(sview(view.data(), delimiterPlace)) };
	auto rest = GhostOption{ StringUtils::trim(sview(view.data() + delimiterPlace + 1, view.size() - delimiterPlace - 1)) };
	return { first, rest };
}

std::pair<Option, Option> Option::breakFirst(wchar_t separator) && {
	auto result = breakFirst(separator);
	if (isOwningSource()) {
		result.first.own();
		result.second.own();
	}
	return result;
}

OptionMap Option::asMap(wchar_t optionDelimiter, wchar_t nameDelimiter) const & {
	return { getView(), optionDelimiter, nameDelimiter };
}

OptionMap Option::asMap(wchar_t optionDelimiter, wchar_t nameDelimiter) && {
	if (isOwningSource()) {
		return { std::move(*this).consumeSource(), optionDelimiter, nameDelimiter };
	} else {
		return { getView(), optionDelimiter, nameDelimiter };
	}
}

OptionList Option::asList(wchar_t delimiter) const & {
	return { getView(), Tokenizer::parse(getView(), delimiter) };
}

OptionList Option::asList(wchar_t delimiter) && {
	auto list = Tokenizer::parse(getView(), delimiter);
	if (isOwningSource()) {
		return { std::move(*this).consumeSource(), std::move(list) };
	} else {
		return { getView(), std::move(list) };
	}
}

OptionSequence Option::asSequence(
	wchar_t optionBegin, wchar_t optionEnd,
	wchar_t optionDelimiter
) const & {
	return { getView(), optionBegin, optionEnd, optionDelimiter };
}

OptionSequence Option::asSequence(
	wchar_t optionBegin, wchar_t optionEnd,
	wchar_t optionDelimiter
) && {
	if (isOwningSource()) {
		return { std::move(*this).consumeSource(), optionBegin, optionEnd, optionDelimiter };
	} else {
		return { getView(), optionBegin, optionEnd, optionDelimiter };
	}
}

//
// ASTParser initialization is expensive.
// It involves defining grammar with a lot of memory allocation and computing
// and initializing inner structures with a lot of memory allocations and computing.
//
// It doesn't make much sense to create a parser for each Option instance:
//		There are a lot of options, that doesn't even need number parsing.
//		Maybe it would be better to initialize parser only when needed,
//		but that's still expensive, considering that most options would only be used once.
//
// It doesn't make much sense to create a parser for each Option#parseNumber() call.
//
// It doesn't make sense to keep parsers in other classes with longer lifetime, like OptionMap:
//		Options is an independent class, it can be created from string.
//
// It would make a lot of sense to make parser a thread_local variable.
// Unfortunately, in DLLs thread local variables are not destructed on dll unload.
// But they are initialized again and again on every DLL load, which crates memory leak.
// Maybe this behaviour depends on compiler or system,
// but using thread_local is not a reliable solution.
//
// Global objects, unlike thead_local, are destroyed on dll unload.
//
// It's possible to use pointers as a thread_local value,
// and let global variables handle memory management,
// and since pointers are plain objects that don't need to be destructed,
// it would prevent memory leak.
// But then the pointer itself would leak memory from thread local storage,
// albeit only 8 bytes per thread per DLL load.
//
//
// I believe that ASTParser initialization is expensive enough
// to justify locking a mutex and std::this_thread::get_id()
// on each call to Option#parseNumber().
//
class ParserKeeper {
	struct ParserMap : public DataWithLock {
		std::map<std::thread::id, std::unique_ptr<common::expressions::ASTParser>> m;
	};

	ParserMap mapWithLock;

public:
	common::expressions::ASTParser& getParser(std::thread::id id) {
		common::expressions::ASTParser* parserPtr = mapWithLock.runGuarded(
			[&]() {
				auto& parserOpt = mapWithLock.m[id];
				if (parserOpt == nullptr) {
					parserOpt = std::make_unique<common::expressions::ASTParser>();

					initParser(*parserOpt);
				}

				return parserOpt.get();
			}
		);

		return *parserPtr;
	}

	static void initParser(common::expressions::ASTParser& parser) {
		common::expressions::GrammarBuilder builder;
		using Data = common::expressions::OperatorInfo::Data;

		// Previously Option could only parse: +, -, *, /, ^
		// So let's limit it to these operators

		builder.pushBinary(L"+", [](Data d1, Data d2) { return Data{ d1.value + d2.value }; });
		builder.pushBinary(L"-", [](Data d1, Data d2) { return Data{ d1.value - d2.value }; });

		builder.increasePrecedence();
		builder.pushBinary(L"*", [](Data d1, Data d2) { return Data{ d1.value * d2.value }; });
		builder.pushBinary(L"/", [](Data d1, Data d2) { return Data{ (d1.value == 0.0 ? 0.0 : d1.value / d2.value) }; });

		builder.increasePrecedence();
		builder.pushPrefix(L"+", [](Data d1, Data d2) { return d1; });
		builder.pushPrefix(L"-", [](Data d1, Data d2) { return Data{ -d1.value }; });

		builder.increasePrecedence();
		builder.pushBinary(L"^", [](Data d1, Data d2) { return Data{ std::pow(d1.value, d2.value) }; }, false);

		builder.pushGrouping(L"(", L")", L",");

		parser.setGrammar(std::move(builder).takeResult(), false);
	}
};

static ParserKeeper parserKeeper{}; // NOLINT(clang-diagnostic-exit-time-destructors)

double Option::parseNumber(sview source) {
	auto& parser = parserKeeper.getParser(std::this_thread::get_id());

	try {
		parser.parse(source);

		expressions::ASTSolver solver{ parser.takeTree() };
		return solver.solve(nullptr);
	} catch (expressions::ASTSolver::Exception& e) {
		buffer_printer::BufferPrinter printer;
		printer.print(L"can't parse '{}' as a number: {}", source, e.getMessage());
		rainmeter::Logger::sourcelessLog(printer.getBufferPtr());
	} catch (expressions::Lexer::Exception& e) {
		buffer_printer::BufferPrinter printer;
		printer.print(L"can't parse '{}' as a number: unknown token here: '{}'", source, source.substr(e.getPosition()));
		rainmeter::Logger::sourcelessLog(printer.getBufferPtr());
	} catch (expressions::ASTParser::Exception& e) {
		buffer_printer::BufferPrinter printer;
		printer.print(L"can't parse '{}' as a number: {}, at position: '{}'", source, e.getReason(), source.substr(e.getPosition()));
		rainmeter::Logger::sourcelessLog(printer.getBufferPtr());
	}

	return 0;
}

std::wostream& rxtd::common::options::operator<<(std::wostream& stream, const Option& opt) {
	stream << opt.asString();
	return stream;
}
