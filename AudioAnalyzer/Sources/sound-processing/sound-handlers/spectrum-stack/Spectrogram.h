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
#include "LinedImageHelper.h"

namespace rxtd::audio_analyzer {
	class Spectrogram : public SoundHandler {
	public:
		struct Params {
		private:
			friend Spectrogram;

			double resolution { };
			index length { };
			istring sourceName { };
			string prefix = { };
			utils::Color baseColor { };
			utils::Color maxColor { };

			struct ColorDescription {
				float widthInverted;
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
			double colorMinValue { };
			double colorMaxValue { };

			// autogenerated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.resolution == rhs.resolution
					&& lhs.length == rhs.length
					&& lhs.sourceName == rhs.sourceName
					&& lhs.prefix == rhs.prefix
					&& lhs.baseColor == rhs.baseColor
					&& lhs.maxColor == rhs.maxColor
					&& lhs.colorLevels == rhs.colorLevels
					&& lhs.colors == rhs.colors
					&& lhs.colorMinValue == rhs.colorMinValue
					&& lhs.colorMaxValue == rhs.colorMaxValue;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		Params params;

		index samplesPerSec { };

		index blockSize { };

		index counter = 0;
		bool changed = false;

		mutable string propString { };

		utils::LinedImageHelperFixed image { };

		string filepath { };

	public:
		void setParams(const Params& _params, Channel channel);

		static std::optional<Params> parseParams(const utils::OptionMap& optionMap, utils::Rainmeter::Logger &cl, const utils::Rainmeter& rain);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override { }

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override;

		array_view<float> getData(layer_t layer) const override {
			return { };
		}
		layer_t getLayersCount() const override {
			return 0;
		}

		const wchar_t* getProp(const isview& prop) const override;
		bool isStandalone() override {
			return true;
		}

	private:
		void updateParams();
		void fillLine(array_view<float> data);
		void fillLineMulticolor(array_view<float> data);
	};
}
