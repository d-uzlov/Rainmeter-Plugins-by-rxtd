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

double LogarithmicIRF::next(double value) {
	result = value + attackDecayConstants[(value < result)] * (result - value);
	return result;
}

double LogarithmicIRF::apply(double prev, double value) {
	return value + attackDecayConstants[(value < prev)] * (prev - value);
}

const double& LogarithmicIRF::getLastResult() const {
	return result;
}

void LogarithmicIRF::reset() {
	result = 0.0;
}

double LogarithmicIRF::calculateAttackDecayConstant(double time, index samplesPerSec, index stride) {
	return std::exp(-2.0 * stride / (samplesPerSec * time));
}
