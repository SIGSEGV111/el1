#include <gtest/gtest.h>
#include <el1/error.hpp>
#include <el1/io_collection_list.hpp>
#include <el1/io_compression_deflate.hpp>
#include <el1/io_text_terminal.hpp>
#include <el1/io_graphics_image.hpp>
#include <el1/io_graphics_image_format_png.hpp>
#include <el1/io_graphics_image_format_pnm.hpp>
#include <el1/io_graphics_plotter.hpp>
#include <zlib.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

using namespace ::testing;

namespace
{
	using namespace el1::error;
	using namespace el1::io::types;
	using namespace el1::io::collection::list;
	using namespace el1::io::compression::deflate;
	using namespace el1::io::text::terminal;
	using namespace el1::io::graphics::image;
	using namespace el1::io::graphics::image::format::png;
	using namespace el1::io::graphics::image::format::pnm;
	using namespace el1::io::graphics::plotter;

	using TByteBuffer = TList<byte_t>;

	void appendU32BE(TByteBuffer& data, const u32_t value)
	{
		data.Append((byte_t)(value >> 24));
		data.Append((byte_t)(value >> 16));
		data.Append((byte_t)(value >> 8));
		data.Append((byte_t)value);
	}

	void appendChunk(TByteBuffer& png, const char type[5], array_t<const byte_t> data)
	{
		appendU32BE(png, (u32_t)data.Count());
		png.Append((const byte_t*)type, 4);
		png.Append(data);

		uLong crc = crc32(0, Z_NULL, 0);
		crc = crc32(crc, reinterpret_cast<const Bytef*>(type), 4);
		if(!data.IsEmpty())
			crc = crc32(crc, data.ItemPtr(0), data.Count());
		appendU32BE(png, (u32_t)crc);
	}

	void appendChunk(TByteBuffer& png, const char type[5], const TByteBuffer& data)
	{
		appendChunk(png, type, (array_t<const byte_t>)data);
	}

	void appendChunk(TByteBuffer& png, const char type[5])
	{
		appendChunk(png, type, TByteBuffer());
	}

	TByteBuffer compressBytes(const TByteBuffer& input)
	{
		uLongf n_output = compressBound(input.Count());
		TByteBuffer output(n_output);
		output.SetCount(n_output);
		EXPECT_EQ(compress2(output.ItemPtr(0), &n_output, input.ItemPtr(0), input.Count(), Z_BEST_SPEED), Z_OK);
		return output;
	}

	int paethPredictor(const int left, const int up, const int upper_left)
	{
		const int estimate = left + up - upper_left;
		const int distance_left = std::abs(estimate - left);
		const int distance_up = std::abs(estimate - up);
		const int distance_upper_left = std::abs(estimate - upper_left);
		if(distance_left <= distance_up && distance_left <= distance_upper_left)
			return left;
		if(distance_up <= distance_upper_left)
			return up;
		return upper_left;
	}

	TByteBuffer filterScanline(const byte_t filter, const TByteBuffer& raw, const TByteBuffer& previous, const usys_t bytes_per_pixel)
	{
		TByteBuffer filtered(raw.Count());
		filtered.SetCount(raw.Count());
		for(usys_t i = 0; i < raw.Count(); i++)
		{
			const byte_t left = i >= bytes_per_pixel ? raw[i - bytes_per_pixel] : 0;
			const byte_t up = previous.IsEmpty() ? 0 : previous[i];
			const byte_t upper_left = previous.IsEmpty() || i < bytes_per_pixel ? 0 : previous[i - bytes_per_pixel];
			int predictor = 0;

			switch(filter)
			{
				case 0:
					predictor = 0;
					break;
				case 1:
					predictor = left;
					break;
				case 2:
					predictor = up;
					break;
				case 3:
					predictor = (left + up) / 2;
					break;
				case 4:
					predictor = paethPredictor(left, up, upper_left);
					break;
				default:
					predictor = 0;
					break;
			}

			filtered[i] = (byte_t)(raw[i] - predictor);
		}
		return filtered;
	}

	TByteBuffer makePng(
		const u32_t width,
		const u32_t height,
		const byte_t bit_depth,
		const byte_t color_type,
		const TList<TByteBuffer>& rows,
		const TList<byte_t>& filters,
		const TByteBuffer& palette = {},
		const byte_t compression_method = 0,
		const byte_t filter_method = 0,
		const byte_t interlace_method = 0,
		const bool add_unknown_chunk = false,
		const bool split_idat = false)
	{
		TByteBuffer png = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
		TByteBuffer ihdr;
		appendU32BE(ihdr, width);
		appendU32BE(ihdr, height);
		ihdr.Append(bit_depth);
		ihdr.Append(color_type);
		ihdr.Append(compression_method);
		ihdr.Append(filter_method);
		ihdr.Append(interlace_method);

		//term.Print("1\n");
		appendChunk(png, "IHDR", ihdr);
		//term.Print("2\n");

		if(!palette.IsEmpty())
			appendChunk(png, "PLTE", palette);

		//term.Print("3\n");

		if(add_unknown_chunk)
			appendChunk(png, "tEXt", { 'k', 0, 'v' });

		//term.Print("4\n");

		TByteBuffer scanlines;
		TByteBuffer previous;
		for(usys_t i = 0; i < rows.Count(); i++)
		{
			//term.Print("5\n");
			const byte_t filter = filters[i];
			const unsigned components = color_type == 0 || color_type == 3 ? 1 : color_type == 4 ? 2 : color_type == 2 ? 3 : 4;
			const usys_t bytes_per_pixel = std::max<usys_t>(1, bit_depth * components / 8);
			//term.Print("6\n");
			scanlines.Append(filter);
			//term.Print("6.5\n");
			const TByteBuffer filtered = filterScanline(filter, rows[i], previous, bytes_per_pixel);
			//term.Print("7\n");
			scanlines.Append(filtered);
			previous = rows[i];
		}
		//term.Print("8\n");

		const TByteBuffer compressed = compressBytes(scanlines);
		//term.Print("9\n");
		if(split_idat && compressed.Count() > 1)
		{
			const usys_t split = compressed.Count() / 2;
			//term.Print("10\n");
			appendChunk(png, "IDAT", array_t<const byte_t>(compressed, split));
			//term.Print("11\n");
			appendChunk(png, "IDAT", array_t<const byte_t>(compressed.ItemPtr(split), compressed.Count() - split));
		}
		else
		{
			//term.Print("12\n");
			appendChunk(png, "IDAT", compressed);
		}

		//term.Print("13\n");

		appendChunk(png, "IEND");
		//term.Print("14\n");
		return png;
	}

	TRasterImage loadPng(const TByteBuffer& data)
	{
		TArraySource<byte_t> source(array_t<const byte_t>(data.ItemPtr(0), data.Count()));
		return LoadPNG(source);
	}

	void expectPixel(const pixel_t& actual, const pixel_t& expected, const float tolerance = 0.0001f)
	{
		for(unsigned i = 0; i < 4; i++)
			EXPECT_NEAR(actual[i], expected[i], tolerance);
	}

	TEST(io_graphics_image, DetectImageFormat)
	{
		const auto detect = [](const TByteBuffer& bytes)
		{
			return DetectImageFormat(array_t<const byte_t>(bytes.ItemPtr(0), bytes.Count()));
		};

		EXPECT_EQ(detect({ 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a }), EImageFormat::PNG);
		EXPECT_EQ(detect({ 0xff, 0xd8, 0xff }), EImageFormat::JPG);
		EXPECT_EQ(detect({ 'G', 'I', 'F', '8', '7', 'a' }), EImageFormat::GIF);
		EXPECT_EQ(detect({ 'G', 'I', 'F', '8', '9', 'a' }), EImageFormat::GIF);
		EXPECT_EQ(detect({ 'B', 'M' }), EImageFormat::BMP);
		EXPECT_EQ(detect({ 'M', 'M', 0, 0x2a }), EImageFormat::TIFF);
		EXPECT_EQ(detect({ 'I', 'I', 0x2a, 0 }), EImageFormat::TIFF);
		EXPECT_EQ(detect({ 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P' }), EImageFormat::WEBP);
		EXPECT_EQ(detect({ '<', '?', 'x', 'm', 'l' }), EImageFormat::SVG);
		EXPECT_EQ(detect({ '<', 's', 'v', 'g', ' ' }), EImageFormat::SVG);
		EXPECT_EQ(detect({ 'P', '6' }), EImageFormat::PPM);
		EXPECT_EQ(detect({ 'P', '3' }), EImageFormat::PPM);
		EXPECT_EQ(detect({ 'P', '5' }), EImageFormat::PGM);
		EXPECT_EQ(detect({ 'P', '2' }), EImageFormat::PGM);
		EXPECT_EQ(detect({ 'P', '4' }), EImageFormat::PBM);
		EXPECT_EQ(detect({ 'P', '1' }), EImageFormat::PBM);
		EXPECT_EQ(detect({ 0x89, 'P' }), EImageFormat::UNKNOWN);
		EXPECT_EQ(detect({}), EImageFormat::UNKNOWN);
	}

	TEST(io_graphics_image, RasterOperations)
	{
		const pixel_t background(0.1f, 0.2f, 0.3f, 0.4f);
		TRasterImage image({ 2, 2 }, background);
		EXPECT_EQ(image.Width(), 2U);
		EXPECT_EQ(image.Height(), 2U);
		EXPECT_EQ(image.Pixels().Count(), 4U);
		expectPixel(image[{ -1, -1 }], background);
		expectPixel(image[{ 2, 2 }], background);

		image[{ 0, 0 }] = pixel_t(0.0f, 0.25f, 1.0f, 0.75f);
		image[{ 1, 1 }] = pixel_t(0.8f, 0.7f, 0.6f, 0.5f);
		expectPixel(image[{ -2, -2 }], image[{ 0, 0 }]);
		expectPixel(image[{ 3, 3 }], image[{ 1, 1 }]);

		image.Invert();
		expectPixel(image[{ 0, 0 }], pixel_t(1.0f, 0.75f, 0.0f, 0.75f));
		expectPixel(image[{ 1, 1 }], pixel_t(0.2f, 0.3f, 0.4f, 0.5f));

		const pixel_t fill(0.9f, 0.8f, 0.7f, 0.6f);
		image.Size({ 3, 3 }, fill);
		expectPixel(image[{ 0, 0 }], pixel_t(1.0f, 0.75f, 0.0f, 0.75f));
		expectPixel(image[{ 2, 0 }], fill);
		expectPixel(image[{ 0, 2 }], fill);
		image.Size({ 1, 1 });
		EXPECT_EQ(image.Pixels().Count(), 1U);
		expectPixel(image[{ 0, 0 }], pixel_t(1.0f, 0.75f, 0.0f, 0.75f));

		EXPECT_THROW(TRasterImage(size2i_t(2, 2), TList<pixel_t>({ background })), TInvalidArgumentException);
	}

	TEST(io_compression_deflate, Inflate)
	{
		TByteBuffer input(1048576 + 257);
		for(usys_t i = 0; i < input.Count(); i++)
			input[i] = (byte_t)((i * 37 + i / 251) & 0xff);

		const TByteBuffer compressed = compressBytes(input);
		const TList<byte_t> output = Inflate(array_t<const byte_t>(compressed.ItemPtr(0), compressed.Count()));
		ASSERT_EQ(output.Count(), input.Count());
		EXPECT_EQ(::memcmp(output.ItemPtr(0), input.ItemPtr(0), input.Count()), 0);

		const byte_t invalid[] = { 1, 2, 3, 4 };
		EXPECT_THROW(Inflate(array_t<const byte_t>(invalid, sizeof(invalid))), TException);
	}

	TEST(io_graphics_pnm, SaveAndLoad8Bit)
	{
		TRasterImage image({ 2, 1 });
		image[{ 0, 0 }] = pixel_t(0.0f, 0.5f, 1.0f, 0.0f);
		image[{ 1, 0 }] = pixel_t(1.0f, 0.25f, 0.0f, 0.0f);
		TList<byte_t> encoded;
		TListSink<byte_t> sink(&encoded);
		SaveP6(image, sink, 255);

		const char header[] = "P6 2 1 255\n";
		ASSERT_EQ(encoded.Count(), sizeof(header) - 1 + 6);
		EXPECT_EQ(::memcmp(encoded.ItemPtr(0), header, sizeof(header) - 1), 0);
		EXPECT_EQ(encoded[sizeof(header) - 1 + 0], 0);
		EXPECT_EQ(encoded[sizeof(header) - 1 + 1], 127);
		EXPECT_EQ(encoded[sizeof(header) - 1 + 2], 255);

		TArraySource<byte_t> source(encoded);
		const TRasterImage decoded = LoadP6(source);
		EXPECT_EQ(decoded.Size(), size2i_t(2, 1));
		expectPixel(decoded[{ 0, 0 }], pixel_t(0.0f, 127.0f / 255.0f, 1.0f, 0.0f));
		expectPixel(decoded[{ 1, 0 }], pixel_t(1.0f, 63.0f / 255.0f, 0.0f, 0.0f));
	}

	TEST(io_graphics_pnm, SaveAndLoad16BitAndComments)
	{
		TRasterImage image({ 1, 1 }, pixel_t(1.0f, 0.5f, 0.25f, 0.0f));
		TList<byte_t> encoded;
		TListSink<byte_t> sink(&encoded);
		SaveP6(image, sink, 65535);
		TArraySource<byte_t> source(encoded);
		const TRasterImage decoded = LoadP6(source);
		expectPixel(decoded[{ 0, 0 }], pixel_t(1.0f, 32767.0f / 65535.0f, 16383.0f / 65535.0f, 0.0f));

		const TByteBuffer commented = {
			'P', '6', '\n', '#', ' ', 'c', 'o', 'm', 'm', 'e', 'n', 't', '\n',
			'2', ' ', '1', '\n', '2', '5', '5', '\n',
			0, 127, 255, 255, 127, 0
		};
		TArraySource<byte_t> commented_source(array_t<const byte_t>(commented.ItemPtr(0), commented.Count()));
		const TRasterImage commented_image = LoadP6(commented_source);
		EXPECT_EQ(commented_image.Size(), size2i_t(2, 1));
		expectPixel(commented_image[{ 1, 0 }], pixel_t(1.0f, 127.0f / 255.0f, 0.0f, 0.0f));
	}

	TEST(io_graphics_pnm, ValidationAndGenericLoad)
	{
		const TByteBuffer valid = { 'P', '6', ' ', '1', ' ', '1', ' ', '2', '5', '5', '\n', 1, 2, 3 };
		const std::unique_ptr<IImage> loaded = IImage::Load(array_t<const byte_t>(valid.ItemPtr(0), valid.Count()));
		ASSERT_NE(dynamic_cast<TRasterImage*>(loaded.get()), nullptr);

		const TByteBuffer invalid_magic = { 'P', '5', ' ', '1', ' ', '1', ' ', '2', '5', '5', '\n', 1 };
		TArraySource<byte_t> invalid_magic_source(array_t<const byte_t>(invalid_magic.ItemPtr(0), invalid_magic.Count()));
		EXPECT_THROW(LoadP6(invalid_magic_source), TInvalidArgumentException);

		const TByteBuffer invalid_max = { 'P', '6', ' ', '1', ' ', '1', ' ', '6', '5', '5', '3', '6', '\n', 0, 0, 0, 0, 0, 0 };
		TArraySource<byte_t> invalid_max_source(array_t<const byte_t>(invalid_max.ItemPtr(0), invalid_max.Count()));
		EXPECT_THROW(LoadP6(invalid_max_source), TInvalidArgumentException);

		const TByteBuffer zero_max = { 'P', '6', ' ', '1', ' ', '1', ' ', '0', '\n', 0, 0, 0 };
		TArraySource<byte_t> zero_max_source(array_t<const byte_t>(zero_max.ItemPtr(0), zero_max.Count()));
		EXPECT_THROW(LoadP6(zero_max_source), TInvalidArgumentException);

		const TByteBuffer excessive_pixel = { 'P', '6', ' ', '1', ' ', '1', ' ', '1', '0', '\n', 11, 0, 0 };
		TArraySource<byte_t> excessive_pixel_source(array_t<const byte_t>(excessive_pixel.ItemPtr(0), excessive_pixel.Count()));
		EXPECT_THROW(LoadP6(excessive_pixel_source), TException);

		TRasterImage image({ 1, 1 });
		TList<byte_t> encoded;
		TListSink<byte_t> sink(&encoded);
		EXPECT_THROW(SaveP6(image, sink, 0), TInvalidArgumentException);
	}

	TEST(io_graphics_png, TruecolorFiltersAndGenericLoad)
	{
		const TList<TByteBuffer> rows = {
			{ 10, 20, 30, 40, 50, 60 },
			{ 15, 25, 35, 45, 55, 65 },
			{ 20, 30, 40, 50, 60, 70 },
			{ 25, 35, 45, 55, 65, 75 },
			{ 30, 40, 50, 60, 70, 80 },
		};
		const TByteBuffer png = makePng(2, 5, 8, 2, rows, { 0, 1, 2, 3, 4 }, {}, 0, 0, 0, true, true);
		const std::unique_ptr<IImage> loaded = IImage::Load(png);
		const auto* const image = dynamic_cast<const TRasterImage*>(loaded.get());
		ASSERT_NE(image, nullptr);
		EXPECT_EQ(image->Size(), size2i_t(2, 5));
		for(usys_t y = 0; y < rows.Count(); y++)
			for(usys_t x = 0; x < 2; x++)
				expectPixel((*image)[{ (s32_t)x, (s32_t)y }], pixel_t(
					rows[y][x * 3 + 0] / 255.0f,
					rows[y][x * 3 + 1] / 255.0f,
					rows[y][x * 3 + 2] / 255.0f,
					0.0f));
	}

	TEST(io_graphics_png, PackedAndPaletteFormats)
	{
		{
			SCOPED_TRACE("grayscale 1-bit");
			const TRasterImage image = loadPng(makePng(8, 1, 1, 0, { { 0xb2 } }, { 0 }));
			const float expected[] = { 1, 0, 1, 1, 0, 0, 1, 0 };
			for(s32_t x = 0; x < 8; x++)
				expectPixel(image[{ x, 0 }], pixel_t(expected[x], expected[x], expected[x], 0));
		}

		{
			SCOPED_TRACE("grayscale 2-bit");
			const TRasterImage image = loadPng(makePng(4, 1, 2, 0, { { 0x1b } }, { 0 }));
			for(s32_t x = 0; x < 4; x++)
			{
				const float value = x / 3.0f;
				expectPixel(image[{ x, 0 }], pixel_t(value, value, value, 0));
			}
		}

		{
			SCOPED_TRACE("grayscale 4-bit");
			const TRasterImage image = loadPng(makePng(2, 1, 4, 0, { { 0x1f } }, { 0 }));
			expectPixel(image[{ 0, 0 }], pixel_t(1.0f / 15.0f, 1.0f / 15.0f, 1.0f / 15.0f, 0));
			expectPixel(image[{ 1, 0 }], pixel_t(1, 1, 1, 0));
		}

		{
			SCOPED_TRACE("indexed 2-bit");
			const TByteBuffer palette = { 255, 0, 0, 0, 255, 0, 0, 0, 255 };
			const TRasterImage image = loadPng(makePng(3, 1, 2, 3, { { 0x18 } }, { 0 }, palette));
			expectPixel(image[{ 0, 0 }], pixel_t(1, 0, 0, 0));
			expectPixel(image[{ 1, 0 }], pixel_t(0, 1, 0, 0));
			expectPixel(image[{ 2, 0 }], pixel_t(0, 0, 1, 0));
		}
	}

	TEST(io_graphics_png, WideSampleAndAlphaFormats)
	{
		{
			const TRasterImage image = loadPng(makePng(1, 1, 16, 0, { { 0x80, 0x00 } }, { 0 }));
			const float value = 32768.0f / 65535.0f;
			expectPixel(image[{ 0, 0 }], pixel_t(value, value, value, 0));
		}

		{
			const TRasterImage image = loadPng(makePng(1, 1, 8, 4, { { 64, 192 } }, { 0 }));
			expectPixel(image[{ 0, 0 }], pixel_t(64.0f / 255.0f, 64.0f / 255.0f, 64.0f / 255.0f, 192.0f / 255.0f));
		}

		{
			const TRasterImage image = loadPng(makePng(1, 1, 16, 4, { { 0x40, 0x00, 0xc0, 0x00 } }, { 0 }));
			expectPixel(image[{ 0, 0 }], pixel_t(16384.0f / 65535.0f, 16384.0f / 65535.0f, 16384.0f / 65535.0f, 49152.0f / 65535.0f));
		}

		{
			const TRasterImage image = loadPng(makePng(1, 1, 8, 6, { { 1, 2, 3, 4 } }, { 0 }));
			expectPixel(image[{ 0, 0 }], pixel_t(1.0f / 255.0f, 2.0f / 255.0f, 3.0f / 255.0f, 4.0f / 255.0f));
		}

		{
			const TRasterImage image = loadPng(makePng(1, 1, 16, 2, { { 0x10, 0, 0x20, 0, 0x30, 0 } }, { 0 }));
			expectPixel(image[{ 0, 0 }], pixel_t(4096.0f / 65535.0f, 8192.0f / 65535.0f, 12288.0f / 65535.0f, 0));
		}

		{
			const TRasterImage image = loadPng(makePng(1, 1, 16, 6, { { 0x10, 0, 0x20, 0, 0x30, 0, 0x40, 0 } }, { 0 }));
			expectPixel(image[{ 0, 0 }], pixel_t(4096.0f / 65535.0f, 8192.0f / 65535.0f, 12288.0f / 65535.0f, 16384.0f / 65535.0f));
		}
	}

	TEST(io_graphics_png, Validation)
	{
		TByteBuffer invalid_magic = makePng(1, 1, 8, 0, { { 0 } }, { 0 });
		invalid_magic[0] = 0;
		EXPECT_THROW(loadPng(invalid_magic), TException);
		EXPECT_THROW(loadPng(makePng(1, 1, 4, 2, { { 0, 0 } }, { 0 })), TInvalidArgumentException);
		EXPECT_THROW(loadPng(makePng(1, 1, 8, 0, { { 0 } }, { 0 }, {}, 1)), TInvalidArgumentException);
		EXPECT_THROW(loadPng(makePng(1, 1, 8, 0, { { 0 } }, { 0 }, {}, 0, 1)), TInvalidArgumentException);
		EXPECT_THROW(loadPng(makePng(1, 1, 8, 0, { { 0 } }, { 0 }, {}, 0, 0, 1)), TInvalidArgumentException);
		EXPECT_THROW(loadPng(makePng(0, 1, 8, 0, { {} }, { 0 })), TInvalidArgumentException);
		EXPECT_THROW(loadPng(makePng(1, 1, 8, 3, { { 0 } }, { 0 })), TException);
		EXPECT_THROW(loadPng(makePng(1, 1, 8, 2, { { 0, 0, 0 } }, { 5 })), TException);
		EXPECT_THROW(loadPng(makePng(1, 1, 2, 3, { { 0 } }, { 0 }, { 1, 2 })), TException);
	}

	TEST(io_graphics_plotter, PaletteAndRasterJob)
	{
		TPalette palette;
		palette.tools = {
			pixel_t(1, 0, 0, 0),
			pixel_t(0, 1, 0, 0),
			pixel_t(0, 0, 1, 0),
		};
		EXPECT_EQ(palette.Select(pixel_t(0.8f, 0.1f, 0.1f, 0)), 0U);
		EXPECT_EQ(palette.Select(pixel_t(0.1f, 0.2f, 0.9f, 0)), 2U);
		EXPECT_THROW({ const u32_t selected = TPalette().Select(pixel_t()); (void)selected; }, TInvalidArgumentException);

		TRasterImage image({ 3, 1 }, pixel_t(0, 0, 0, 1));
		image[{ 0, 0 }] = pixel_t(1, 0, 0, 0);
		image[{ 1, 0 }] = pixel_t(1, 0, 0, 0);
		const TJob job = TJob::FromImage(image, 2.0f, nullptr);
		ASSERT_EQ(job.commands.Count(), 6U);
		ASSERT_NE(dynamic_cast<TPenCommand*>(job.commands[0].get()), nullptr);
		ASSERT_NE(dynamic_cast<TColorChangeCommand*>(job.commands[1].get()), nullptr);
		ASSERT_NE(dynamic_cast<TGotoCommand*>(job.commands[2].get()), nullptr);
		const auto* const pen_down = dynamic_cast<TPenCommand*>(job.commands[3].get());
		ASSERT_NE(pen_down, nullptr);
		EXPECT_EQ(pen_down->dir, TPenCommand::EDirection::DOWN);
		EXPECT_FLOAT_EQ(pen_down->stroke_width, 0.5f);
		const auto* const end_run = dynamic_cast<TGotoCommand*>(job.commands[4].get());
		ASSERT_NE(end_run, nullptr);
		EXPECT_EQ(end_run->endpoint, v2d_t(0.5f, 0.0f));
		const auto* const pen_up = dynamic_cast<TPenCommand*>(job.commands[5].get());
		ASSERT_NE(pen_up, nullptr);
		EXPECT_EQ(pen_up->dir, TPenCommand::EDirection::UP);

		const TJob palette_job = TJob::FromImage(image, 2.0f, &palette);
		ASSERT_NE(dynamic_cast<TToolChangeCommand*>(palette_job.commands[1].get()), nullptr);
		EXPECT_THROW(TJob::FromImage(TRasterImage({ 0, 0 }), 1.0f, nullptr), TInvalidArgumentException);
		EXPECT_THROW(TJob::FromImage(image, 0.0f, nullptr), TInvalidArgumentException);
	}
}
