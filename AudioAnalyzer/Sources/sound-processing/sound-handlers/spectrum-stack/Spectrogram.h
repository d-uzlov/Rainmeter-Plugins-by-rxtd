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
#include "Color.h"
#include "RainmeterWrappers.h"
#include "image-utils/StripedImage.h"
#include "image-utils/ImageWriteHelper.h"
#include "image-utils/StripedImageFadeHelper.h"

namespace rxtd::audio_analyzer {
	class Spectrogram : public SoundHandler {
	public:
		enum class ColorMixMode {
			eRGB,
			eHSV,
		};

		struct Params {
		private:
			friend Spectrogram;

			double resolution{ };
			index length{ };
			index borderSize{ };
			istring sourceName{ };
			string prefix = { };
			utils::Color borderColor{ };
			double fading{ };

			struct ColorDescription {
				float widthInverted{ };
				utils::Color color;

				friend bool operator==(const ColorDescription& lhs, const ColorDescription& rhs) {
					return lhs.widthInverted == rhs.widthInverted
						&& lhs.color == rhs.color;
				}

				friend bool operator!=(const ColorDescription& lhs, const ColorDescription& rhs) {
					return !(lhs == rhs);
				}
			};

			std::vector<float> colorLevels;
			std::vector<ColorDescription> colors;
			float colorMinValue{ };
			float colorMaxValue{ };
			ColorMixMode mixMode{ };

			bool stationary{ };

			//  autogenerated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.resolution == rhs.resolution
					&& lhs.length == rhs.length
					&& lhs.borderSize == rhs.borderSize
					&& lhs.sourceName == rhs.sourceName
					&& lhs.prefix == rhs.prefix
					&& lhs.borderColor == rhs.borderColor
					&& lhs.fading == rhs.fading
					&& lhs.colorLevels == rhs.colorLevels
					&& lhs.colors == rhs.colors
					&& lhs.colorMinValue == rhs.colorMinValue
					&& lhs.colorMaxValue == rhs.colorMaxValue
					&& lhs.mixMode == rhs.mixMode
					&& lhs.stationary == rhs.stationary;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		Params params;

		index blockSize{ };

		std::vector<utils::Color> stripBuffer{ };
		std::vector<utils::IntColor> intBuffer{ };
		index counter = 0;
		bool changed = false;

		utils::StripedImage<utils::IntColor> image{ };
		utils::StripedImageFadeHelper sifh{ };
		utils::ImageWriteHelper writerHelper{ };

		string filepath{ };

	public:
		[[nodiscard]]
		bool parseParams(
			const OptionMap& om, Logger& cl, const Rainmeter& rain,
			void* paramsPtr,
			index legacyNumber
		) const override;

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
			return params.sourceName;
		}

		[[nodiscard]]
		LinkingResult vFinishLinking(Logger& cl) override;

	public:
		void vProcess(array_view<float> wave) override;
		void vFinish() override;

		[[nodiscard]]
		bool vGetProp(const isview& prop, utils::BufferPrinter& printer) const override;

		[[nodiscard]]
		bool vIsStandalone() override {
			return true;
		}

	private:
		void fillStrip(array_view<float> data, array_span<utils::Color> buffer) const;
		void fillStripMulticolor(array_view<float> data, array_span<utils::Color> buffer) const;
	};
}
