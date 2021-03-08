// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "rxtd/audio_analyzer/audio_utils/GaussianCoefficientsManager.h"
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/HandlerBase.h"

namespace rxtd::audio_analyzer::handler {
	class UniformBlur : public HandlerBase {
		struct Params {
			double blurRadius{};
			double blurRadiusAdaptation{};

			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.blurRadius == rhs.blurRadius
					&& lhs.blurRadiusAdaptation == rhs.blurRadiusAdaptation;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

		Params params{};
		audio_utils::GaussianCoefficientsManager gcm;

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

	protected:
		[[nodiscard]]
		ConfigurationResult vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) override;

	public:
		void vProcess(ProcessContext context, ExternalData& externalData) override;

	private:
		void blurCascade(array_view<float> source, array_span<float> dest, index radius);
	};
}
