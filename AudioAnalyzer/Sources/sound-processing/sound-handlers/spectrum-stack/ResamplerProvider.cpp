/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ResamplerProvider.h"
#include "BandResampler.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

void ResamplerProvider::_process(const DataSupplier& dataSupplier) {
	const auto source = dataSupplier.getHandler(resamplerId);
	if (source == nullptr) {
		setValid(false);
		return;
	}
	const auto tryResampler = dynamic_cast<const BandResampler*>(source);
	if (tryResampler != nullptr) {
		resampler = tryResampler;
		return;
	}

	const auto provider = dynamic_cast<const ResamplerProvider*>(source);
	if (provider == nullptr) {
		setValid(false);
		return;
	}

	resampler = provider->getResampler();
}
