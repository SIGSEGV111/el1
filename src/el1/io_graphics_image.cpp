#include "io_graphics_image.hpp"
#include "io_graphics_image_format_png.hpp"
#include <string.h>

namespace el1::io::graphics::image
{
	EImageFormat DetectImageFormat(const array_t<const byte_t> header)
	{
		// Check for PNG (signature: 89 50 4E 47 0D 0A 1A 0A)
		if (header.Count() >= 8 && ::memcmp(&header[0], "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) == 0)
			return EImageFormat::PNG;

		// Check for JPG (signature: FF D8 FF)
		if (header.Count() >= 3 && ::memcmp(&header[0], "\xFF\xD8\xFF", 3) == 0)
			return EImageFormat::JPG;

		// Check for GIF (signature: GIF87a or GIF89a)
		if (header.Count() >= 6 && (::memcmp(&header[0], "GIF87a", 6) == 0 || ::memcmp(&header[0], "GIF89a", 6) == 0))
			return EImageFormat::GIF;

		// Check for BMP (signature: 42 4D)
		if (header.Count() >= 2 && ::memcmp(&header[0], "\x42\x4D", 2) == 0)
			return EImageFormat::BMP;

		// Check for TIFF (big-endian: 4D 4D 00 2A, little-endian: 49 49 2A 00)
		if (header.Count() >= 4 && (::memcmp(&header[0], "\x4D\x4D\x00\x2A", 4) == 0 || ::memcmp(&header[0], "\x49\x49\x2A\x00", 4) == 0))
			return EImageFormat::TIFF;

		// Check for WEBP (signature: RIFF....WEBP)
		if (header.Count() >= 12 && ::memcmp(&header[0], "RIFF", 4) == 0 && ::memcmp(&header[8], "WEBP", 4) == 0)
			return EImageFormat::WEBP;

		// Check for SVG (starts with '<?xml' or '<svg')
		if (header.Count() >= 5 && (::memcmp(&header[0], "<?xml", 5) == 0 || ::memcmp(&header[0], "<svg", 4) == 0))
			return EImageFormat::SVG;

		// Check for PPM (P6 or P3), PGM (P5 or P2), PBM (P4 or P1)
		if (header.Count() >= 2 && header[0] == 'P')
		{
			if (header[1] == '6' || header[1] == '3')
				return EImageFormat::PPM;
			if (header[1] == '5' || header[1] == '2')
				return EImageFormat::PGM;
			if (header[1] == '4' || header[1] == '1')
				return EImageFormat::PBM;
		}

		return EImageFormat::UNKNOWN;
	}

	std::unique_ptr<IImage> IImage::Load(io::file::TFile& file)
	{
		using namespace io::file;
		const TMapping map(&file, TAccess::RO);
		return Load(map);
	}

	std::unique_ptr<IImage> IImage::Load(const array_t<const byte_t> data)
	{
		const EImageFormat format = DetectImageFormat(data);
		TArraySource stream(data);
		return Load(stream, format);
	}

	std::unique_ptr<IImage> IImage::Load(IBinarySource& stream, const EImageFormat format)
	{
		switch(format)
		{
			case EImageFormat::PNG:
				return New<TRasterImage>(format::png::LoadPNG(stream));
			default:
				EL_NOT_IMPLEMENTED;
		}
	}

	size2i_t TRasterImage::AbsPos(pos2i_t pos) const
	{
		if(util::Abs(pos[0]) >= (s32_t)size[0])
			pos[0] %= size[0];

		if(util::Abs(pos[1]) >= (s32_t)size[1])
			pos[1] %= size[1];

		return {
			(u32_t)(pos[0] < 0 ? (s32_t)size[0] + pos[0] : pos[0]),
			(u32_t)(pos[1] < 0 ? (s32_t)size[1] + pos[1] : pos[1])
		};
	}

	usys_t TRasterImage::Pos2Index(const pos2i_t _pos) const
	{
		const size2i_t pos = AbsPos(_pos);
		return pos[1] * size[0] + pos[0];
	}

	array_t<pixel_t> TRasterImage::Pixels()
	{
		return pixels;
	}

	array_t<const pixel_t> TRasterImage::Pixels() const
	{
		return pixels;
	}

	pixel_t& TRasterImage::operator[](const pos2i_t pos)
	{
		return pixels[Pos2Index(pos)];
	}

	const pixel_t& TRasterImage::operator[](const pos2i_t pos) const
	{
		return pixels[Pos2Index(pos)];
	}

	size2i_t TRasterImage::Size() const
	{
		return size;
	}

	void TRasterImage::Size(const size2i_t new_size, const pixel_t background)
	{
		TList<pixel_t> new_pixels;
		new_pixels.SetCount(new_size.Space());

		const size2i_t min_size(util::Min(size[0], new_size[0]), util::Min(size[1], new_size[1]));

		for(u32_t y = 0; y < min_size[1]; y++)
			for(u32_t x = 0; x < min_size[0]; x++)
				new_pixels[y * new_size[0] + x] = pixels[y * size[0] + x];

		for(u32_t y = 0; y < min_size[1]; y++)
			for(u32_t x = size[0]; x < new_size[0]; x++)
				new_pixels[y * new_size[0] + x] = background;

		for(u32_t y = min_size[1]; y < new_size[1]; y++)
			for(u32_t x = 0; x < new_size[0]; x++)
				new_pixels[y * new_size[0] + x] = background;

		pixels = std::move(new_pixels);
		size = new_size;
	}

	void TRasterImage::Invert()
	{
		for(auto& px : pixels)
			for(unsigned i = 0; i < 3; i++)
				px[i] = 1.0f - px[i];
	}

	TRasterImage::TRasterImage(const size2i_t _size, const pixel_t background) : size({0,0})
	{
		Size(_size, background);
	}

	TRasterImage::TRasterImage(const size2i_t size, TList<pixel_t> _pixels) : pixels(std::move(_pixels)), size(size)
	{
		EL_ERROR(size.Space() != pixels.Count(), TInvalidArgumentException, "pixels", "pixels count does not match size");
	}
}
