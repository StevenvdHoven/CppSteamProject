#include "Core/Uid.h"
#include <charconv>
#include <mutex>
#include <random>

namespace Flex
{
	Uid Uid::Generate()
	{
		static std::mutex lock;
		static std::mt19937_64 generator = [] {
			std::random_device device;
			std::seed_seq seed{ device(), device(), device(), device(), device(), device(), device(), device() };
			return std::mt19937_64(seed);
		}();

		std::lock_guard<std::mutex> guard(lock);
		uint64_t value = 0;
		while (value == 0) {
			value = generator();
		}
		return Uid(value);
	}

	std::optional<Uid> Uid::FromString(std::string_view text)
	{
		if (text.size() != 16) {
			return std::nullopt;
		}

		uint64_t value = 0;
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
		if (error != std::errc{} || end != text.data() + text.size()) {
			return std::nullopt;
		}
		return Uid(value);
	}

	std::string Uid::ToString() const
	{
		char buffer[17]{};
		auto [end, error] = std::to_chars(buffer, buffer + 16, Value, 16);
		(void)error;

		// Left-pad with zeros to always get 16 characters.
		const size_t length = static_cast<size_t>(end - buffer);
		return std::string(16 - length, '0') + std::string(buffer, length);
	}
}
