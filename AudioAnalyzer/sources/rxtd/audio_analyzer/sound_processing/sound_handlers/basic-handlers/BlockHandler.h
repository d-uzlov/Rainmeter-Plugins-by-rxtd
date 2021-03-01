/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "rxtd/audio_analyzer/audio_utils/CustomizableValueTransformer.h"
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/HandlerBase.h"
#include "rxtd/filter_utils/LogarithmicIRF.h"

namespace rxtd::audio_analyzer::handler {
	class BlockHandler : public HandlerBase {
		using CVT = audio_utils::CustomizableValueTransformer;
		using LogarithmicIRF = filter_utils::LogarithmicIRF;

		struct Params {
			double updateInterval{};

			double attackTime{};
			double decayTime{};
			CVT transformer{};

			// autogenerated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.updateInterval == rhs.updateInterval
					&& lhs.transformer == rhs.transformer;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

		struct Snapshot {
			index blockSize{};
		};

		Params params{};

		index blockSize{};

		LogarithmicIRF filter;

	protected:
		index counter = 0;

	public:
		[[nodiscard]]
		bool vCheckSameParams(const ParamsContainer& p) const override {
			return compareParamsEquals(params, p);
		}

		[[nodiscard]]
		ParamsContainer vParseParams(
			const OptionMap& om, Logger& cl, const Rainmeter& rain,
			Version version
		) const override;

	protected:
		[[nodiscard]]
		ConfigurationResult vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) override;

		void vProcess(ProcessContext context, ExternalData& externalData) final;

		void setNextValue(float value);

		[[nodiscard]]
		index getBlockSize() const {
			return blockSize;
		}

		virtual void _process(array_view<float> wave) = 0;
		virtual void finishBlock() = 0;

		ExternalMethods::GetPropMethodType vGetExt_getProp() const override {
			return wrapExternalGetProp<Snapshot, &getProp>();
		}

	private:
		static bool getProp(
			const Snapshot& snapshot,
			isview prop,
			BufferPrinter& printer,
			const ExternalMethods::CallContext& context
		);
	};


	class BlockRms : public BlockHandler {
		double intermediateResult = 0.0;

	public:
		void _process(array_view<float> wave) override;

	protected:
		void finishBlock() override;
	};

	class BlockPeak : public BlockHandler {
		float intermediateResult = 0.0;

	public:
		void _process(array_view<float> wave) override;

	protected:
		void finishBlock() override;
	};
}
