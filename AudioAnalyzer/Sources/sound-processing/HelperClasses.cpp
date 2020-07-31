/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "HelperClasses.h"

using namespace audio_analyzer;

SoundHandler* HandlerFinderImpl::getHandlerRaw(isview id) const {
	const auto iter = channelData->find(id);
	if (iter == channelData->end()) {
		return nullptr;
	}

	const auto handler = iter->second.ptr.get();
	if (!handler->isValid()) {
		return nullptr;
	}

	return handler;
}
