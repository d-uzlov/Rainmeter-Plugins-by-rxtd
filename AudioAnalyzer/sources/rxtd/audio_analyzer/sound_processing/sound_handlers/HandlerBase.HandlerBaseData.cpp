/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "HandlerBase.h"

using rxtd::audio_analyzer::handler::HandlerBase;

void HandlerBase::HandlerBaseData::inflateLayers() const {
	if (layers.valid) {
		return;
	}

	for (auto& data : layers.inflatableCache) {
		data.chunksView.resize(data.offsets.size());
		for (index i = 0; i < static_cast<index>(data.offsets.size()); i++) {
			data.chunksView[static_cast<size_t>(i)] = { buffer.data() + data.offsets[static_cast<size_t>(i)], size.valuesCount };
		}
	}

	layers.valid = true;
}

void HandlerBase::HandlerBaseData::clearChunks() {
	for (index layer = 0; layer < size.layersCount; layer++) {
		auto& offsets = layers.inflatableCache[static_cast<size_t>(layer)].offsets;
		if (!offsets.empty()) {
			lastResults[layer].copyFrom({ buffer.data() + offsets.back(), size.valuesCount });
		}
		offsets.clear();
	}

	layers.valid = false;

	buffer.clear();
}

array_span<float> HandlerBase::HandlerBaseData::pushLayer(index layer) {
	const index offset = static_cast<index>(buffer.size());

	// Prevent handlers from producing too much data
	// see: https://github.com/d-uzlov/Rainmeter-Plugins-by-rxtd/issues/4
	if (offset + size.valuesCount > 1'000'000) {
		throw TooManyValuesException{ name };
	}

	buffer.resize(static_cast<size_t>(offset + size.valuesCount));
	layers.valid = false;

	layers.inflatableCache[static_cast<size_t>(layer)].offsets.push_back(offset);

	return { buffer.data() + offset, size.valuesCount };
}
