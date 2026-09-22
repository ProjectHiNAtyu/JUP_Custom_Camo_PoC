#include "imgutils.hpp"

#include "stb_image.h"
#include <gsl/gsl>

#include <stdexcept>
#include <cstring>

#include <fstream>
#include <sstream>
#include <iomanip>

namespace imgutils
{


	image::image(const std::string& image_data)
	{
		int channels{};
		auto* rgb_image = stbi_load_from_memory(reinterpret_cast<const uint8_t*>(image_data.data()), static_cast<int>(image_data.size()), &this->width, &this->height, &channels, 4);
		if(!rgb_image)
		{
			throw std::runtime_error("Unable to load image");
		}

		auto _ = gsl::finally([rgb_image]()
		{
			stbi_image_free(rgb_image);
		});

		const auto size = this->width * this->height * 4;
		this->data.resize(size);
		
		std::memmove(this->data.data(), rgb_image, size);
	}


	image image::from_ddsfile(const std::string& path)
	{
		std::ifstream ifs(path, std::ios::binary);
		if (!ifs)
		{
			std::ostringstream oss;
			oss << "Failed to open DDS file: " << path;
			throw std::runtime_error(oss.str());
		}

		ifs.seekg(0, std::ios::end);
		auto size = static_cast<std::streamsize>(ifs.tellg());
		ifs.seekg(0, std::ios::beg);

		if (size <= 0)
		{
			std::ostringstream oss;
			oss << "DDS file empty or could not determine size: " << path;
			throw std::runtime_error(oss.str());
		}

		std::string fileData;
		fileData.resize(static_cast<size_t>(size));
		if (!ifs.read(&fileData[0], size))
		{
			std::ostringstream oss;
			oss << "Failed to read DDS file: " << path;
			throw std::runtime_error(oss.str());
		}

		// Minimum DDS size is header (4 + 124)
		if (fileData.size() < 128)
		{
			std::ostringstream oss;
			oss << "DDS file too small or invalid: " << path;
			throw std::runtime_error(oss.str());
		}

		if (std::memcmp(fileData.data(), "DDS ", 4) != 0)
		{
			std::ostringstream oss;
			oss << "Not a DDS file: " << path;
			throw std::runtime_error(oss.str());
		}

		// Read width/height from DDS header (little-endian)
		uint32_t height = 0;
		uint32_t width = 0;
		std::memcpy(&height, fileData.data() + 12, sizeof(uint32_t));
		std::memcpy(&width, fileData.data() + 16, sizeof(uint32_t));

		// Detect optional DX10 header: ddspf.dwFourCC at offset 84
		size_t pixelDataOffset = 128;
		if (fileData.size() >= 128 + 4)
		{
			if (std::memcmp(fileData.data() + 84, "DX10", 4) == 0)
			{
				// DDS_HEADER_DXT10 present (20 bytes)
				pixelDataOffset += 20;
			}
		}

		if (fileData.size() <= pixelDataOffset)
		{
			std::ostringstream oss;
			oss << "DDS file has no pixel data: " << path;
			throw std::runtime_error(oss.str());
		}

		image img;
		img.width = static_cast<int>(width);
		img.height = static_cast<int>(height);

		const size_t payloadSize = fileData.size() - pixelDataOffset;
		img.data.resize(payloadSize);
		std::memcpy(img.data.data(), fileData.data() + pixelDataOffset, payloadSize);

		return img;
	}


	image image::from_file(const std::string& path)
	{
		std::ifstream ifs(path, std::ios::binary);
		if (!ifs)
		{
			std::ostringstream oss;
			oss << "Failed to open image file: " << path;
			throw std::runtime_error(oss.str());
		}

		ifs.seekg(0, std::ios::end);

		auto size = static_cast<std::streamsize>(ifs.tellg());
		ifs.seekg(0, std::ios::beg);

		if (size <= 0)
		{
			std::ostringstream oss;
			oss << "Image file empty or could not determine size: " << path;
			throw std::runtime_error(oss.str());
		}

		std::string data;
		data.resize(static_cast<size_t>(size));
		if (!ifs.read(&data[0], size))
		{
			std::ostringstream oss;
			oss << "Failed to read image file: " << path;
			throw std::runtime_error(oss.str());
		}

		// Diagnostic: try to get image info from memory
		int iw = 0, ih = 0, ic = 0;
		if (!stbi_info_from_memory(reinterpret_cast<const unsigned char*>(data.data()), static_cast<int>(data.size()), &iw, &ih, &ic))
		{
			const char* reason = stbi_failure_reason();
			std::ostringstream oss;
			oss << "stbi_info_from_memory failed for '" << path << "' reason='" << (reason ? reason : "<null>") << "'";
			// still attempt to construct to let constructor produce a clearer error if possible
			printf("imgutils stbi_failure_reason %s\n", oss.str().c_str());
		}

		try {
			return image(data);
		}
		catch (const std::exception& e) {
			const char* reason = stbi_failure_reason();
			std::ostringstream oss;
			oss << "imgutils failed to load '" << path << "': " << e.what() << " stbi_reason='" << (reason ? reason : "<null>") << "'";
			throw std::runtime_error(oss.str());
		}
	}

	int image::get_width() const
	{
		return this->width;
	}

	int image::get_height() const
	{
		return this->height;
	}

	const void* image::get_buffer() const
	{
		return this->data.data();
	}

	size_t image::get_size() const
	{
		return this->data.size();
	}

	const std::string& image::get_data() const
	{
		return this->data;
	}
}
