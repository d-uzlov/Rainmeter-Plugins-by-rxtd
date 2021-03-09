// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "rxtd/audio_analyzer/audio_utils/MinMaxCounter.h"
#include "rxtd/audio_analyzer/image_utils/Color.h"
#include "rxtd/audio_analyzer/image_utils/ImageWriteHelper.h"
#include "rxtd/audio_analyzer/image_utils/StripedImage.h"
#include "rxtd/audio_analyzer/image_utils/StripedImageFadeHelper.h"
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/HandlerBase.h"

namespace rxtd::audio_analyzer::handler {
	class Spectrogram : public HandlerBase {
		using Color = image_utils::Color;
		using IntColor = image_utils::IntColor;
		template<typename T>
		using StripedImage = image_utils::StripedImage<T>;
		using ImageWriteHelper = image_utils::ImageWriteHelper;
		using StripedImageFadeHelper = image_utils::StripedImageFadeHelper;

		struct ColorDescription {
			float widthInverted{};
			Color color;

			friend bool operator==(const ColorDescription& lhs, const ColorDescription& rhs) {
				return lhs.widthInverted == rhs.widthInverted
					&& lhs.color == rhs.color;
			}

			friend bool operator!=(const ColorDescription& lhs, const ColorDescription& rhs) {
				return !(lhs == rhs);
			}
		};

		struct Params {
			double resolution{};
			index length{};
			index borderSize{};
			string folder = {};
			Color borderColor{};
			double fading{};

			std::vector<float> colorLevels;
			std::vector<ColorDescription> colors;
			Color::Mode mixMode{};

			bool stationary{};
			float silenceThreshold{};

			//  autogenerated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.resolution == rhs.resolution
					&& lhs.length == rhs.length
					&& lhs.borderSize == rhs.borderSize
					&& lhs.folder == rhs.folder
					&& lhs.borderColor == rhs.borderColor
					&& lhs.fading == rhs.fading
					&& lhs.colorLevels == rhs.colorLevels
					&& lhs.colors == rhs.colors
					&& lhs.mixMode == rhs.mixMode
					&& lhs.stationary == rhs.stationary
					&& lhs.silenceThreshold == rhs.silenceThreshold;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

		struct Snapshot {
			string folder;
			Vector2D<IntColor> pixels;
			index blockSize{};
			uint32_t id = 0;
			bool empty{};

			mutable ImageWriteHelper writerHelper{};
			mutable bool writeNeeded{};
		};

		class InputStripMaker {
			index counter{};
			index blockSize{};
			index chunkEquivalentWaveSize{};
			array_view<ColorDescription> colors;
			array_view<float> colorLevels;

			array_view<array_view<float>> chunks;
			std::vector<IntColor> buffer{};

		public:
			void setParams(
				index _blockSize, index _chunkEquivalentWaveSize, index bufferSize,
				array_view<ColorDescription> _colors, array_view<float> _colorLevels
			);

			[[nodiscard]]
			array_view<IntColor> getBuffer() const {
				return buffer;
			}

			void setChunks(array_view<array_view<float>> value) {
				chunks = value;
			}

			void next();

		private:
			void fillStrip(array_view<float> data, array_span<IntColor> buffer) const;
			void fillStripMulticolor(array_view<float> data, array_span<IntColor> buffer) const;
		};

		Params params;

		audio_utils::MinMaxCounter minMaxCounter;
		uint32_t snapshotId = 0;

		StripedImage<IntColor> image{};
		StripedImageFadeHelper fadeHelper{};
		InputStripMaker inputStripMaker;

	public:
		[[nodiscard]]
		bool vCheckSameParams(const ParamsContainer& p) const override {
			return compareParamsEquals(params, p);
		}

		[[nodiscard]]
		index vGetSourcesCount() const override {
			return 1;
		}

		[[nodiscard]]
		ParamsContainer vParseParams(ParamParseContext& context) const noexcept(false) override;

	protected:
		[[nodiscard]]
		ConfigurationResult vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) override;

	private:
		static std::pair<std::vector<ColorDescription>, std::vector<float>>
		parseColors(const OptionList& list, Color::Mode defaultColorSpace, option_parsing::OptionParser& parser, Logger& cl);

	public:
		void vProcess(ProcessContext context, ExternalData& externalData) override;

	protected:
		ExternalMethods::GetPropMethodType vGetExt_getProp() const override {
			return wrapExternalGetProp<Snapshot, &getProp>();
		}

		ExternalMethods::FinishMethodType vGetExt_finish() const override {
			return wrapExternalFinish<Snapshot, &staticFinisher>();
		}

	private:
		void updateSnapshot(Snapshot& snapshot);

		static void staticFinisher(const Snapshot& snapshot, const ExternalMethods::CallContext& context);

		static bool getProp(
			const Snapshot& snapshot,
			isview prop,
			const ExternalMethods::CallContext& context
		);
	};
}
