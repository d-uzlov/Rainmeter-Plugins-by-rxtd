// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "rxtd/audio_analyzer/audio_utils/CustomizableValueTransformer.h"
#include "rxtd/audio_analyzer/audio_utils/MinMaxCounter.h"
#include "rxtd/audio_analyzer/image_utils/ImageWriteHelper.h"
#include "rxtd/audio_analyzer/image_utils/WaveFormDrawer.h"
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/HandlerBase.h"

namespace rxtd::audio_analyzer::handler {
	class WaveForm : public HandlerBase {
		using WaveFormDrawer = image_utils::WaveFormDrawer;
		using LDP = WaveFormDrawer::LineDrawingPolicy;
		using Colors = WaveFormDrawer::Colors;
		using CVT = audio_utils::CustomizableValueTransformer;
		using IntColor = image_utils::IntColor;
		using ImageWriteHelper = image_utils::ImageWriteHelper;

		struct Params {
			double resolution{};
			index width{};
			index height{};
			string folder;
			Colors colors{};
			LDP lineDrawingPolicy{};
			index lineThickness{};
			bool stationary{};
			bool connected{};
			index borderSize{};
			double fading{};
			CVT transformer;
			float silenceThreshold{};

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
			string folder;
			Vector2D<IntColor> pixels;
			index blockSize{};
			uint32_t id;
			bool empty{};

			mutable ImageWriteHelper writerHelper{};
			mutable bool writeNeeded{};
		};

		Params params;

		audio_utils::MinMaxCounter mainCounter;
		audio_utils::MinMaxCounter originalCounter;
		uint32_t snapshotId;

		double minDistinguishableValue{};

		WaveFormDrawer drawer{};

	public:
		[[nodiscard]]
		bool vCheckSameParams(const ParamsContainer& p) const override {
			return compareParamsEquals(params, p);
		}

		[[nodiscard]]
		ParamsContainer vParseParams(ParamParseContext& context) const noexcept(false) override;

	protected:
		[[nodiscard]]
		ConfigurationResult vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) override;

		void vProcess(ProcessContext context, ExternalData& externalData) override;

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
			BufferPrinter& printer,
			const ExternalMethods::CallContext& context
		);
	};
}
