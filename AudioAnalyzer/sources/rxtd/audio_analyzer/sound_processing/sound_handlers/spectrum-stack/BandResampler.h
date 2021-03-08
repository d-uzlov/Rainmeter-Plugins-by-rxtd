// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "FftAnalyzer.h"
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/HandlerBase.h"
#include "rxtd/std_fixes/Vector2D.h"

namespace rxtd::audio_analyzer::handler {
	class BandResampler final : public HandlerBase {
		struct Params {
			std::vector<float> bandFreqs;
			bool useCubicResampling{};

			// autogenerated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.bandFreqs == rhs.bandFreqs
					&& lhs.useCubicResampling == rhs.useCubicResampling;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

		Params params{};

		FftAnalyzer* fftSource = nullptr;

		Vector2D<float> layerWeights;
		Vector2D<float> bandWeights;
		index bandsCount = 0;

		struct Snapshot {
			std::vector<float> bandFreqs;
		};

	public:
		[[nodiscard]]
		bool vCheckSameParams(const ParamsContainer& p) const override {
			return compareParamsEquals(params, p);
		}

		[[nodiscard]]
		index vGetSourcesCount() const override {
			return 1;
		}

		[[nodiscard]]
		ParamsContainer vParseParams(ParamParseContext& context) const noexcept(false) override;

	private:
		static void parseBandsElement(isview type, const Option& bandParams, std::vector<float>& freqs, Parser& parser, Logger& cl);

	protected:
		[[nodiscard]]
		ConfigurationResult vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) override;

	public:
		void vProcess(ProcessContext context, ExternalData& externalData) override;

		[[nodiscard]]
		array_view<float> getLayerWeights(index cascade) const {
			return layerWeights[cascade];
		}

		[[nodiscard]]
		array_view<float> getBandWeights(index band) const {
			return bandWeights[band];
		}

	protected:
		ExternalMethods::GetPropMethodType vGetExt_getProp() const override {
			return wrapExternalGetProp<Snapshot, &getProp>();
		}

	private:
		void sampleCascade(array_view<float> source, array_span<float> dest, float binWidth);

		// depends on fft size and sample rate
		void computeWeights(index fftSize);
		void computeCascadeWeights(array_span<float> result, index fftBinsCount, float binWidth);

		static bool getProp(
			const Snapshot& snapshot,
			isview prop,
			const ExternalMethods::CallContext& context
		);
	};
}
