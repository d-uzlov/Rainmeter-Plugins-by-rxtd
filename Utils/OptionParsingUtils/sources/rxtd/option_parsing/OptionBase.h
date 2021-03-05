// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once

namespace rxtd::option_parsing {
	class OptionBase {
	protected:
		using SourceType = std::vector<wchar_t>;

	private:
		// There can be two states of this object:
		// 1. Owning source. Then field source is filled and field view points to it.
		// 2. Non-owning. Then field source is empty, and field view point to some user-managed memory.

		sview view;

		// source is a vector and not a string,
		// because small string optimization kills string views on move
		SourceType source{};

	protected:
		explicit OptionBase() = default;

		OptionBase(sview view) : view(view) {}

		OptionBase(SourceType&& source) : source(std::move(source)) {
			view = makeView();
		}

	public:
		void own() {
			if (isOwningSource()) {
				return;
			}

			source.resize(static_cast<SourceType::size_type>(view.size()));
			std::copy(view.begin(), view.end(), source.begin());
			view = makeView();
		}

		[[nodiscard]]
		bool isOwningSource() const {
			return !source.empty();
		}

	protected:
		[[nodiscard]]
		sview getView() const {
			return view;
		}

		[[nodiscard]]
		SourceType consumeSource() && {
			return std::move(source);
		}

	private:
		[[nodiscard]]
		sview makeView() const {
			return sview{ source.data(), source.size() };
		}
	};
}
