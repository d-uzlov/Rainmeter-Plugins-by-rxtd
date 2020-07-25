/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <utility>
#include "SoundHandler.h"
#include "option-parser/OptionMap.h"
#include "RainmeterWrappers.h"
#include "image-utils/WaveFormDrawer.h"
#include "image-utils/ImageWriteHelper.h"
#include "../../audio-utils/CustomizableValueTransformer.h"

namespace rxtd::audio_analyzer {
	class WaveForm : public SoundHandler {
		using LDP = utils::WaveFormDrawer::LineDrawingPolicy;
		using FD = utils::WaveFormDrawer::FadingType;
		using SE = utils::WaveFormDrawer::SmoothEdges;
		using Colors = utils::WaveFormDrawer::Colors;
		using CVT = audio_utils::CustomizableValueTransformer;

	public:
		struct Params {
		private:
			friend WaveForm;

			double resolution{ };
			index width{ };
			index height{ };
			string folder = L".";
			Colors colors{ };
			LDP lineDrawingPolicy{ };
			SE edges{ };
			bool stationary{ };
			bool connected{ };
			index borderSize{ };
			FD fading{ };
			CVT transformer{ };

			// generated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.resolution == rhs.resolution
					&& lhs.width == rhs.width
					&& lhs.height == rhs.height
					&& lhs.folder == rhs.folder
					&& lhs.colors == rhs.colors
					&& lhs.lineDrawingPolicy == rhs.lineDrawingPolicy
					&& lhs.edges == rhs.edges
					&& lhs.stationary == rhs.stationary
					&& lhs.connected == rhs.connected
					&& lhs.borderSize == rhs.borderSize
					&& lhs.fading == rhs.fading
					&& lhs.transformer == rhs.transformer;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		class WaveformValueTransformer {
			CVT cvt;

		public:
			WaveformValueTransformer() = default;

			WaveformValueTransformer(CVT cvt) : cvt(std::move(cvt)) {
			}

			double apply(double value);

			void updateTransformations(index samplesPerSec, index blockSize);

			void reset();
		};

		index samplesPerSec{ };

		Params params;

		index blockSize{ };

		index counter = 0;
		double min{ };
		double max{ };
		bool changed = false;

		WaveformValueTransformer minTransformer{ };
		WaveformValueTransformer maxTransformer{ };
		double minDistinguishableValue{ };

		utils::WaveFormDrawer drawer{ };
		utils::ImageWriteHelper writerHelper{ };

		string filepath{ };

	public:
		void setParams(const Params& _params, Channel channel);

		static std::optional<Params> parseParams(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void _process(const DataSupplier& dataSupplier) override;
		void _processSilence(const DataSupplier& dataSupplier) override;
		void _finish(const DataSupplier& dataSupplier) override;

		array_view<float> getData(layer_t layer) const override {
			return { };
		}

		layer_t getLayersCount() const override {
			return 0;
		}

		bool getProp(const isview& prop, utils::BufferPrinter& printer) const override;

		bool isStandalone() override {
			return true;
		}

	private:
		void updateParams();

		void pushStrip(double min, double max);
	};
}
