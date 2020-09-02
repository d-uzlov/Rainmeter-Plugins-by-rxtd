/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../SoundHandler.h"
#include "ResamplerProvider.h"
#include "audio-utils/filter-utils/LogarithmicIRF.h"

namespace rxtd::audio_analyzer {
	class TimeResampler : public ResamplerProvider {
	public:
		struct Params {
			double granularity{ };
			double attack{ };
			double decay{ };

			// autogenerated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.granularity == rhs.granularity
					&& lhs.attack == rhs.attack
					&& lhs.decay == rhs.decay;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		Params params{ };
		index blockSize{ };

		struct LayerData {
			index dataCounter{ };
			index waveCounter{ };
			audio_utils::LogarithmicIRF lowPass;
			std::vector<float> values;
		};

		std::vector<LayerData> layersData;

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
		void vProcess(ProcessContext context) override;

	private:
		void processLayer(index waveSize, index layer);
	};
}
