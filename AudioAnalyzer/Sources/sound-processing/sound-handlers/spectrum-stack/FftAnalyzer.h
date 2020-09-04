﻿/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../SoundHandler.h"
#include "RainmeterWrappers.h"
#include "../../../audio-utils/FFT.h"
#include "../../../audio-utils/FftCascade.h"
#include "../../../audio-utils/WindowFunctionHelper.h"

namespace rxtd::audio_analyzer {
	class FftAnalyzer : public SoundHandler {
		using WCF = audio_utils::WindowFunctionHelper::WindowCreationFunc;

	public:
		enum class SizeBy {
			BIN_WIDTH,
			SIZE,
			SIZE_EXACT
		};

		struct Params {
		private:
			friend FftAnalyzer;

			SizeBy legacy_sizeBy{ };
			double legacy_attackTime{ };
			double legacy_decayTime{ };
			bool legacy_correctZero{ };

			double binWidth{ };
			double overlap{ };

			index cascadesCount{ };

			double randomTest{ };
			double randomDuration{ };
			bool legacyAmplification{ };

			string wcfDescription{ };
			WCF wcf{ };

			// autogenerated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.legacy_sizeBy == rhs.legacy_sizeBy
					&& lhs.legacy_attackTime == rhs.legacy_attackTime
					&& lhs.legacy_decayTime == rhs.legacy_decayTime
					&& lhs.legacy_correctZero == rhs.legacy_correctZero
					&& lhs.binWidth == rhs.binWidth
					&& lhs.overlap == rhs.overlap
					&& lhs.cascadesCount == rhs.cascadesCount
					&& lhs.randomTest == rhs.randomTest
					&& lhs.randomDuration == rhs.randomDuration
					&& lhs.legacyAmplification == rhs.legacyAmplification
					&& lhs.wcfDescription == rhs.wcfDescription;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

		struct Snapshot {
			index fftSize{ };
			index sampleRate{ };
			index cascadesCount{ };
			std::vector<float> dc;
		};

	private:
		Params params{ };

		index fftSize = 0;
		index inputStride = 0;

		index randomBlockSize = 0;
		index randomCurrentOffset = 0;

		enum class RandomState { ON, OFF } randomState{ RandomState::ON };

		std::vector<audio_utils::FftCascade> cascades{ };

		audio_utils::FFT fft{ };

	public:
		[[nodiscard]]
		bool checkSameParams(const std::any& p) const override {
			return compareParamsEquals(params, p);
		}

		[[nodiscard]]
		ParseResult parseParams(
			const OptionMap& om, Logger& cl, const Rainmeter& rain,
			index legacyNumber
		) const override;

	protected:
		[[nodiscard]]
		ConfigurationResult vConfigure(const std::any& _params, Logger& cl, std::any& snapshotAny) override;

	public:
		index getFftSize() const {
			// todo is this even needed?
			return fftSize;
		}

		void vProcess(ProcessContext context, std::any& handlerSpecificData) override;

	private:
		static bool getProp(
			const Snapshot& snapshot,
			isview prop,
			utils::BufferPrinter& printer,
			const ExternCallContext& context
		);

		void processRandom(index waveSize, clock::time_point killTime);
	};
}
