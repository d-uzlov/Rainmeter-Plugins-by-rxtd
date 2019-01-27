/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../Channel.h"
#include "StringUtils.h"

namespace rxaa {
	class SoundHandler;

	class DataSupplier {
	public:
		virtual ~DataSupplier() = default;
		virtual const float* getWave() const = 0;
		virtual index getWaveSize()  const = 0;
		virtual const SoundHandler* getHandler(isview id) const = 0;
		virtual Channel getChannel() const = 0;

		/**
		 * returns array of size @code size.
		 * Can be called several times, each time buffer will be different.
		 * Released automatically after end of process() and processSilent().
		 */
		template<typename T>
		T* getBuffer(size_t size) const {
			return reinterpret_cast<T*>(getBufferRaw(size * sizeof(T)));
		}

	protected:
		virtual uint8_t* getBufferRaw(index size) const = 0;
	};

	class SoundHandler {
	public:
		virtual ~SoundHandler() = default;
		virtual void process(const DataSupplier &dataSupplier) = 0;
		virtual void processSilence(const DataSupplier &dataSupplier) = 0;
		virtual const double* getData() const = 0;
		virtual index getCount() const = 0;
		virtual void setSamplesPerSec(index samplesPerSec) = 0;
		virtual const wchar_t* getProp(const isview& prop) {
			return nullptr;
		}
		virtual void reset() = 0;

	protected:
		static double calculateAttackDecayConstant(double time, index samplesPerSec, index stride) {
			return exp(-2.0 * stride / (samplesPerSec * time));
		}

		static double calculateAttackDecayConstant(double time, index samplesPerSec) {
			return calculateAttackDecayConstant(time, samplesPerSec, 1u);
		}

		static int parseIndexProp(const isview& request, const isview& propName, index endBound) {
			return parseIndexProp(request, propName, 0, endBound);
		}

		static int parseIndexProp(const isview& request, const isview& propName, index minBound, index endBound) {
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
