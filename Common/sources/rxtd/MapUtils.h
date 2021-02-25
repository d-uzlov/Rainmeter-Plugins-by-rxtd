/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::utils {
	class MapUtils {
	public:
		// remove elements that are not present in collection
		// collection must have #find() and #end methods
		template<typename Map, typename Collection>
		static void intersectKeyCollection(Map& map, const Collection& collection) {
			for (auto iter = map.begin();
			     iter != map.end();) {
				if (collection.find(iter->first) == collection.end()) {
					iter = map.erase(iter);
				} else {
					++iter;
				}
			}
		}
		
		// remove element if predicate returns true
		// predicate must be a callable that accepts map::key_type and returns bool
		template<typename Map, typename Predicate>
		static void removeKeysByPredicate(Map& map, const Predicate& predicate) {
			for (auto iter = map.begin();
			     iter != map.end();) {
				if (predicate(iter->first)) {
					iter = map.erase(iter);
				} else {
					++iter;
				}
			}
		}
	};
}
