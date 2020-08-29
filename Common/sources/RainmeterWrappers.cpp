/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "RainmeterWrappers.h"

#include <mutex>


#include "DataWithLock.h"
#include "RainmeterAPI.h"

#pragma comment(lib, "Rainmeter.lib")

using namespace utils;

DWORD WINAPI asyncSender_run(void* param);

class AsyncRainmeterMessageSender {
	enum class MessageType {
		eNONE,
		eLOG,
		eEXECUTE,
		eFINALIZE,
	};

	struct Message {
		MessageType type = MessageType::eNONE;
		string messageText;
		int logLevel = 0;
		void* rainmeterData = nullptr;
	};

	struct MyDataWithLock : DataWithLock {
		MyDataWithLock() : DataWithLock(true) {
		}
	};

	struct HandlerInfo : MyDataWithLock {
		bool invalid = false;
		HMODULE dllHandle = nullptr;
		index counter = 0;
	} handlerInfo;

	struct {
		std::mutex mutex;
		std::condition_variable sleepVariable;
	} threadInfo;

	struct MessageQueue : MyDataWithLock {
		std::vector<Message> queue;
	} messagesStruct;

public:
	AsyncRainmeterMessageSender() = default;

	void init() {
		auto handlerLock = handlerInfo.getLock();

		if (handlerInfo.invalid) {
			return;
		}

		handlerInfo.counter++;

		if (handlerInfo.dllHandle != nullptr) {
			return;
		}

		const auto success = GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
			reinterpret_cast<LPCTSTR>(asyncSender_run),
			&handlerInfo.dllHandle
		);
		if (!success) {
			handlerInfo.invalid = true;
			return;
		}

		CreateThread(nullptr, 0, &asyncSender_run, nullptr, 0, nullptr);
	}

	void deinit() {
		Message mes;
		mes.type = MessageType::eFINALIZE;
		sendMessage(mes);
	}

	void log(void* rm, string text, Rainmeter::Logger::LogLevel level) {
		Message mes;
		mes.type = MessageType::eLOG;
		mes.rainmeterData = rm;
		mes.messageText = std::move(text);
		mes.logLevel = static_cast<int>(level);
		sendMessage(mes);
	}

	void execute(void* skin, string text) {
		Message mes;
		mes.type = MessageType::eEXECUTE;
		mes.rainmeterData = skin;
		mes.messageText = std::move(text);
		sendMessage(mes);
	}

private:

	void sendMessage(Message value) {
		auto lock = messagesStruct.getLock();
		messagesStruct.queue.push_back(std::move(value));

		auto mainLock = std::unique_lock<std::mutex>{ threadInfo.mutex, std::defer_lock };
		const auto locked = mainLock.try_lock();
		if (locked) {
			threadInfo.sleepVariable.notify_one();
		}
	}

	void processMessage(const Message& value) {
		switch (value.type) {
		case MessageType::eNONE: break;
		case MessageType::eLOG:
			RmLog(value.rainmeterData, value.logLevel, value.messageText.c_str());
			break;
		case MessageType::eEXECUTE:
			RmExecute(value.rainmeterData, value.messageText.c_str());
			break;
		case MessageType::eFINALIZE: {
			auto handlerLock = handlerInfo.getLock();
			handlerInfo.counter--;
			break;
		}
		}
	}

	friend DWORD WINAPI asyncSender_run(void* param);
};

AsyncRainmeterMessageSender asyncSender{ };


DWORD WINAPI asyncSender_run(void* param) {
	using namespace std::chrono_literals;

	{
		auto mainLock = std::unique_lock<std::mutex>{ asyncSender.threadInfo.mutex };
		while (true) {
			while (true) {
				std::vector<AsyncRainmeterMessageSender::Message> queue;
				{
					auto lock = asyncSender.messagesStruct.getLock();
					queue = std::exchange(asyncSender.messagesStruct.queue, { });
				}

				if (queue.empty()) {
					break;
				}

				for (auto& mes : queue) {
					asyncSender.processMessage(mes);
				}

				{
					auto handlerLock = asyncSender.handlerInfo.getLock();
					if (asyncSender.handlerInfo.counter == 0) {
						goto label_OUTER_LOOP_END;
					}
				}
			}

			asyncSender.threadInfo.sleepVariable.wait_for(mainLock, 10.0s);
		}
	label_OUTER_LOOP_END:;
	}

	HMODULE handle;
	{
		auto handlerLock = asyncSender.handlerInfo.getLock();
		handle = std::exchange(asyncSender.handlerInfo.dllHandle, nullptr);
		handlerLock.unlock();
	}

	FreeLibraryAndExitThread(handle, 0);
}


void Rainmeter::Logger::logRainmeter(LogLevel logLevel, sview message) const {
	asyncSender.log(rm, message % own(), logLevel);
}

Rainmeter::Rainmeter(void* rm) :
	rm(rm) {
	skin = Skin{ RmGetSkin(rm) };
	measureName = RmGetMeasureName(rm);
}

sview Rainmeter::readString(sview optionName, const wchar_t* defaultValue) const {
	return RmReadString(rm, makeNullTerminated(optionName), defaultValue);
}

sview Rainmeter::readPath(sview optionName, const wchar_t* defaultValue) const {
	return RmReadPath(rm, makeNullTerminated(optionName), defaultValue);
}

double Rainmeter::readDouble(sview optionName, double defaultValue) const {
	return RmReadFormula(rm, makeNullTerminated(optionName), defaultValue);
}

sview Rainmeter::replaceVariables(sview string) const {
	return RmReplaceVariables(rm, makeNullTerminated(string));
}

sview Rainmeter::transformPathToAbsolute(sview path) const {
	return RmPathToAbsolute(rm, makeNullTerminated(path));
}

void Rainmeter::executeCommand(sview command, Skin skin) {
	asyncSender.execute(skin.getRawPointer(), command % own());
}

void* Rainmeter::getWindowHandle() {
	return RmGetSkinWindow(rm);
}

void Rainmeter::sourcelessLog(const wchar_t* message) {
	asyncSender.log(nullptr, message, Logger::LogLevel::eDEBUG);
}

void Rainmeter::incrementLibraryCounter() {
	asyncSender.init();
}

void Rainmeter::decrementLibraryCounter() {
	asyncSender.deinit();
}

const wchar_t* Rainmeter::makeNullTerminated(sview view) const {
	// can't use view[view.length()] because it's out of view's range
	if (view.data()[view.length()] == L'\0') {
		return view.data();
	}

	optionNameBuffer = view;
	return optionNameBuffer.c_str();
}
