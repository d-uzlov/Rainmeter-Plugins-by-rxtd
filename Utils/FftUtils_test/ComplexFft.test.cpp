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
		using Float = double;
		using Fft = ComplexFft<Float>;
		Fft fft;

		std::vector<Fft::complex_type> wave;
		std::vector<Fft::complex_type> frequencies;
		std::vector<Fft::complex_type> wave2;

	public:
		TEST_METHOD(Test_16_1) {
			testForward(16, 1, 1.0e-6f);
			testReverse();
			testForward(16, 3, 1.0e-6f);
			testReverse();
			testForward(16, 5, 1.0e-6f);
			testReverse();
		}

		TEST_METHOD(Test_96_5) {
			testForward(96, 3, 1.0e-5f);
			testReverse();
		}

		TEST_METHOD(Test_1024_20) {
			testForward(1024, 20, 1.0e-6f);
			testReverse();
		}

		TEST_METHOD(Test_1024_250) {
			testForward(1024, 250, 1.0e-6f);
			testReverse();
		}

		TEST_METHOD(Test_1024_503) {
			testForward(1024, 503, 1.0e-6f);
			testReverse();
		}

	private:
		void doForward(index size, index desiredWaveSinCount) {
			wave.resize(static_cast<size_t>(size));
			generateSinWave(wave, static_cast<Float>(desiredWaveSinCount));

			fft.setParams(size, false);
			fft.forward(wave);

			fft.getResult().transferToVector(frequencies);
		}

		void testForward(index size, index desiredWaveSinCount, Float tolerance) {
			Assert::IsTrue(desiredWaveSinCount > 0);
			Assert::IsTrue(desiredWaveSinCount < size / 2);

			doForward(size, desiredWaveSinCount);

			auto freqs = array_span{ frequencies };
			for (index i = 0; i < freqs.size(); i++) {
				if (i == desiredWaveSinCount) {
					Assert::AreEqual(static_cast<Float>(0.0), freqs[i].real(), tolerance);
					Assert::AreEqual(static_cast<Float>(-0.5), freqs[i].imag(), tolerance);
				} else if (size - i == desiredWaveSinCount) {
					Assert::AreEqual(static_cast<Float>(0.0), freqs[i].real(), tolerance);
					Assert::AreEqual(static_cast<Float>(0.5), freqs[i].imag(), tolerance);
				} else {
					Assert::AreEqual(static_cast<Float>(0.0), freqs[i].real(), tolerance);
					Assert::AreEqual(static_cast<Float>(0.0), freqs[i].imag(), tolerance);
				}
			}
		}

		void testReverse() {
			const auto size = static_cast<index>(wave.size());
			fft.setParams(size, true);
			fft.inverse(frequencies);
			fft.getResult().transferToVector(wave2);

			assertArraysEqual(wave, wave2);
		}

		static void generateSinWave(array_span<Fft::complex_type> array, Float count) {
			Assert::IsTrue(count >= 1.0f);

			for (index i = 0; i < array.size(); i++) {
				array[i] = std::sin(count * static_cast<Float>(i) * MyMath::pi<Float>() * 2.0f / static_cast<Float>(array.size()));
			}
		}

		static void assertArraysEqual(array_view<Fft::complex_type> a, array_view<Fft::complex_type> b) {
			Assert::AreEqual(a.size(), b.size());
			for (index i = 0; i < a.size(); i++) {
				if (!MyMath::checkFloatEqual<Float>(a[i].real(), b[i].real(), 16.0, 16.0)) {
					Assert::AreEqual(a[i].real(), b[i].real());
				}
				if (!MyMath::checkFloatEqual<Float>(a[i].imag(), b[i].imag(), 16.0, 16.0)) {
					Assert::AreEqual(a[i].real(), b[i].real());
				}
			}
		}
	};
}
