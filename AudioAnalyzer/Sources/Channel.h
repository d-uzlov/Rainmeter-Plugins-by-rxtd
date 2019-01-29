/*
 * Copyright (C) 2019 rxtd
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
			FRONT_LEFT = 0,
			FRONT_RIGHT = 1,
			CENTER = 2,
			LOW_FREQUENCY = 3,
			BACK_LEFT = 4,
			BACK_RIGHT = 5,
			SIDE_LEFT = 6,
			SIDE_RIGHT = 7,
			AUTO,
		};

	private:
		Value value = FRONT_LEFT;

	public:
		Channel() = default;
		Channel(Value value);

		bool operator==(Channel a) const;

		bool operator!=(Channel a) const;

		index toInt() const;
		const wchar_t* technicalName() const;

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
		size_t operator()(const rxtd::audio_analyzer::Channel &c) const {
			return std::hash<decltype(c.toInt())>()(c.toInt());
		}
	};
}

namespace rxtd::audio_analyzer {
	class ChannelLayout {
		string name;
		std::unordered_set<Channel> channels;
		std::unordered_map<Channel, index> forward;

		ChannelLayout() = default;

	public:

		const string& getName() const;
		std::optional<index> fromChannel(Channel channel) const;
		const std::unordered_set<Channel> & channelsView() const;

		template<Channel::Value... channels>
		static ChannelLayout create(string name);
	private:
		template<index N, Channel::Value nextChannel, Channel::Value... otherChannels>
		typename std::enable_if<sizeof...(otherChannels) != 0, void>::type
			insert();
		template<index N, Channel::Value nextChannel>
		void insert();
	};

	template <Channel::Value... channels>
	ChannelLayout ChannelLayout::create(string name) {
		ChannelLayout result;
		result.name = name;
		result.insert<0, channels...>();

		return result;
	}

	template <index N, Channel::Value nextChannel, Channel::Value... otherChannels>
	typename std::enable_if<sizeof...(otherChannels) != 0, void>::type
		ChannelLayout::insert() {
		channels.insert(nextChannel);
		forward[nextChannel] = N;

		insert<N + 1, otherChannels...>();
	}

	template <index N, Channel::Value lastChannel>
	void ChannelLayout::insert() {
		channels.insert(lastChannel);
		forward[lastChannel] = N;
	}

	class ChannelLayoutKeeper {
		ChannelLayout mono = ChannelLayout::create<
			Channel::CENTER
		>(L"1.0 mono");
		ChannelLayout _1_1 = ChannelLayout::create<
			Channel::CENTER, Channel::LOW_FREQUENCY
		>(L"1.1");
		ChannelLayout stereo = ChannelLayout::create<
			Channel::FRONT_LEFT, Channel::FRONT_RIGHT
		>(L"2.0 stereo");
		ChannelLayout _2_1 = ChannelLayout::create<
			Channel::FRONT_LEFT, Channel::FRONT_RIGHT,
			Channel::LOW_FREQUENCY
		>(L"2.1");
		ChannelLayout _3_0 = ChannelLayout::create<
			Channel::FRONT_LEFT, Channel::FRONT_RIGHT,
			Channel::CENTER
		>(L"3.0");
		ChannelLayout _3_1 = ChannelLayout::create<
			Channel::FRONT_LEFT, Channel::FRONT_RIGHT,
			Channel::CENTER, Channel::LOW_FREQUENCY
		>(L"3.1");
		ChannelLayout quad = ChannelLayout::create<
			Channel::FRONT_LEFT, Channel::FRONT_RIGHT,
			Channel::BACK_LEFT, Channel::BACK_RIGHT
		>(L"4.0 quad");
		ChannelLayout _5_0 = ChannelLayout::create<
			Channel::FRONT_LEFT, Channel::FRONT_RIGHT,
			Channel::CENTER,
			Channel::SIDE_LEFT, Channel::SIDE_RIGHT
		>(L"5.0");
		ChannelLayout _5_1 = ChannelLayout::create<
			Channel::FRONT_LEFT, Channel::FRONT_RIGHT,
			Channel::CENTER, Channel::LOW_FREQUENCY,
			Channel::BACK_LEFT, Channel::BACK_RIGHT
		>(L"5.1");
		ChannelLayout _7_0 = ChannelLayout::create<
			Channel::FRONT_LEFT, Channel::FRONT_RIGHT,
			Channel::CENTER,
			Channel::BACK_LEFT, Channel::BACK_RIGHT,
			Channel::SIDE_LEFT, Channel::SIDE_RIGHT
		>(L"7.0");
		ChannelLayout _7_1surround = ChannelLayout::create<
			Channel::FRONT_LEFT, Channel::FRONT_RIGHT,
			Channel::CENTER, Channel::LOW_FREQUENCY,
			Channel::BACK_LEFT, Channel::BACK_RIGHT,
			Channel::SIDE_LEFT, Channel::SIDE_RIGHT
		>(L"7.1 surround");

	public:
		ChannelLayoutKeeper() = default;

		const ChannelLayout* getMono() const;
		const ChannelLayout* getStereo() const;
		const ChannelLayout* layoutFromChannelMask(uint32_t mask) const;
	};

	extern ChannelLayoutKeeper layoutKeeper;
}


