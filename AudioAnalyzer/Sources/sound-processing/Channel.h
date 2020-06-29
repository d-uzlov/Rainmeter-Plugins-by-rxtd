/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace rxtd::audio_analyzer {
	class Channel {
	public:
		class ChannelParser {
			std::map<istring, Channel, std::less<>> map { };

		public:
			ChannelParser();

			std::optional<Channel> find(sview string);

		private:
			void addElement(isview name, Channel value);
		};

		static ChannelParser channelParser;

		enum Value {
			eFRONT_LEFT,
			eFRONT_RIGHT,
			eCENTER,
			eCENTER_BACK,
			eLOW_FREQUENCY,
			eBACK_LEFT,
			eBACK_RIGHT,
			eSIDE_LEFT,
			eSIDE_RIGHT,
			eAUTO,
		};

		using underlying_type = std::underlying_type<Value>::type;

	private:
		Value value = eFRONT_LEFT;

	public:
		Channel() = default;
		Channel(Value value);
		underlying_type toUnderlyingType() const {
			return static_cast<underlying_type>(value);
		}

		bool operator==(Channel a) const;

		bool operator!=(Channel a) const;

		sview technicalName() const;

	private:
		friend bool operator <(Channel left, Channel right);
	};
	/**
	 * For use in std::map
	 */
	bool operator <(Channel left, Channel right);
}

namespace std {
	template<>
	struct hash<rxtd::audio_analyzer::Channel> {
		using Channel = audio_analyzer::Channel;
		using hash_type = audio_analyzer::Channel::underlying_type;

		size_t operator()(const Channel &c) const {
			return std::hash<hash_type>()(c.toUnderlyingType());
		}
	};
}

namespace rxtd::audio_analyzer {
	class LayoutBuilder;

	class ChannelLayout {
		friend LayoutBuilder;
	
		sview name = { };
		std::unordered_map<Channel, index> channelMap;
	
	public:
	
		sview getName() const;
		std::optional<index> indexOf(Channel channel) const;
		bool contains(Channel channel) const;

		static ChannelLayout create(sview name, const std::vector<Channel::Value>& channels);
	
		class const_iterator {
			decltype(channelMap)::const_iterator iter;
	
		public:
			explicit const_iterator(const decltype(channelMap)::const_iterator& iter) :
				iter(iter) { }
	
			const_iterator& operator++() {
				++iter;
				return *this;
			}
			Channel operator*() const {
				return (*iter).first;
			}
			bool operator!=(const const_iterator& other) const {
				return iter != other.iter;
			}
		};
		const_iterator begin() const {
			return const_iterator { channelMap.cbegin() };
		}
		const_iterator end() const {
			return const_iterator { channelMap.cend() };
		}

		const std::unordered_map<Channel, index>& getChannelsMapView() const;
	};

	class LayoutBuilder {
		index nextIndex = 0;
		ChannelLayout layout { };

	public:
		LayoutBuilder& add(Channel channel);
		LayoutBuilder& skip() {
			nextIndex++;

			return *this;
		}
		ChannelLayout finish() const;
	};

	class ChannelLayouts {
	public:
		static ChannelLayout getMono();
		static ChannelLayout getStereo();
		static ChannelLayout layoutFromChannelMask(uint32_t mask, bool forceBackSpeakers);
	};
}


