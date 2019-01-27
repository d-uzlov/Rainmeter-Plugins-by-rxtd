/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "Channel.h"

namespace rxaa {
	class Format {
	public:
		enum Value {
			INVALID,
			PCM_S16,
			PCM_F32,
		};
	private:
		Value value = INVALID;

	public:
		Format() = default;
		Format(Value value);

		bool operator==(Format a) const;

		bool operator!=(Format a) const;

		const wchar_t* toString() const;
	};

	struct MyWaveFormat {
		index samplesPerSec = 0;
		index channelsCount = 0;
		Format format = Format::INVALID;
		const ChannelLayout* channelLayout = nullptr;


		MyWaveFormat() = default;
		~MyWaveFormat() {
			destroy();
		}

		MyWaveFormat(const MyWaveFormat& other) = default;
		MyWaveFormat& operator=(const MyWaveFormat& other) = default;

		MyWaveFormat(MyWaveFormat&& other) noexcept
			: samplesPerSec(other.samplesPerSec),
			  channelsCount(other.channelsCount),
			  format(other.format),
			  channelLayout(other.channelLayout) {
			other.destroy();
		}
		MyWaveFormat& operator=(MyWaveFormat&& other) noexcept {
			if (this == &other)
				return *this;

			samplesPerSec = other.samplesPerSec;
			channelsCount = other.channelsCount;
			format = std::move(other.format);
			channelLayout = other.channelLayout;

			other.destroy();

			return *this;
		}

	private:
		void destroy() {
			samplesPerSec = 0;
			channelsCount = 0;
			format = Format::INVALID;
			channelLayout = nullptr;
		}
	};

}

