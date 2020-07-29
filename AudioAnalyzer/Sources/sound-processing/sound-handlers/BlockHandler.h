/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "SoundHandler.h"
#include "RainmeterWrappers.h"
#include "../../audio-utils/CustomizableValueTransformer.h"

namespace rxtd::audio_analyzer {
	class BlockHandler : public SoundHandler {
		using CVT = audio_utils::CustomizableValueTransformer;
	public:
		struct Params {
		private:
			friend BlockHandler;

			double resolution{ };
			bool subtractMean{ };
			string transform{ };

			double legacy_attackTime;
			double legacy_decayTime;
			audio_utils::CustomizableValueTransformer transformer{ };

			// autogenerated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.resolution == rhs.resolution
					&& lhs.subtractMean == rhs.subtractMean
					&& lhs.transform == rhs.transform;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		Params params{ };

		index samplesPerSec{ };
		index blockSize{ };

	protected:
		index counter = 0;

	private:
		float result = 0.0;

	public:
		void setParams(const Params& params, Channel channel);

		void setSamplesPerSec(index samplesPerSec) final;
		void reset() final;

		void _process(const DataSupplier& dataSupplier) final;

		void _finish(const DataSupplier& dataSupplier) final {
		}

		array_view<float> getData(index layer) const final {
			return { &result, 1 };
		}

		bool getProp(const isview& prop, utils::BufferPrinter& printer) const override;

		static std::optional<Params> parseParams(const OptionMap& optionMap, Logger& cl);

	protected:
		void setNextValue(double value);

		index getBlockSize() const {
			return blockSize;
		}

	private:
		void recalculateConstants();

	protected:
		virtual void _process(array_view<float> wave, float average) = 0;
		virtual void finishBlock() = 0;
		virtual void _reset() = 0;

		virtual void _setSamplesPerSec(index samplesPerSec) {
		}

		virtual bool isAverageNeeded() const {
			return false;
		}

		virtual sview getDefaultTransform() = 0;
	};

	class BlockRms : public BlockHandler {
		double intermediateResult = 0.0;

	public:
		void _process(array_view<float> wave, float average) override;

	protected:
		bool isAverageNeeded() const override {
			return true;
		}

		void finishBlock() override;
		void _reset() override;
		sview getDefaultTransform() override;
	};

	class BlockPeak : public BlockHandler {
		double intermediateResult = 0.0;

	public:
		void _process(array_view<float> wave, float average) override;

	protected:
		void finishBlock() override;
		void _reset() override;
		sview getDefaultTransform() override;
	};
}
