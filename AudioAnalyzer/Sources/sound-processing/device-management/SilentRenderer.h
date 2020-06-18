/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <mmdeviceapi.h>
#include <Audioclient.h>

namespace rxtd::audio_analyzer {
	/** AudioLevel uses silent renderer to prevent stuttering
	  * Details of the issue can be found here:
	  * http://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/c7ba0a04-46ce-43ff-ad15-ce8932c00171/loopback-recording-causes-digital-stuttering?forum=windowspro-audiodevelopment
	  * I temporary disable this as I don't seem to have this problem.
	  */
	class SilentRenderer {
		IAudioClient *audioClient = nullptr;
		IAudioRenderClient *renderer = nullptr;

	public:
		SilentRenderer() = default;
		SilentRenderer(IMMDevice *audioDeviceHandle);

		SilentRenderer(SilentRenderer&& other) noexcept;
		SilentRenderer& operator=(SilentRenderer&& other) noexcept;

		SilentRenderer(const SilentRenderer& other) = delete;
		SilentRenderer& operator=(const SilentRenderer& other) = delete;

		~SilentRenderer();

		bool isError() const;

	private:
		void releaseHandles();
	};
}

