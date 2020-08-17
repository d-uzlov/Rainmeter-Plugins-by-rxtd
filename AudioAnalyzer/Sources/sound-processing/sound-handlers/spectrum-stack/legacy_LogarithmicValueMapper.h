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
			float offset{ };
			double sensitivity{ };

			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.offset == rhs.offset
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
		bool checkSameParams(const std::any& p) const override {
			return compareParamsEquals(params, p);
		}

		void setParams(const std::any& p) override {
			params = std::any_cast<Params>(p);
		}

		[[nodiscard]]
		ParseResult parseParams(
			const OptionMap& om, Logger& cl, const Rainmeter& rain,
			index legacyNumber
		) const override;

	protected:
		[[nodiscard]]
		ConfigurationResult vConfigure(Logger& cl) override;

	public:
		void vProcess(array_view<float> wave) override;
		void vFinish() override;

	};
}
