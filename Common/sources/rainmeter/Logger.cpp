/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Logger.h"

using namespace ::rxtd::common::rainmeter;


void Logger::logRainmeter(LogLevel logLevel, sview message) const {
	asyncSender.log(rm, message % own(), logLevel);
}
