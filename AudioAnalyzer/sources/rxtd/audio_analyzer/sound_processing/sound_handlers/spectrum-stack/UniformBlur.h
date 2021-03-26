// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/HandlerBase.h"

namespace rxtd::audio_analyzer::handler {
	class UniformBlur : public HandlerBase {
		struct Params {
			float blurRadius{};
			std::vector<float> blurCoefficients;

			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.blurRadius == rhs.blurRadius;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

		Params params{};

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

		void vProcessLayer(array_view<float> chunk, array_span<float> dest, ExternalData& handlerSpecificData) override;
	};
}
