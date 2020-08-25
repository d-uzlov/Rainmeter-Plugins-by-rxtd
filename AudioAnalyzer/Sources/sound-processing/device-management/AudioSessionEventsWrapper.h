/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "windows-wrappers/implementations/AudioSessionEventsImpl.h"
#include "windows-wrappers/IAudioClientWrapper.h"

namespace rxtd::audio_analyzer {
	// the whole and only point of this class is to
	// automatically call AudioSessionEventsImpl#deinit
	class AudioSessionEventsWrapper {
		utils::GenericComWrapper<utils::AudioSessionEventsImpl> impl;

	public:
		AudioSessionEventsWrapper() = default;

		~AudioSessionEventsWrapper() {
			destruct();
		}

		AudioSessionEventsWrapper(const AudioSessionEventsWrapper& other) = delete;
		AudioSessionEventsWrapper& operator=(const AudioSessionEventsWrapper& other) = delete;

		AudioSessionEventsWrapper(AudioSessionEventsWrapper&& other) noexcept {
			destruct();
			impl = std::move(other.impl);
		}

		AudioSessionEventsWrapper& operator=(AudioSessionEventsWrapper&& other) noexcept {
			if (this == &other) {
				return *this;
			}

			destruct();
			impl = std::move(other.impl);

			return *this;
		}

		void init(utils::IAudioClientWrapper& client) {
			destruct();
			impl = {
				[&](auto ptr) {
					*ptr = new utils::AudioSessionEventsImpl{ client };
					return true;
				}
			};
		}

	private:
		void destruct() {
			if (impl.isValid()) {
				impl.getPointer()->deinit();
			}
			impl = { };
		}
	};
}
