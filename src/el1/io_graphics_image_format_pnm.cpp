#include "io_graphics_image_format_pnm.hpp"
#include "io_text_string.hpp"
#include "io_text_encoding_utf8.hpp"
#include <endian.h>

namespace el1::io::graphics::image::format::pnm
{
	using namespace io::text::string;

	void SaveP6(const TRasterImage& img, stream::IBinarySink& stream, const u16_t max_value)
	{
		using namespace io::text::encoding::utf8;
		TString::Format("P6 %d %d %d\n", img.Width(), img.Height(), max_value).chars.Pipe().Transform(TUTF8Encoder()).ToStream(stream);

		const float f_max_value = max_value;

		for(usys_t y = 0; y < img.Height(); y++)
			for(usys_t x = 0; x < img.Width(); x++)
			{
				const auto& pixel = img[{x,y}];

				if(max_value <= 255)
				{
					const rgb8_t rgb = {
						(u8_t)(pixel[0] * f_max_value),
						(u8_t)(pixel[1] * f_max_value),
						(u8_t)(pixel[2] * f_max_value),
					};
					stream.WriteAll((const byte_t*)&rgb, 3);
				}
				else
				{
					const rgb16_t rgb = {
						htobe16((u16_t)(pixel[0] * f_max_value)),
						htobe16((u16_t)(pixel[1] * f_max_value)),
						htobe16((u16_t)(pixel[2] * f_max_value)),
					};
					stream.WriteAll((const byte_t*)&rgb, 6);
				}
			}
	}

	static bool IsWhitespace(const byte_t byte)
	{
		return (byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n');
	}

	static byte_t EatWhitespace(stream::IBinarySource& stream)
	{
		byte_t buffer;
		do stream.ReadAll(&buffer, 1); while(IsWhitespace(buffer));
		return buffer;
	}

	static TString NextField(stream::IBinarySource& stream)
	{
		TString s;
		byte_t buffer = EatWhitespace(stream);
		for(;;)
		{
			s.chars.Append(TUTF32((u32_t)buffer));
			stream.ReadAll(&buffer, 1);
			if(IsWhitespace(buffer))
				break;
		}
		return s;
	}

	TRasterImage LoadP6(stream::IBinarySource& stream)
	{
		{
			byte_t magic[2];
			stream.ReadAll(magic, 2);
			EL_ERROR(!(magic[0] == 'P' && magic[1] == '6'), TInvalidArgumentException, "stream", "stream does not contain a P6 image");
		}

		const usys_t width = NextField(stream).ToInteger();
		const usys_t height = NextField(stream).ToInteger();
		const usys_t n = width * height;
		const s64_t max_value = NextField(stream).ToInteger();
		EL_ERROR(max_value > 65535, TInvalidArgumentException, "max_value", "max_value must be <= 65535");
		const float f_max_value = (float)max_value;

		TList<pixel_t> pixelsf;
		pixelsf.SetCount(n);

		if(max_value <= 255)
		{
			TList<rgb8_t> pixels8;
			pixels8.SetCount(n);
			stream.ReadAll((byte_t*)&pixels8[0], n * 3);

			for(usys_t i = 0; i < n; i++)
			{
				auto& pixelf = pixelsf[i];
				auto pixel8 = pixels8[i];

				EL_ERROR(pixel8[0] > max_value || pixel8[1] > max_value || pixel8[2] > max_value, TException, "pixel color value bigger than max-value");

				pixelf[0] = static_cast<float>(pixel8[0]) / f_max_value;
				pixelf[1] = static_cast<float>(pixel8[1]) / f_max_value;
				pixelf[2] = static_cast<float>(pixel8[2]) / f_max_value;
				pixelf[3] = 0;
			}
		}
		else
		{
			TList<rgb16_t> pixels16;
			pixels16.SetCount(n);
			stream.ReadAll((byte_t*)&pixels16[0], n * 6);

			for(usys_t i = 0; i < n; i++)
			{
				auto& pixelf = pixelsf[i];
				auto pixel16 = pixels16[i];

				pixel16[0] = be16toh(pixel16[0]);
				pixel16[1] = be16toh(pixel16[1]);
				pixel16[2] = be16toh(pixel16[2]);

				EL_ERROR(pixel16[0] > max_value || pixel16[1] > max_value || pixel16[2] > max_value, TException, "pixel color value bigger than max-value");

				pixelf[0] = static_cast<float>(pixel16[0]) / f_max_value;
				pixelf[1] = static_cast<float>(pixel16[1]) / f_max_value;
				pixelf[2] = static_cast<float>(pixel16[2]) / f_max_value;
				pixelf[3] = 0;
			}
		}

		return TRasterImage({width,height}, std::move(pixelsf));
	}
}
