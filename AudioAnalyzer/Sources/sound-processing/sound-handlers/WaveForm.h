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
		bool changed = false;

		WaveformValueTransformer minTransformer{ };
		WaveformValueTransformer maxTransformer{ };
		double minDistinguishableValue{ };

		utils::WaveFormDrawer drawer{ };
		utils::ImageWriteHelper writerHelper{ };

		string filepath{ };

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
			return { };
		}

		[[nodiscard]]
		LinkingResult vFinishLinking(Logger& cl) override;

	public:
		void vReset() override;
		void vProcess(const DataSupplier& dataSupplier) override;
		void vFinish() override;

		bool vGetProp(const isview& prop, utils::BufferPrinter& printer) const override;

		bool vIsStandalone() override {
			return true;
		}

	private:
		void pushStrip(double min, double max);
	};
}
