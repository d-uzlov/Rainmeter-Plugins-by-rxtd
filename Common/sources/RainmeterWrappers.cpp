/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "RainmeterWrappers.h"


#include <atomic>
#include <mutex>


#include "DataWithLock.h"
#include "RainmeterAPI.h"

#pragma comment(lib, "Rainmeter.lib")

using namespace utils;

enum class MessageType {
	eNONE,
	eLOG,
	eEXECUTE,
	eKILL,
};

struct Message {
	MessageType type = MessageType::eNONE;
	string messageText;
	int logLevel = 0;
	void* rainmeterData = nullptr;
};

struct MessageQueue : DataWithLock {
	std::vector<Message> buffer;
	std::condition_variable sleepVariable;

	MessageQueue() : DataWithLock(true) { }
};

struct ThreadArguments {
	HMODULE dllHandle = nullptr;
	MessageQueue* queue = nullptr;
};

DWORD WINAPI asyncSender_run(void* param);

class AsyncRainmeterMessageSender {

	// only main rainmeter UI thread should access this variable
	bool initialized = false;

	std::atomic<int32_t> counter{ 0 };

	MessageQueue queue;

public:
	AsyncRainmeterMessageSender() = default;

	void init(void* rm) {
		counter.fetch_add(1);

		if (initialized) {
			// regardless of if it was success or failure, we only try to initialize once
			return;
		}

		initialized = true;

		HMODULE dllHandle = nullptr;
		const auto handlerReadSuccess = GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
			reinterpret_cast<LPCTSTR>(asyncSender_run),
			&dllHandle
		);
		if (!handlerReadSuccess) {
			const auto errorCode = GetLastError();
			BufferPrinter bp;
			bp.print(L"Log and bangs are not available, GetModuleHandleExW failed (error {})", errorCode);
			RmLog(rm, LOG_ERROR, bp.getBufferPtr());

			wchar_t* receiveBuffer = nullptr;
			const auto messageLength = FormatMessageW(
				FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
				nullptr,
				errorCode,
				0,
				reinterpret_cast<LPWSTR>(&receiveBuffer),
				0,
				nullptr
			);
			if (messageLength != 0) {
				bp.print(L"More info: {}", receiveBuffer);
			} else {
				bp.print(
					L"Can't get readable explanation of error because FormatMessageW failed (error {})",
					GetLastError()
				);
			}
			RmLog(rm, LOG_ERROR, bp.getBufferPtr());

			LocalFree(receiveBuffer);

			return;
		}

		auto threadArgs = new ThreadArguments;
		threadArgs->dllHandle = dllHandle;
		threadArgs->queue = &queue;
		const auto threadHandle = CreateThread(
			nullptr,
			0,
			&asyncSender_run,
			threadArgs,
			0,
			nullptr
		);

		if (threadHandle == nullptr) {
			const auto errorCode = GetLastError();
			BufferPrinter bp;
			bp.print(L"Log and bangs are not available, CreateThread failed (error {})", errorCode);
			RmLog(rm, LOG_ERROR, bp.getBufferPtr());

			wchar_t* receiveBuffer = nullptr;
			const auto messageLength = FormatMessageW(
				FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
				nullptr,
				errorCode,
				0,
				reinterpret_cast<LPWSTR>(&receiveBuffer),
				0,
				nullptr
			);
			if (messageLength != 0) {
				bp.print(L"More info: {}", receiveBuffer);
			} else {
				bp.print(
					L"Can't get readable explanation of error because FormatMessageW failed (error {})",
					GetLastError()
				);
			}
			RmLog(rm, LOG_ERROR, bp.getBufferPtr());

			LocalFree(receiveBuffer);

			FreeLibrary(dllHandle);
			delete threadArgs;
		}
	}

	void deinit() {
		counter.fetch_add(-1);
		if (counter.load() == 0) {
			try {
				Message mes{};
				mes.type = MessageType::eKILL;
				auto lock = queue.getLock();
				queue.buffer.push_back(mes);
				queue.sleepVariable.notify_one();
				initialized = false;
			} catch (...) { }
		}
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
		auto lock = queue.getLock();
		queue.buffer.push_back(std::move(value));
		queue.sleepVariable.notify_one();
	}

public:
	static void processMessage(const Message& value) {
		switch (value.type) {
		case MessageType::eNONE: break;
		case MessageType::eLOG:
			RmLog(value.rainmeterData, value.logLevel, value.messageText.c_str());
			break;
		case MessageType::eEXECUTE:
			RmExecute(value.rainmeterData, value.messageText.c_str());
			break;
		default: break;
		}
	}
};

AsyncRainmeterMessageSender asyncSender{};


DWORD WINAPI asyncSender_run(void* param) {
	using namespace std::chrono_literals;

	auto args = static_cast<ThreadArguments*>(param);
	HMODULE handle = args->dllHandle;
	auto& queue = *args->queue;
	delete args;

	// RmLog(nullptr, 0, L"asyncSender_run started");

	{
		auto lock = queue.getLock();
		std::vector<Message> localQueueBuffer;
		while (true) {
			std::swap(localQueueBuffer, queue.buffer);

			if (!localQueueBuffer.empty()) {
				lock.unlock();

				for (auto& mes : localQueueBuffer) {
					if (mes.type == MessageType::eKILL) {
						goto label_LOOP_END;
					}
					asyncSender.processMessage(mes);
				}
				localQueueBuffer.clear();

				lock.lock();
			}

			if (queue.buffer.empty()) {
				queue.sleepVariable.wait(lock);
			}
		}
	label_LOOP_END:;
	}

	// RmLog(nullptr, 0, L"asyncSender_run finished");

	FreeLibraryAndExitThread(handle, 0);
}


Rainmeter::InstanceKeeper::InstanceKeeper(void* rm) {
	asyncSender.init(rm);
	initialized = true;
}

void Rainmeter::InstanceKeeper::deinit() {
	if (initialized) {
		asyncSender.deinit();
		initialized = false;
	}
}

void Rainmeter::Logger::logRainmeter(LogLevel logLevel, sview message) const {
	asyncSender.log(rm, message % own(), logLevel);
}

Rainmeter::Rainmeter(void* rm) :
	rm(rm) {
	skin = Skin{ RmGetSkin(rm) };
	measureName = RmGetMeasureName(rm);
}

sview Rainmeter::readString(sview optionName, const wchar_t* defaultValue, bool replaceVariables) const {
	return RmReadString(rm, makeNullTerminated(optionName), defaultValue, replaceVariables);
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

void Rainmeter::executeCommandAsync(sview command, Skin skin) {
	asyncSender.execute(skin.getRawPointer(), command % own());
}

void* Rainmeter::getWindowHandle() {
	return RmGetSkinWindow(rm);
}

void Rainmeter::sourcelessLog(const wchar_t* message) {
	asyncSender.log(nullptr, message, Logger::LogLevel::eDEBUG);
}

Rainmeter::InstanceKeeper Rainmeter::getInstanceKeeper() {
	return InstanceKeeper{ rm };
}

const wchar_t* Rainmeter::makeNullTerminated(sview view) const {
	// can't use view[view.length()] because it's out of view's range
	if (view.data()[view.length()] == L'\0') {
		return view.data();
	}

	optionNameBuffer = view;
	return optionNameBuffer.c_str();
}
