/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "Channel.h"
#include <variant>
#include "HelperClasses.h"

namespace rxtd::audio_analyzer {
	class AudioChildHelper {
	public:
		enum class SearchError {
			eCHANNEL_NOT_FOUND,
			eHANDLER_NOT_FOUND,
		};

	private:
		std::map<Channel, ChannelData>* channels;
		DataSupplierImpl* dataSupplier;

	public:
		explicit AudioChildHelper(std::map<Channel, ChannelData>& channels, DataSupplierImpl& dataSupplier);

		std::variant<SoundHandler*, SearchError> findHandler(Channel channel, isview handlerId) const;
		double getValue(Channel channel, isview handlerId, index index) const;
		std::variant<const wchar_t*, SearchError> getProp(Channel channel, isview handlerId, isview prop) const;
	};
}
