// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/HandlerBase.h"

namespace rxtd::audio_analyzer::options {
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
