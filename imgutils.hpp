#pragma once

#include <string>

namespace imgutils
{
	class image
	{
	public:
		// default constructor to allow manual population (e.g., from DDS payload)
		image() noexcept = default;
		image(const std::string& data);

		static image from_file(const std::string& path);
		static image from_ddsfile(const std::string& path);

		int get_width() const;
		int get_height() const;
		const void* get_buffer() const;
		size_t get_size() const;

		const std::string& get_data() const;

	private:
		int width{};
		int height{};
		std::string data{};
	};
}
