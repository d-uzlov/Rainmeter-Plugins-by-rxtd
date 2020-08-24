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
#include "image-utils/WaveFormDrawer.h"
#include "image-utils/ImageWriteHelper.h"
#include "../../audio-utils/CustomizableValueTransformer.h"

namespace rxtd::audio_analyzer {
	class WaveForm : public SoundHandler {
		using LDP = utils::WaveFormDrawer::LineDrawingPolicy;
		using Colors = utils::WaveFormDrawer::Colors;
		using CVT = audio_utils::CustomizableValueTransformer;

	public:
		struct Params {
			double resolution{ };
			index width{ };
			index height{ };
			string folder = L".";
			Colors colors{ };
			LDP lineDrawingPolicy{ };
			bool stationary{ };
			bool connected{ };
			index borderSize{ };
			double fading{ };
			CVT transformer{ };

			// generated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.resolution == rhs.resolution
					&& lhs.width == rhs.width
					&& lhs.height == rhs.height
					&& lhs.folder == rhs.folder
					&& lhs.colors == rhs.colors
					&& lhs.lineDrawingPolicy == rhs.lineDrawingPolicy
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

		struct Snapshot {
			utils::Vector2D<utils::IntColor> pixels;
			utils::ImageWriteHelper writerHelper{ };
			string filepath;
			index blockSize{ };
			bool writeNeeded{ };
			bool empty{ };
		};

	private:
		class WaveformValueTransformer {
			CVT cvt;

		public:
			WaveformValueTransformer() = default;

			WaveformValueTransformer(CVT cvt) : cvt(std::move(cvt)) {
			}

			double apply(double value) {
				const bool positive = value > 0;
				value = cvt.apply(float(std::abs(value)));

				if (!positive) {
					value *= -1;
				}

				return value;
			}

			void updateTransformations(index sampleRate, index blockSize) {
				cvt.setParams(sampleRate, blockSize);
			}

			void reset() {
				cvt.resetState();
			}
		};

		Params params;

		index blockSize{ };

		index counter = 0;
		double min{ };
		double max{ };
		mutable bool writeNeeded = false;

		WaveformValueTransformer minTransformer{ };
		WaveformValueTransformer maxTransformer{ };
		double minDistinguishableValue{ };

		utils::WaveFormDrawer drawer{ };

		string filepath{ };

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

		void vReset() override;
		void vProcess(array_view<float> wave, clock::time_point killTime) override;


		void vConfigureSnapshot(std::any& handlerSpecificData) const override;
		void vUpdateSnapshot(std::any& handlerSpecificData) const override;

	private:
		static void staticFinisher(std::any& handlerSpecificData);
		void pushStrip(double min, double max);

		static bool getProp(const std::any& handlerSpecificData, isview prop, utils::BufferPrinter& printer);
	};
}
