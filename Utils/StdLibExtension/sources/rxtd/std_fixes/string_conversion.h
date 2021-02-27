/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::std_fixes::string_conversion {
	namespace details {
		struct caseInsensitiveObject { };

		struct caseSensitiveObject { };

		struct caseInsensitiveViewObject { };

		struct caseSensitiveViewObject { };

		struct stringCreationObject { };
	}

	static struct {
		[[nodiscard]]
		details::caseInsensitiveObject operator()() const {
			return {};
		}
	} ciString{};

	[[nodiscard]]
	istring operator%(string&& str, details::caseInsensitiveObject);

	static struct {
		[[nodiscard]]
		details::caseSensitiveObject operator()() const {
			return {};
		}
	} csString{};

	[[nodiscard]]
	string operator%(istring&& str, details::caseSensitiveObject);


	static struct {
		[[nodiscard]]
		details::caseInsensitiveViewObject operator()() const {
			return {};
		}
	} ciView{};

	[[nodiscard]]
	inline isview operator%(sview view, details::caseInsensitiveViewObject) {
		return { view.data(), view.length() };
	}

	static struct {
		[[nodiscard]]
		details::caseSensitiveViewObject operator()() const {
			return {};
		}
	} csView{};

	[[nodiscard]]
	sview operator%(isview view, details::caseSensitiveViewObject);


	static struct {
		[[nodiscard]]
		details::stringCreationObject operator()() const {
			return {};
		}
	} own{};

	[[nodiscard]]
	string operator%(sview view, details::stringCreationObject);

	[[nodiscard]]
	istring operator%(isview view, details::stringCreationObject);
}
