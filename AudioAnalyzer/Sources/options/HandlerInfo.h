/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "sound-processing/sound-handlers/HandlerBase.h"

namespace rxtd::audio_analyzer {
	struct HandlerInfo {
		string rawDescription;
		string rawDescription2;
		istring type;
		std::vector<istring> sources;
		handler::HandlerBase::HandlerMetaInfo meta;

		friend bool operator==(const HandlerInfo& lhs, const HandlerInfo& rhs) {
			return lhs.rawDescription == rhs.rawDescription
				&& lhs.rawDescription2 == rhs.rawDescription2;
		}

		friend bool operator!=(const HandlerInfo& lhs, const HandlerInfo& rhs) { return !(lhs == rhs); }
	};
}
