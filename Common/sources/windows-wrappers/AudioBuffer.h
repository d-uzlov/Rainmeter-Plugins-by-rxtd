#pragma once

namespace rxtd::utils {

	class IAudioCaptureClientWrapper;

	class AudioBuffer {
		friend IAudioCaptureClientWrapper;

		IAudioCaptureClientWrapper& parent;
		const index id;
		const bool silent{ };
		const uint8_t* const buffer = nullptr;
		const uint32_t size{ };

		AudioBuffer(IAudioCaptureClientWrapper& parent, index id, bool silent, const uint8_t* buffer, uint32_t size);

	public:
		// I want lifetime of this object to be very limited
		// Ideally it should be limited to one block where it was created
		// So no move constructors are allowed
		AudioBuffer(const AudioBuffer& other) = delete;
		AudioBuffer(AudioBuffer&& other) noexcept = delete;
		AudioBuffer& operator=(const AudioBuffer& other) = delete;
		AudioBuffer& operator=(AudioBuffer&& other) noexcept = delete;

		~AudioBuffer();

		bool isSilent() const {
			return silent;
		}

		const uint8_t* getBuffer() const {
			return buffer;
		}

		uint32_t getSize() const {
			return size;
		}
	};
}
