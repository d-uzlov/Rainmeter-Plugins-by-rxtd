// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "rxtd/audio_analyzer/sound_processing/sound_handlers/HandlerBase.h"

namespace rxtd::audio_analyzer::options {
	struct HandlerInfo {
		string rawDescription;
		istring type;
		istring source;
		handler::HandlerBase::HandlerMetaInfo meta;

		friend bool operator==(const HandlerInfo& lhs, const HandlerInfo& rhs) {
			return lhs.rawDescription == rhs.rawDescription
				&& lhs.source == rhs.source;
		}

		friend bool operator!=(const HandlerInfo& lhs, const HandlerInfo& rhs) { return !(lhs == rhs); }
	};
}
