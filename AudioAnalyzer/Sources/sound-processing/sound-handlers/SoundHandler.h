/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../DataSupplier.h"
#include "StringUtils.h"
#include "array_view.h"

namespace rxtd::audio_analyzer {
	class SoundHandler {
	public:
		using layer_t = int8_t;

		SoundHandler() = default;

		SoundHandler(const SoundHandler& other) = delete;
		SoundHandler(SoundHandler&& other) noexcept = delete;
		SoundHandler& operator=(const SoundHandler& other) = delete;
		SoundHandler& operator=(SoundHandler&& other) noexcept = delete;

		virtual ~SoundHandler() = default;

		virtual void setSamplesPerSec(index samplesPerSec) = 0;
		virtual void reset() = 0;

		virtual void process(const DataSupplier &dataSupplier) = 0;
		virtual void processSilence(const DataSupplier &dataSupplier) = 0;

		// Method can be called several times in a row, handler should check for changes to optimize performance
		virtual void finish(const DataSupplier &dataSupplier) = 0;

		virtual bool isValid() const = 0;
		virtual array_view<float> getData(layer_t layer) const = 0;
		virtual layer_t getLayersCount() const = 0;
		virtual layer_t getStartingLayer() const {
			return 0;
		}

		virtual const wchar_t* getProp(const isview& prop) const {
			return nullptr;
		}
		virtual bool isStandalone() {
			return false;
		}

	protected:
		static double calculateAttackDecayConstant(double time, index samplesPerSec, index stride) {
			return std::exp(-2.0 * stride / (samplesPerSec * time));
		}

		static double calculateAttackDecayConstant(double time, index samplesPerSec) {
			return calculateAttackDecayConstant(time, samplesPerSec, 1);
		}

		static index parseIndexProp(const isview& request, const isview& propName, index endBound) {
			return parseIndexProp(request, propName, 0, endBound);
		}

		static index parseIndexProp(const isview& request, const isview& propName, index minBound, index endBound) {
			const auto indexPos = request.find(propName);

			if (indexPos != 0) {
				return -1;
			}

			const auto indexView = request.substr(propName.length());
			if (indexView.length() == 0) {
				return 0;
			}

			const auto cascadeIndex = utils::StringUtils::parseInt(indexView % csView());

			if (cascadeIndex < minBound || cascadeIndex >= endBound) {
				return -2;
			}

			return cascadeIndex;
		}
	};
}
