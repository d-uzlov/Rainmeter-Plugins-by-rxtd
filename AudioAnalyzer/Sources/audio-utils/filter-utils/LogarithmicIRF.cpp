/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "LogarithmicIRF.h"

#include "undef.h"

using namespace audio_utils;

void LogarithmicIRF::setParams(double attackTime, double decayTime, index samplesPerSec, index stride) {
	attackDecayConstants[0] = calculateAttackDecayConstant(attackTime, samplesPerSec, stride);
	attackDecayConstants[1] = calculateAttackDecayConstant(decayTime, samplesPerSec, stride);
}

void LogarithmicIRF::setParams(double attackTime, double decayTime, index samplesPerSec) {
	setParams(attackTime, decayTime, samplesPerSec, 1);
}

float LogarithmicIRF::next(float value) {
	result = value + attackDecayConstants[(value < result)] * (result - value);
	return result;
}

float LogarithmicIRF::apply(float prev, float value) {
	return value + attackDecayConstants[(value < prev)] * (prev - value);
}

float LogarithmicIRF::getLastResult() const {
	return result;
}

void LogarithmicIRF::reset() {
	result = 0.0;
}

float LogarithmicIRF::calculateAttackDecayConstant(float time, index samplesPerSec, index stride) {
	// stride and samplesPerSec are semantically guaranteed to be positive
	// time can be positive or zero
	// In case of zero result is exp(-inf) == 0 which is totally fine
	return std::exp(-2.0f * stride / (samplesPerSec * time));
}
