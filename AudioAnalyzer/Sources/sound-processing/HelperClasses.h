/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "sound-handlers/SoundHandler.h"
#include "RainmeterWrappers.h"

namespace rxtd::audio_analyzer {
	struct HandlerValidityInfo {
		std::unique_ptr<SoundHandler> ptr;
		bool wasValid;
	};

	using ChannelData = std::map<istring, HandlerValidityInfo, std::less<>>;

	class DataSupplierImpl : public DataSupplier {
		array_view<float> wave{ };

	public:
		mutable utils::Rainmeter::Logger logger;

		void setWave(array_view<float> value) {
			wave = value;
		}

		array_view<float> getWave() const override {
			return wave;
		}

		void _log(const wchar_t* message) const override {
			logger.error(message);
		}
	};

	class HandlerFinderImpl : public HandlerFinder {
		const ChannelData* channelData = nullptr;

	public:
		void setChannelData(const ChannelData& value) {
			channelData = &value;
		}

	protected:
		[[nodiscard]]
		SoundHandler* getHandlerRaw(isview id) const override;
	};
}
