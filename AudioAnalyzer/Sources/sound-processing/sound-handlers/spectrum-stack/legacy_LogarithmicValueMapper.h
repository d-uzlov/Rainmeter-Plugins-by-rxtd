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
#include "RainmeterWrappers.h"

namespace rxtd::audio_analyzer {
	class legacy_LogarithmicValueMapper : public SoundHandler {
	public:
		struct Params {
			istring sourceId;

			float offset{ };
			double sensitivity{ };

			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.sourceId == rhs.sourceId
					&& lhs.offset == rhs.offset
					&& lhs.sensitivity == rhs.sensitivity;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		Params params{ };

		float logNormalization{ };

		bool changed = true;

	public:
		[[nodiscard]]
		bool parseParams(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr) const override;

		[[nodiscard]]
		const Params& getParams() const {
			return params;
		}

		void setParams(const Params& value);

	protected:
		[[nodiscard]]
		isview vGetSourceName() const override {
			return params.sourceId;
		}

		[[nodiscard]]
		LinkingResult vFinishLinking(Logger& cl) override;

	public:
		void vProcess(array_view<float> wave) override;
		void vFinish() override;

	};
}
