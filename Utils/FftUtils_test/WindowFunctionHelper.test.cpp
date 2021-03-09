// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#include <CppUnitTest.h>

#include "rxtd/fft_utils/WindowFunctionHelper.h"
#include "rxtd/std_fixes/MyMath.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using rxtd::std_fixes::MyMath;

namespace rxtd::test::fft_utils {
	using namespace rxtd::fft_utils;
	TEST_CLASS(WindowFunctionHelper_test) {
		static Logger createLogger() {
			return Logger{
				0, [](std_fixes::AnyContainer&, Logger::LogLevel, sview message) {
					Microsoft::VisualStudio::CppUnitTestFramework::Logger::WriteMessage(message.data());
					Microsoft::VisualStudio::CppUnitTestFramework::Logger::WriteMessage(L"\n");
				}
			};
		}

	public:
		TEST_METHOD(testCosineSum_100_05_Hann) {
			static const float predefined[] = {
				0.0f, 0.000986636f, 0.00394264f, 0.00885639f, 0.0157084f, 0.0244717f, 0.0351118f, 0.0475865f, 0.0618467f, 0.077836f, 0.0954915f, 0.114743f, 0.135516f, 0.157726f,
				0.181288f, 0.206107f, 0.232087f, 0.259123f, 0.28711f, 0.315938f, 0.345492f, 0.375655f, 0.406309f, 0.437333f, 0.468605f, 0.5f, 0.531395f, 0.562667f, 0.593691f,
				0.624345f, 0.654508f, 0.684062f, 0.71289f, 0.740877f, 0.767913f, 0.793893f, 0.818712f, 0.842274f, 0.864484f, 0.885257f, 0.904509f, 0.922164f, 0.938153f, 0.952413f,
				0.964888f, 0.975528f, 0.984292f, 0.991144f, 0.996057f, 0.999013f, 1.0f, 0.999013f, 0.996057f, 0.991144f, 0.984292f, 0.975528f, 0.964888f, 0.952414f, 0.938153f,
				0.922164f, 0.904509f, 0.885257f, 0.864484f, 0.842273f, 0.818712f, 0.793893f, 0.767913f, 0.740877f, 0.712889f, 0.684062f, 0.654508f, 0.624345f, 0.593691f, 0.562667f,
				0.531395f, 0.5f, 0.468605f, 0.437333f, 0.406309f, 0.375655f, 0.345491f, 0.315938f, 0.28711f, 0.259123f, 0.232087f, 0.206107f, 0.181288f, 0.157726f, 0.135516f,
				0.114743f, 0.0954915f, 0.0778359f, 0.0618467f, 0.0475865f, 0.0351118f, 0.0244717f, 0.0157084f, 0.00885636f, 0.00394264f, 0.000986636f
			};

			WindowFunctionHelper helper;
			std::vector<float> generated;
			generated.resize(100);

			helper.parse(L"hann", option_parsing::OptionParser::getDefault(), createLogger())(generated);
			assertArraysEqual(predefined, generated);
		}

		TEST_METHOD(testCosineSum_100_05_Hamming) {
			static const float predefined[] = {
				0.07672f, 0.0776309f, 0.0803601f, 0.0848969f, 0.0912233f, 0.0993143f, 0.109138f, 0.120656f, 0.133822f, 0.148584f, 0.164885f, 0.18266f, 0.201839f, 0.222346f,
				0.2441f, 0.267015f, 0.291001f, 0.315963f, 0.341803f, 0.368419f, 0.395705f, 0.423555f, 0.451857f, 0.480501f, 0.509373f, 0.53836f, 0.567347f, 0.596219f, 0.624863f,
				0.653165f, 0.681015f, 0.708301f, 0.734917f, 0.760757f, 0.785719f, 0.809705f, 0.83262f, 0.854374f, 0.874881f, 0.89406f, 0.911835f, 0.928136f, 0.942898f, 0.956064f,
				0.967582f, 0.977406f, 0.985497f, 0.991823f, 0.99636f, 0.999089f, 1.0f, 0.999089f, 0.99636f, 0.991823f, 0.985497f, 0.977406f, 0.967582f, 0.956064f, 0.942898f,
				0.928136f, 0.911835f, 0.89406f, 0.874881f, 0.854374f, 0.83262f, 0.809705f, 0.785719f, 0.760757f, 0.734917f, 0.708301f, 0.681014f, 0.653165f, 0.624863f, 0.596219f,
				0.567347f, 0.53836f, 0.509373f, 0.480501f, 0.451857f, 0.423555f, 0.395705f, 0.368419f, 0.341803f, 0.315963f, 0.291001f, 0.267015f, 0.2441f, 0.222346f, 0.201839f,
				0.18266f, 0.164885f, 0.148584f, 0.133822f, 0.120656f, 0.109138f, 0.0993142f, 0.0912233f, 0.0848969f, 0.0803601f, 0.0776309f
			};

			WindowFunctionHelper helper;
			std::vector<float> generated;
			generated.resize(100);

			helper.parse(L"hamming", option_parsing::OptionParser::getDefault(), createLogger())(generated);
			assertArraysEqual(predefined, generated);
		}

		TEST_METHOD(testKaiser_100_2) {
			static const float predefined[] = {
				0.0114799f, 0.0164246f, 0.0222363f, 0.028976f, 0.0367018f, 0.0454683f, 0.0553258f, 0.0663194f, 0.0784886f, 0.0918666f, 0.106479f, 0.122345f, 0.139475f, 0.15787f,
				0.177522f, 0.198416f, 0.220523f, 0.243809f, 0.268227f, 0.293719f, 0.32022f, 0.347654f, 0.375934f, 0.404966f, 0.434646f, 0.464862f, 0.495495f, 0.526419f, 0.5575f,
				0.5886f, 0.619579f, 0.650289f, 0.680584f, 0.710313f, 0.739327f, 0.767477f, 0.794616f, 0.820602f, 0.845293f, 0.868557f, 0.890264f, 0.910295f, 0.928538f, 0.944888f,
				0.959254f, 0.971553f, 0.981714f, 0.989679f, 0.995402f, 0.998849f, 1.0f, 0.998849f, 0.995402f, 0.989679f, 0.981714f, 0.971553f, 0.959254f, 0.944888f, 0.928538f,
				0.910295f, 0.890264f, 0.868557f, 0.845293f, 0.820602f, 0.794616f, 0.767477f, 0.739327f, 0.710313f, 0.680584f, 0.650289f, 0.619579f, 0.5886f, 0.5575f, 0.526419f,
				0.495495f, 0.464862f, 0.434646f, 0.404966f, 0.375934f, 0.347654f, 0.32022f, 0.293719f, 0.268227f, 0.243809f, 0.220523f, 0.198416f, 0.177522f, 0.15787f, 0.139475f,
				0.122345f, 0.106479f, 0.0918666f, 0.0784886f, 0.0663194f, 0.0553258f, 0.0454683f, 0.0367018f, 0.028976f, 0.0222363f, 0.0164246f
			};

			WindowFunctionHelper helper;
			std::vector<float> generated;
			generated.resize(100);

			helper.parse(L"kaiser(2)", option_parsing::OptionParser::getDefault(), createLogger())(generated);
			assertArraysEqual(predefined, generated);
		}

		TEST_METHOD(testKaiser_100_3) {
			static const float predefined[] = {
				0.000612336f, 0.00128142f, 0.00224266f, 0.00356445f, 0.00532256f, 0.0075998f, 0.0104855f, 0.0140746f, 0.018467f, 0.0237661f, 0.0300779f, 0.037509f, 0.0461651f,
				0.0561494f, 0.0675603f, 0.0804899f, 0.0950212f, 0.111227f, 0.129166f, 0.148883f, 0.170406f, 0.193745f, 0.218887f, 0.245802f, 0.274433f, 0.304703f, 0.336509f,
				0.369726f, 0.404203f, 0.439767f, 0.476225f, 0.513361f, 0.550941f, 0.588715f, 0.626419f, 0.663777f, 0.700505f, 0.736314f, 0.770916f, 0.804022f, 0.83535f, 0.864628f,
				0.891598f, 0.916015f, 0.937659f, 0.956328f, 0.971849f, 0.984077f, 0.992895f, 0.998219f, 1.0f, 0.998219f, 0.992895f, 0.984077f, 0.971849f, 0.956328f, 0.937659f,
				0.916015f, 0.891598f, 0.864628f, 0.83535f, 0.804022f, 0.770916f, 0.736314f, 0.700505f, 0.663777f, 0.626419f, 0.588715f, 0.550941f, 0.513361f, 0.476225f, 0.439767f,
				0.404203f, 0.369726f, 0.336509f, 0.304703f, 0.274433f, 0.245802f, 0.218887f, 0.193745f, 0.170406f, 0.148883f, 0.129166f, 0.111227f, 0.0950212f, 0.0804899f,
				0.0675603f, 0.0561494f, 0.0461651f, 0.037509f, 0.0300779f, 0.0237661f, 0.018467f, 0.0140746f, 0.0104855f, 0.0075998f, 0.00532256f, 0.00356445f, 0.00224266f,
				0.00128142f
			};

			WindowFunctionHelper helper;
			std::vector<float> generated;
			generated.resize(100);

			helper.parse(L"kaiser", option_parsing::OptionParser::getDefault(), createLogger())(generated);
			assertArraysEqual(predefined, generated);

			helper.parse(L"kaiser(3)", option_parsing::OptionParser::getDefault(), createLogger())(generated);
			assertArraysEqual(predefined, generated);
		}

		TEST_METHOD(testExponential_100_869) {
			static const float predefined[] = {
				0.367879f, 0.375311f, 0.382893f, 0.390628f, 0.398519f, 0.40657f, 0.414783f, 0.423162f, 0.431711f, 0.440432f, 0.449329f, 0.458406f, 0.467666f, 0.477114f, 0.486752f,
				0.496585f, 0.506617f, 0.516851f, 0.527292f, 0.537944f, 0.548812f, 0.559898f, 0.571209f, 0.582748f, 0.594521f, 0.606531f, 0.618783f, 0.631284f, 0.644036f, 0.657047f,
				0.67032f, 0.683861f, 0.697676f, 0.71177f, 0.726149f, 0.740818f, 0.755784f, 0.771052f, 0.786628f, 0.802519f, 0.818731f, 0.83527f, 0.852144f, 0.869358f, 0.88692f,
				0.904837f, 0.923116f, 0.941765f, 0.960789f, 0.980199f, 1.0f, 0.980199f, 0.960789f, 0.941765f, 0.923116f, 0.904837f, 0.88692f, 0.869358f, 0.852144f, 0.83527f,
				0.818731f, 0.802519f, 0.786628f, 0.771052f, 0.755784f, 0.740818f, 0.726149f, 0.71177f, 0.697676f, 0.683861f, 0.67032f, 0.657047f, 0.644036f, 0.631284f, 0.618783f,
				0.606531f, 0.594521f, 0.582748f, 0.571209f, 0.559898f, 0.548812f, 0.537944f, 0.527292f, 0.516851f, 0.506617f, 0.496585f, 0.486752f, 0.477114f, 0.467666f, 0.458406f,
				0.449329f, 0.440432f, 0.431711f, 0.423162f, 0.414783f, 0.40657f, 0.398519f, 0.390628f, 0.382893f, 0.375311f
			};

			WindowFunctionHelper helper;
			std::vector<float> generated;
			generated.resize(100);

			helper.parse(L"exponential(8.69)", option_parsing::OptionParser::getDefault(), createLogger())(generated);
			assertArraysEqual(predefined, generated);

			helper.parse(L"exponential", option_parsing::OptionParser::getDefault(), createLogger())(generated);
			assertArraysEqual(predefined, generated);
		}

		TEST_METHOD(testChebyshev_100_80) {
			static const float predefined[] = {
				0.00338714f, 0.00330047f, 0.00486025f, 0.00686419f, 0.00938819f, 0.0125136f, 0.0163264f, 0.0209169f, 0.026378f, 0.0328049f, 0.040293f, 0.0489372f, 0.0588301f,
				0.07006f, 0.0827098f, 0.0968548f, 0.112561f, 0.129884f, 0.148865f, 0.169532f, 0.191897f, 0.215954f, 0.241678f, 0.269026f, 0.297932f, 0.32831f, 0.360054f, 0.393035f,
				0.427104f, 0.462092f, 0.49781f, 0.534052f, 0.570598f, 0.607211f, 0.643644f, 0.67964f, 0.714939f, 0.749272f, 0.782376f, 0.813985f, 0.843843f, 0.871703f, 0.897329f,
				0.920501f, 0.941017f, 0.958698f, 0.973386f, 0.98495f, 0.993286f, 0.998318f, 1.0f, 0.998318f, 0.993286f, 0.98495f, 0.973386f, 0.958698f, 0.941017f, 0.920501f,
				0.897329f, 0.871703f, 0.843843f, 0.813985f, 0.782376f, 0.749272f, 0.714939f, 0.67964f, 0.643644f, 0.607211f, 0.570598f, 0.534052f, 0.49781f, 0.462092f, 0.427104f,
				0.393035f, 0.360054f, 0.32831f, 0.297932f, 0.269026f, 0.241678f, 0.215954f, 0.191897f, 0.169532f, 0.148865f, 0.129884f, 0.112561f, 0.0968548f, 0.0827098f, 0.07006f,
				0.0588301f, 0.0489372f, 0.040293f, 0.0328049f, 0.026378f, 0.0209169f, 0.0163264f, 0.0125136f, 0.00938819f, 0.00686419f, 0.00486025f, 0.00330047f
			};

			WindowFunctionHelper helper;
			std::vector<float> generated;
			generated.resize(100);

			helper.parse(L"chebyshev(80)", option_parsing::OptionParser::getDefault(), createLogger())(generated);
			assertArraysEqual(predefined, generated);

			helper.parse(L"chebyshev", option_parsing::OptionParser::getDefault(), createLogger())(generated);
			assertArraysEqual(predefined, generated);
		}

		TEST_METHOD(testChebyshev_150_100) {
			static const float predefined[] = {
				0.000456762f, 0.000451686f, 0.000670552f, 0.000955706f, 0.0013204f, 0.00177953f, 0.0023497f, 0.0030493f, 0.00389851f, 0.00491941f, 0.00613592f, 0.00757382f,
				0.00926073f, 0.0112261f, 0.0135009f, 0.0161179f, 0.0191113f, 0.0225166f, 0.0263702f, 0.0307097f, 0.0355732f, 0.0409992f, 0.0470263f, 0.0536927f, 0.0610363f,
				0.0690937f, 0.0779003f, 0.0874898f, 0.0978936f, 0.109141f, 0.121256f, 0.134263f, 0.14818f, 0.163021f, 0.178795f, 0.195507f, 0.213156f, 0.231734f, 0.25123f,
				0.271624f, 0.292891f, 0.314998f, 0.337906f, 0.36157f, 0.385937f, 0.410948f, 0.436537f, 0.462631f, 0.489153f, 0.516017f, 0.543134f, 0.570409f, 0.597743f, 0.625032f,
				0.65217f, 0.679046f, 0.705549f, 0.731566f, 0.756984f, 0.781689f, 0.805568f, 0.828511f, 0.850409f, 0.871157f, 0.890655f, 0.908807f, 0.925522f, 0.940717f, 0.954314f,
				0.966244f, 0.976446f, 0.984866f, 0.991461f, 0.996197f, 0.999048f, 1.0f, 0.999048f, 0.996197f, 0.991461f, 0.984866f, 0.976446f, 0.966244f, 0.954314f, 0.940717f,
				0.925522f, 0.908807f, 0.890655f, 0.871157f, 0.850409f, 0.828511f, 0.805568f, 0.781689f, 0.756984f, 0.731566f, 0.705549f, 0.679046f, 0.65217f, 0.625032f, 0.597743f,
				0.570409f, 0.543134f, 0.516017f, 0.489153f, 0.462631f, 0.436537f, 0.410948f, 0.385937f, 0.36157f, 0.337906f, 0.314998f, 0.292891f, 0.271624f, 0.25123f, 0.231734f,
				0.213156f, 0.195507f, 0.178795f, 0.163021f, 0.14818f, 0.134263f, 0.121256f, 0.109141f, 0.0978936f, 0.0874898f, 0.0779003f, 0.0690937f, 0.0610363f, 0.0536927f,
				0.0470263f, 0.0409992f, 0.0355732f, 0.0307097f, 0.0263702f, 0.0225166f, 0.0191113f, 0.0161179f, 0.0135009f, 0.0112261f, 0.00926073f, 0.00757382f, 0.00613592f,
				0.00491941f, 0.00389851f, 0.0030493f, 0.0023497f, 0.00177953f, 0.0013204f, 0.000955706f, 0.000670552f, 0.000451686f
			};

			WindowFunctionHelper helper;
			std::vector<float> generated;
			generated.resize(150);

			helper.parse(L"chebyshev(100)", option_parsing::OptionParser::getDefault(), createLogger())(generated);
			assertArraysEqual(predefined, generated);
		}

	private:
		static void assertArraysEqual(array_view<float> a, array_view<float> b) {
			Assert::AreEqual(a.size(), b.size());
			for (index i = 0; i < a.size(); i++) {
				Assert::IsTrue(MyMath::checkFloatEqual(a[i], b[i]));
			}
		}
	};
}
