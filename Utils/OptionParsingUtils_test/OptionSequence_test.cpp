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
	TEST_CLASS(OptionSequence_test) {
		static Logger createLogger() {
			return Logger{
				0, [](std_fixes::AnyContainer&, Logger::LogLevel, sview message) {
					Microsoft::VisualStudio::CppUnitTestFramework::Logger::WriteMessage(message.data());
					Microsoft::VisualStudio::CppUnitTestFramework::Logger::WriteMessage(L"\n");
				}
			};
		}

	public:
		TEST_METHOD(testNoArg) {
			auto opt = Option{ L"name" };
			auto seq = opt.asSequence(L'(', L')', L',', createLogger());
			Assert::AreEqual(static_cast<index>(1), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).first.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(0).second.asString());
		}

		TEST_METHOD(testEmptyArg) {
			auto opt = Option{ L"name()" };
			auto seq = opt.asSequence(L'(', L')', L',', createLogger());
			Assert::AreEqual(static_cast<index>(1), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).first.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(0).second.asString());
		}

		TEST_METHOD(testSimple) {
			auto opt = Option{ L"name(arg)" };
			auto seq = opt.asSequence(L'(', L')', L',', createLogger());
			Assert::AreEqual(static_cast<index>(1), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).first.asString());
			Assert::AreEqual(sview{ L"arg" }, seq.getElement(0).second.asString());
		}

		TEST_METHOD(testMany) {
			auto opt = Option{ L"name(arg), name2(arg2), name3, name4(arg4), name5" };
			auto seq = opt.asSequence(L'(', L')', L',', createLogger());
			Assert::AreEqual(static_cast<index>(5), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).first.asString());
			Assert::AreEqual(sview{ L"arg" }, seq.getElement(0).second.asString());
			Assert::AreEqual(sview{ L"name2" }, seq.getElement(1).first.asString());
			Assert::AreEqual(sview{ L"arg2" }, seq.getElement(1).second.asString());
			Assert::AreEqual(sview{ L"name3" }, seq.getElement(2).first.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(2).second.asString());
			Assert::AreEqual(sview{ L"name4" }, seq.getElement(3).first.asString());
			Assert::AreEqual(sview{ L"arg4" }, seq.getElement(3).second.asString());
			Assert::AreEqual(sview{ L"name5" }, seq.getElement(4).first.asString());
			Assert::AreEqual(sview{ L"" }, seq.getElement(4).second.asString());
		}

		TEST_METHOD(testSpacing) {
			auto opt = Option{ L"   	,,,,   ,   	name	  (	 arg	 	)	 ,,,	name2((arg2)) " };
			auto seq = opt.asSequence(L'(', L')', L',', createLogger());
			Assert::AreEqual(static_cast<index>(2), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).first.asString());
			Assert::AreEqual(sview{ L"arg" }, seq.getElement(0).second.asString());
			Assert::AreEqual(sview{ L"name2" }, seq.getElement(1).first.asString());
			Assert::AreEqual(sview{ L"(arg2)" }, seq.getElement(1).second.asString());
		}

		TEST_METHOD(testLayered) {
			auto opt = Option{ L"name(arg, x(z))" };
			auto seq = opt.asSequence(L'(', L')', L',', createLogger());
			Assert::AreEqual(static_cast<index>(1), seq.getSize());
			Assert::AreEqual(sview{ L"name" }, seq.getElement(0).first.asString());
			Assert::AreEqual(sview{ L"arg, x(z)" }, seq.getElement(0).second.asString());
		}

		TEST_METHOD(testLayered_fail_notEnough) {
			Assert::ExpectException<OptionParser::Exception>(
				[]() {
					auto opt = Option{ L"name(arg, x((z))" };
					auto seq = opt.asSequence(L'(', L')', L',', createLogger());
				}
			);
		}

		TEST_METHOD(testLayered_fail_tooMany) {
			Assert::ExpectException<OptionParser::Exception>(
				[]() {
					auto opt = Option{ L"name(arg))" };
					auto seq = opt.asSequence(L'(', L')', L',', createLogger());
				}
			);
		}

		TEST_METHOD(test_fail_noClosing) {
			Assert::ExpectException<OptionParser::Exception>(
				[]() {
					auto opt = Option{ L"name(" };
					auto seq = opt.asSequence(L'(', L')', L',', createLogger());
				}
			);
		}

		TEST_METHOD(test_fail_noOpening) {
			Assert::ExpectException<OptionParser::Exception>(
				[]() {
					auto opt = Option{ L"name)" };
					auto seq = opt.asSequence(L'(', L')', L',', createLogger());
				}
			);
		}
	};
}
