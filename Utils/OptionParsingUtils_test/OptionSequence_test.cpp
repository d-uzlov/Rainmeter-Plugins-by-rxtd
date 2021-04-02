// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#include <CppUnitTest.h>

#include "rxtd/option_parsing/Option.h"
#include "rxtd/option_parsing/OptionParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using rxtd::option_parsing::OptionParser;

namespace Microsoft::VisualStudio::CppUnitTestFramework {
	template<>
	inline std::wstring ToString(const rxtd::sview& t) { return std::wstring{ t }; }

	template<>
	inline std::wstring ToString(const rxtd::sview* t) { return std::wstring{ *t }; }
}

namespace rxtd::test::option_parsing {
	using namespace rxtd::option_parsing;

	Logger createLogger() {
		return Logger{
			0, [](std_fixes::AnyContainer&, Logger::LogLevel, sview message) {
				Microsoft::VisualStudio::CppUnitTestFramework::Logger::WriteMessage(message.data());
				Microsoft::VisualStudio::CppUnitTestFramework::Logger::WriteMessage(L"\n");
			}
		};
	}

	TEST_CLASS(OptionSequence_basic) {
	public:
		TEST_METHOD(testNoArg) {
			auto opt = Option{ L"name" };
			auto seq = opt.asSequence(L'(', L')', L',', false, createLogger());
			Assert::AreEqual(static_cast<index>(1), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).name.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(0).args.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(0).postfix.asString());
		}

		TEST_METHOD(testEmptyArg) {
			auto opt = Option{ L"name()" };
			auto seq = opt.asSequence(L'(', L')', L',', false, createLogger());
			Assert::AreEqual(static_cast<index>(1), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).name.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(0).args.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(0).postfix.asString());
		}

		TEST_METHOD(testSimple) {
			auto opt = Option{ L"name(arg)" };
			auto seq = opt.asSequence(L'(', L')', L',', false, createLogger());
			Assert::AreEqual(static_cast<index>(1), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).name.asString());
			Assert::AreEqual(sview{ L"arg" }, seq.getElement(0).args.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(0).postfix.asString());
		}

		TEST_METHOD(testMany) {
			auto opt = Option{ L"name(arg), name2(arg2), name3, name4(arg4), name5" };
			auto seq = opt.asSequence(L'(', L')', L',', false, createLogger());
			Assert::AreEqual(static_cast<index>(5), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).name.asString());
			Assert::AreEqual(sview{ L"arg" }, seq.getElement(0).args.asString());

			Assert::AreEqual(sview{ L"name2" }, seq.getElement(1).name.asString());
			Assert::AreEqual(sview{ L"arg2" }, seq.getElement(1).args.asString());

			Assert::AreEqual(sview{ L"name3" }, seq.getElement(2).name.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(2).args.asString());

			Assert::AreEqual(sview{ L"name4" }, seq.getElement(3).name.asString());
			Assert::AreEqual(sview{ L"arg4" }, seq.getElement(3).args.asString());

			Assert::AreEqual(sview{ L"name5" }, seq.getElement(4).name.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(4).args.asString());

			for (auto e : seq) {
				Assert::AreEqual(sview{ L"" }, e.postfix.asString());
			}
		}

		TEST_METHOD(testSpacing) {
			auto opt = Option{ L"   	,,,,   ,   	name	  (	 arg	 	)	 ,,,	name2((arg2)) " };
			auto seq = opt.asSequence(L'(', L')', L',', false, createLogger());
			Assert::AreEqual(static_cast<index>(2), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).name.asString());
			Assert::AreEqual(sview{ L"arg" }, seq.getElement(0).args.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(0).postfix.asString());

			Assert::AreEqual(sview{ L"name2" }, seq.getElement(1).name.asString());
			Assert::AreEqual(sview{ L"(arg2)" }, seq.getElement(1).args.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(1).postfix.asString());
		}
	};

	TEST_CLASS(OptionSequence_layered) {
		TEST_METHOD(testLayered) {
			auto opt = Option{ L"name(arg, x(z))" };
			auto seq = opt.asSequence(L'(', L')', L',', false, createLogger());
			Assert::AreEqual(static_cast<index>(1), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).name.asString());
			Assert::AreEqual(sview{ L"arg, x(z)" }, seq.getElement(0).args.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(0).postfix.asString());
		}

		TEST_METHOD(testLayered_fail_notEnough) {
			Assert::ExpectException<OptionParser::Exception>(
				[]() {
					auto opt = Option{ L"name(arg, x((z))" };
					auto seq = opt.asSequence(L'(', L')', L',', false, createLogger());
				}
			);
		}

		TEST_METHOD(testLayered_fail_tooMany) {
			Assert::ExpectException<OptionParser::Exception>(
				[]() {
					auto opt = Option{ L"name(arg))" };
					auto seq = opt.asSequence(L'(', L')', L',', false, createLogger());
				}
			);
		}

		TEST_METHOD(test_fail_nameWithSpace) {
			Assert::ExpectException<OptionParser::Exception>(
				[]() {
					auto opt = Option{ L"name name2" };
					auto seq = opt.asSequence(L'(', L')', L',', false, createLogger());
				}
			);
		}

		TEST_METHOD(test_fail_noClosing) {
			Assert::ExpectException<OptionParser::Exception>(
				[]() {
					auto opt = Option{ L"name(" };
					auto seq = opt.asSequence(L'(', L')', L',', false, createLogger());
				}
			);
		}

		TEST_METHOD(test_fail_noOpening) {
			Assert::ExpectException<OptionParser::Exception>(
				[]() {
					auto opt = Option{ L"name)" };
					auto seq = opt.asSequence(L'(', L')', L',', false, createLogger());
				}
			);
		}
	};

	TEST_CLASS(OptionSequence_postfix) {
		TEST_METHOD(testPostfix_afterOptions) {
			auto opt = Option{ L"name()post" };
			auto seq = opt.asSequence(L'(', L')', L',', true, createLogger());
			Assert::AreEqual(static_cast<index>(1), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).name.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(0).args.asString());
			Assert::AreEqual(sview{ L"post" }, seq.getElement(0).postfix.asString());
		}

		TEST_METHOD(testPostfix_noOptions) {
			auto opt = Option{ L"name post" };
			auto seq = opt.asSequence(L'(', L')', L',', true, createLogger());
			Assert::AreEqual(static_cast<index>(1), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).name.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(0).args.asString());
			Assert::AreEqual(sview{ L"post" }, seq.getElement(0).postfix.asString());
		}

		TEST_METHOD(testPostfix_many) {
			auto opt = Option{ L"name(arg)post, name2()post2" };
			auto seq = opt.asSequence(L'(', L')', L',', true, createLogger());
			Assert::AreEqual(static_cast<index>(2), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).name.asString());
			Assert::AreEqual(sview{ L"arg" }, seq.getElement(0).args.asString());
			Assert::AreEqual(sview{ L"post" }, seq.getElement(0).postfix.asString());
			Assert::AreEqual(sview{ L"name2" }, seq.getElement(1).name.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(1).args.asString());
			Assert::AreEqual(sview{ L"post2" }, seq.getElement(1).postfix.asString());
		}

		TEST_METHOD(testPostfix_fail_delimOpen) {
			Assert::ExpectException<OptionParser::Exception>(
				[]() {
					auto opt = Option{ L"name()post()" };
					auto seq = opt.asSequence(L'(', L')', L',', true, createLogger());
				}
			);
		}

		TEST_METHOD(testPostfix_fail_delimClose) {
			Assert::ExpectException<OptionParser::Exception>(
				[]() {
					auto opt = Option{ L"name()post)" };
					auto seq = opt.asSequence(L'(', L')', L',', true, createLogger());
				}
			);
		}
	};
}
