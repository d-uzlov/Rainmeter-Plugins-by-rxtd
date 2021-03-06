// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#include <CppUnitTest.h>

#include "rxtd/fft_utils/ComplexFft.h"
#include "rxtd/std_fixes/MyMath.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using rxtd::std_fixes::MyMath;

namespace rxtd::test::fft_utils {
	using namespace rxtd::fft_utils;
	TEST_CLASS(ComplexFft_test) {
		static_assert(std::is_same<float, ComplexFft::scalar_type>::value);
		ComplexFft fft;

		std::vector<ComplexFft::complex_type> wave;
		std::vector<ComplexFft::complex_type> frequencies;
		std::vector<ComplexFft::complex_type> wave2;

	public:
		TEST_METHOD(Test_4) {
			testForward(4, 1, 1.0e-6f);
			testReverse();
		}

		TEST_METHOD(Test_6) {
			testForward(6, 1, 1.0e-6f);
			testReverse();
			testForward(6, 2, 1.0e-6f);
			testReverse();
		}

		TEST_METHOD(Test_10) {
			testForward(10, 1, 1.0e-6f);
			testReverse();
			testForward(10, 3, 1.0e-6f);
			testReverse();
		}

		TEST_METHOD(Test_12_1) {
			testForward(12, 1, 1.0e-6f);
			testReverse();
			testForward(12, 3, 1.0e-6f);
			testReverse();
			testForward(12, 5, 1.0e-6f);
			testReverse();
		}

		TEST_METHOD(Test_100_5) {
			testForward(100, 3, 1.0e-5f);
			testReverse();
		}

		TEST_METHOD(Test_1000_20) {
			testForward(1000, 20, 1.0e-5f);
			testReverse();
		}

		TEST_METHOD(Test_1000_250) {
			testForward(1000, 250, 1.0e-4f);
			testReverse();
		}

		TEST_METHOD(Test_1000_499) {
			testForward(1000, 499, 1.0e-4f);
			testReverse();
		}

	private:
		void doForward(index size, index desiredWaveSinCount) {
			wave.resize(static_cast<size_t>(size));
			generateSinWave(wave, static_cast<float>(desiredWaveSinCount));

			fft.setParams(size, false);
			fft.process(wave);

			fft.getResult().transferToVector(frequencies);
		}

		void testForward(index size, index desiredWaveSinCount, float tolerance) {
			Assert::IsTrue(desiredWaveSinCount > 0);
			Assert::IsTrue(desiredWaveSinCount < size / 2);

			doForward(size, desiredWaveSinCount);

			auto freqs = array_span{ frequencies };
			for (index i = 0; i < freqs.size(); i++) {
				if (i == desiredWaveSinCount) {
					Assert::AreEqual(0.0f, freqs[i].real(), tolerance);
					Assert::AreEqual(-0.5f, freqs[i].imag(), tolerance);
				} else if (size - i == desiredWaveSinCount) {
					Assert::AreEqual(0.0f, freqs[i].real(), tolerance);
					Assert::AreEqual(0.5f, freqs[i].imag(), tolerance);
				} else {
					Assert::AreEqual(0.0f, freqs[i].real(), tolerance);
					Assert::AreEqual(0.0f, freqs[i].imag(), tolerance);
				}
			}
		}

		void testReverse() {
			const auto size = static_cast<index>(wave.size());
			fft.setParams(size, true);
			fft.process(frequencies);
			fft.getResult().transferToVector(wave2);

			assertArraysEqual(wave, wave2);
		}

		static void generateSinWave(array_span<ComplexFft::complex_type> array, float count) {
			Assert::IsTrue(count >= 1.0f);

			for (index i = 0; i < array.size(); i++) {
				array[i] = std::sin(count * static_cast<float>(i) * MyMath::pif * 2.0f / static_cast<float>(array.size()));
			}
		}

		static void assertArraysEqual(array_view<ComplexFft::complex_type> a, array_view<ComplexFft::complex_type> b) {
			Assert::AreEqual(a.size(), b.size());
			for (index i = 0; i < a.size(); i++) {
				Assert::IsTrue(MyMath::checkFloatEqual(a[i].real(), b[i].real(), 64.0f, 8.0f));
				Assert::IsTrue(MyMath::checkFloatEqual(a[i].imag(), b[i].imag(), 64.0f, 8.0f));
			}
		}
	};
}
