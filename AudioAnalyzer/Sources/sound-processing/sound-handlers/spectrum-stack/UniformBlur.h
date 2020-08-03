/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../SoundHandler.h"
#include "ResamplerProvider.h"
#include "Vector2D.h"
#include "../../../audio-utils/GaussianCoefficientsManager.h"

namespace rxtd::audio_analyzer {
	class UniformBlur : public ResamplerProvider {
	public:
		struct Params {
			istring sourceId;

			double blurRadius{ };
			double blurRadiusAdaptation{ };

			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.sourceId == rhs.sourceId
					&& lhs.blurRadius == rhs.blurRadius
					&& lhs.blurRadiusAdaptation == rhs.blurRadiusAdaptation;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		Params params{ };
		audio_utils::GaussianCoefficientsManager gcm;

		bool changed = true;

	public:
		[[nodiscard]]
		bool parseParams(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr) const override;

		[[nodiscard]]
		const Params& getParams() const {
			return params;
		}

		void setParams(const Params& value) {
			params = value;
		}

	protected:
		[[nodiscard]]
		isview vGetSourceName() const override {
			return params.sourceId;
		}

		[[nodiscard]]
		LinkingResult vFinishLinking(Logger& cl) override;

	public:
		void vProcess(const DataSupplier& dataSupplier) override;
		void vFinish() override;

	private:
		void blurCascade(array_view<float> source, array_span<float> dest, index radius);
	};
}
