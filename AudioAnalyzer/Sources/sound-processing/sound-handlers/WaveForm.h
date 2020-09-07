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
#include "audio-utils/MinMaxCounter.h"

namespace rxtd::audio_analyzer {
	class WaveForm : public SoundHandler {
		using LDP = utils::WaveFormDrawer::LineDrawingPolicy;
		using Colors = utils::WaveFormDrawer::Colors;
		using CVT = audio_utils::CustomizableValueTransformer;

		struct Params {
			double resolution{ };
			index width{ };
			index height{ };
			string folder;
			Colors colors{ };
			LDP lineDrawingPolicy{ };
			index lineThickness{ };
			bool stationary{ };
			bool connected{ };
			index borderSize{ };
			double fading{ };
			CVT transformer;
			float silenceThreshold{ };

			// generated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.resolution == rhs.resolution
					&& lhs.width == rhs.width
					&& lhs.height == rhs.height
					&& lhs.folder == rhs.folder
					&& lhs.colors == rhs.colors
					&& lhs.lineDrawingPolicy == rhs.lineDrawingPolicy
					&& lhs.lineThickness == rhs.lineThickness
					&& lhs.stationary == rhs.stationary
					&& lhs.connected == rhs.connected
					&& lhs.borderSize == rhs.borderSize
					&& lhs.fading == rhs.fading
					&& lhs.transformer == rhs.transformer
					&& lhs.silenceThreshold == rhs.silenceThreshold;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

		struct Snapshot {
			index blockSize{ };

			string prefix;

			utils::Vector2D<utils::IntColor> pixels;
			bool empty{ };

			mutable utils::ImageWriteHelper writerHelper{ };
			mutable bool writeNeeded{ };

			mutable string filenameBuffer;
		};

		Params params;

		audio_utils::MinMaxCounter mainCounter;
		audio_utils::MinMaxCounter originalCounter;

		double minDistinguishableValue{ };

		utils::WaveFormDrawer drawer{ };

	public:
		[[nodiscard]]
		bool checkSameParams(const ParamsContainer& p) const override {
			return compareParamsEquals(params, p);
		}

		[[nodiscard]]
		ParseResult parseParams(
			const OptionMap& om, Logger& cl, const Rainmeter& rain,
			index legacyNumber
		) const override;

	protected:
		[[nodiscard]]
		ConfigurationResult vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) override;

		void vProcess(ProcessContext context, ExternalData& externalData) override;

	private:
		static void staticFinisher(const Snapshot& snapshot, const ExternCallContext& context);

		static bool getProp(
			const Snapshot& snapshot,
			isview prop,
			utils::BufferPrinter& printer,
			const ExternCallContext& context
		);
	};
}
