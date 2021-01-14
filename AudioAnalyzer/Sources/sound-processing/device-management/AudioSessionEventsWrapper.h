/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "wasapi-wrappers/implementations/AudioSessionEventsImpl.h"
#include "wasapi-wrappers/IAudioClientWrapper.h"

namespace rxtd::audio_analyzer {
	// the whole and only point of this class is to
	// automatically call AudioSessionEventsImpl#deinit
	class AudioSessionEventsWrapper : MovableOnlyBase {
		utils::GenericComWrapper<utils::AudioSessionEventsImpl> impl;

	public:
		AudioSessionEventsWrapper() = default;

		AudioSessionEventsWrapper(utils::IAudioClientWrapper& client) {
			impl = {
				[&](auto ptr) {
					*ptr = utils::AudioSessionEventsImpl::create(client);
					return true;
				}
			};
		}

		AudioSessionEventsWrapper(utils::GenericComWrapper<IAudioSessionControl>&& _sessionController) {
			impl = {
				[&](auto ptr) {
					*ptr = utils::AudioSessionEventsImpl::create(std::move(_sessionController));
					return true;
				}
			};
		}

		~AudioSessionEventsWrapper() {
			destruct();
		}

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

		[[nodiscard]]
		bool isValid() const {
			return impl.isValid();
		}

		utils::AudioSessionEventsImpl::Changes grabChanges() {
			if (!impl.isValid()) {
				return {};
			}
			return impl.ref().takeChanges();
		}

		void destruct() {
			if (impl.isValid()) {
				impl.ref().deinit();
			}
			impl = {};
		}
	};
}
