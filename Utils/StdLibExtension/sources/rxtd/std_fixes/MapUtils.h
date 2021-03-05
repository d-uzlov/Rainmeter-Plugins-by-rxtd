// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once

namespace rxtd::std_fixes {
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
