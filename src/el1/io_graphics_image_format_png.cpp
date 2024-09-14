#include "io_graphics_image_format_png.hpp"
#include "io_compression_deflate.hpp"
#include "util_bits.hpp"
#include <endian.h>

namespace el1::io::graphics::image::format::png
{
	using namespace io::graphics::image;
	using namespace io::collection::list;

	static const byte_t MAGIC[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

	enum class EChunkType : u8_t
	{
		UNKNOWN, // Unknown chunk type

		IHDR,  // Image header
		PLTE,  // Palette table
		IDAT,  // Image data
		IEND,  // Image end

		CHRM,  // Chromaticity
		GAMA,  // Gamma correction
		ICCP,  // ICC profile
		SBIT,  // Significant bits
		SRGB,  // Standard RGB color space
		TEXT,  // Textual data
		ZTXT,  // Compressed textual data
		ITXT,  // International textual data
		BKGD,  // Background color
		PHYS,  // Physical pixel dimensions
		SPLT,  // Suggested palette
		HIST,  // Histogram
		TIME,  // Image modification time
		TRNS,  // Transparency
	};

	enum class EColorType : u8_t
	{
		GRAYSCALE = 0,         // Grayscale
		TRUECOLOR = 2,         // Truecolor (RGB)
		INDEXED_COLOR = 3,     // Indexed color (uses a palette)
		GRAYSCALE_ALPHA = 4,   // Grayscale with alpha channel
		TRUECOLOR_ALPHA = 6    // Truecolor with alpha channel (RGBA)
	};

	enum class ECompressionMethod : u8_t
	{
		DEFLATE = 0            // DEFLATE compression (only method defined by PNG standard)
	};

	enum class EFilterMethod : u8_t
	{
		ADAPTIVE = 0           // Adaptive filtering (only method defined by PNG standard)
	};

	// Enum class for PNG Scanline Filter Methods
	enum class EFilterType : u8_t
	{
		NONE = 0,       // No filtering; the scanline is transmitted as-is.
		SUB = 1,        // The byte is replaced by the difference between it and the corresponding byte of the prior pixel.
		UP = 2,         // The byte is replaced by the difference between it and the corresponding byte of the scanline above.
		AVERAGE = 3,    // The byte is replaced by the difference between it and the average of the corresponding bytes of the prior pixel and the scanline above.
		PAETH = 4       // The byte is replaced by the difference between it and the Paeth predictor of the corresponding bytes of the prior pixel, the scanline above, and the pixel above the prior pixel.
	};

	enum class EInterlaceMethod : u8_t
	{
		NONE = 0,              // No interlacing
		ADAM7 = 1              // Adam7 interlacing
	};

	struct IChunk
	{
		const EChunkType type;
		constexpr IChunk(const EChunkType type) : type(type) {}
		virtual ~IChunk();
	};

	struct TChunkHeader
	{
		u32_t size;
		char type[4];

		EChunkType Type() const EL_GETTER;
		static TChunkHeader LoadHeader(IBinarySource&);
	};

	struct TIHDRChunk : IChunk
	{
		u32_t width;               // Image width in pixels
		u32_t height;              // Image height in pixels
		u8_t bit_depth;            // Number of bits per sample or per palette index
		EColorType color_type;                   // Type of color (e.g., grayscale, truecolor, etc.)
		ECompressionMethod compression_method;   // Compression method used (always 0 for PNG)
		EFilterMethod filter_method;             // Filter method used (always 0 for PNG)
		EInterlaceMethod interlace_method;       // Interlace method (0 for none, 1 for Adam7)

		TIHDRChunk(IBinarySource& stream, const u32_t size);
	};

	struct TPLTEChunk : IChunk
	{
		TList<color::rgb8_t> palette;
		TPLTEChunk(IBinarySource& stream, const u32_t size, const unsigned bpc);
	};

	struct TIENDChunk : IChunk
	{
		constexpr TIENDChunk() : IChunk(EChunkType::IEND) {}
	};

	static void ReadAndVerifyMagic(IBinarySource& stream)
	{
		byte_t magic[8];
		stream.ReadAll(magic, 8);
		EL_ERROR(::memcmp(MAGIC, magic, 8) != 0, TException, "invalid PNG file magic");
	}

	constexpr unsigned ComponentsPerPixel(const EColorType color_type)
	{
		switch(color_type)
		{
			case EColorType::GRAYSCALE:
			case EColorType::INDEXED_COLOR:
				return 1;

			case EColorType::GRAYSCALE_ALPHA:
				return 2;

			case EColorType::TRUECOLOR:
				return 3;

			case EColorType::TRUECOLOR_ALPHA:
				return 4;
		}

		EL_THROW(TInvalidArgumentException, "color_type", "invalid/unknown color type");
	}

	u32_t ReadU32BE(IBinarySource& stream)
	{
		u32_t v;
		stream.ReadAll((byte_t*)&v, 4);
		return be32toh(v);
	}

	u8_t ReadU8(IBinarySource& stream)
	{
		u8_t v;
		stream.ReadAll((byte_t*)&v, 1);
		return v;
	}

	IChunk::~IChunk()
	{
		// nothing to do
	}

	EChunkType TChunkHeader::Type() const
	{
		if(::memcmp(type, "IHDR", 4) == 0) return EChunkType::IHDR;
		if(::memcmp(type, "PLTE", 4) == 0) return EChunkType::PLTE;
		if(::memcmp(type, "IDAT", 4) == 0) return EChunkType::IDAT;
		if(::memcmp(type, "IEND", 4) == 0) return EChunkType::IEND;
		if(::memcmp(type, "cHRM", 4) == 0) return EChunkType::CHRM;
		if(::memcmp(type, "gAMA", 4) == 0) return EChunkType::GAMA;
		if(::memcmp(type, "iCCP", 4) == 0) return EChunkType::ICCP;
		if(::memcmp(type, "sBIT", 4) == 0) return EChunkType::SBIT;
		if(::memcmp(type, "sRGB", 4) == 0) return EChunkType::SRGB;
		if(::memcmp(type, "tEXt", 4) == 0) return EChunkType::TEXT;
		if(::memcmp(type, "zTXt", 4) == 0) return EChunkType::ZTXT;
		if(::memcmp(type, "iTXt", 4) == 0) return EChunkType::ITXT;
		if(::memcmp(type, "bKGD", 4) == 0) return EChunkType::BKGD;
		if(::memcmp(type, "pHYs", 4) == 0) return EChunkType::PHYS;
		if(::memcmp(type, "sPLT", 4) == 0) return EChunkType::SPLT;
		if(::memcmp(type, "hIST", 4) == 0) return EChunkType::HIST;
		if(::memcmp(type, "tIME", 4) == 0) return EChunkType::TIME;
		if(::memcmp(type, "tRNS", 4) == 0) return EChunkType::TRNS;
		return EChunkType::UNKNOWN;
	}

	TChunkHeader TChunkHeader::LoadHeader(IBinarySource& stream)
	{
		TChunkHeader header;
		header.size = ReadU32BE(stream);
		stream.ReadAll((byte_t*)header.type, 4);
		return header;
	}

	TIHDRChunk::TIHDRChunk(IBinarySource& stream, const u32_t size) : IChunk(EChunkType::IHDR)
	{
		EL_ERROR(size != 13, TInvalidArgumentException, "size", "size must always be 13 as the IHDR is defined to be 13 bytes long");
		this->width = ReadU32BE(stream);
		this->height = ReadU32BE(stream);
		this->bit_depth = ReadU8(stream);
		this->color_type = static_cast<EColorType>(ReadU8(stream));
		this->compression_method = static_cast<ECompressionMethod>(ReadU8(stream));
		this->filter_method = static_cast<EFilterMethod>(ReadU8(stream));
		this->interlace_method = static_cast<EInterlaceMethod>(ReadU8(stream));
	}

	TPLTEChunk::TPLTEChunk(IBinarySource& stream, const u32_t size, const unsigned bpc) : IChunk(EChunkType::PLTE)
	{
		const unsigned n_entries = 1 << bpc;
		EL_ERROR(n_entries * 3 != size, TException, "PLTE chunk has invalid size");
		palette.SetCount(n_entries);
		stream.ReadAll((byte_t*)&palette[0], size);
	}

	// TTRANSChunk::TTRANSChunk(IBinarySource& stream, const u32_t size, const EColorType color_type, const unsigned bpc) : IChunk(EChunkType::PLTE)
	// {
	// 	switch(color_type)
	// 	{
	// 		case EColorType::INDEXED_COLOR:
	// 		case EColorType::GRAYSCALE:
	// 		case EColorType::TRUECOLOR:
	// 			EL_NOT_IMPLEMENTED;
	// 		default:
	// 			EL_THROW(TException, "TRANS chunk provided for a color-type that does not make use of it");
	// 	}
	// }

	struct scanline_t
	{
		array_t<byte_t> pixel_data;
		const scanline_t* previous;
		EFilterType filter_type;

		constexpr scanline_t() : pixel_data(nullptr, 0), previous(nullptr), filter_type(EFilterType::NONE) {}
	};

	struct TScanlineReader : IPipe<TScanlineReader, scanline_t>
	{
		using TOut = scanline_t;

		array_t<byte_t> data;
		const usys_t sz_scanline;
		TList<byte_t> zero_pixel_data;

		scanline_t previous;
		scanline_t current;
		usys_t pos;

		scanline_t* NextItem()
		{
			if(pos + sz_scanline > data.Count())
				return nullptr;

			previous = current;
			previous.previous = nullptr;
			current.pixel_data = array_t<byte_t>(&data[pos + 1], sz_scanline - 1);
			current.previous = &previous;
			current.filter_type = (EFilterType)data[pos];
			pos += sz_scanline;

			return &current;
		}

		TScanlineReader(array_t<byte_t> data, const usys_t sz_scanline) : data(data), sz_scanline(sz_scanline), pos(0)
		{
			zero_pixel_data.SetCount(sz_scanline - 1);
			::memset(&zero_pixel_data[0], 0, sz_scanline - 1);
			current.pixel_data = zero_pixel_data;
		}
	};

	template<unsigned bpc, unsigned ncc>
	struct TScanlineUnfilter
	{
		using TIn = scanline_t;
		using TOut = scanline_t;
		static constexpr unsigned bpx = util::Max(1U, bpc * ncc / 8U);

		void UnfilterSub(scanline_t& scanline)
		{
			for(usys_t i = bpx; i < scanline.pixel_data.Count(); i++)
				scanline.pixel_data[i] += scanline.pixel_data[i - bpx];
		};

		void UnfilterUp(scanline_t& scanline)
		{
			for(usys_t i = 0; i < scanline.pixel_data.Count(); i++)
				scanline.pixel_data[i] += scanline.previous->pixel_data[i];
		};

		void UnfilterAverage(scanline_t& scanline)
		{
			for(usys_t i = 0; i < bpx; i++)
			{
				const unsigned up = scanline.previous->pixel_data[i];
				const unsigned avg = up / 2U;
				scanline.pixel_data[i] += avg;
			}

			for(usys_t i = bpx; i < scanline.pixel_data.Count(); i++)
			{
				const unsigned left = scanline.pixel_data[i - bpx];
				const unsigned up = scanline.previous->pixel_data[i];
				const unsigned avg = (left + up) / 2U;
				scanline.pixel_data[i] += avg;
			}
		};

		// Dr. Alan William Paeth https://en.wikipedia.org/wiki/Alan_W._Paeth
		// After completing his final year of high school overseas in "Lan Schule Heim Stein and der Tran" (should probably read "Stein an der Traun"), Bavaria, Germany, he went on to earn a Bachelor of Computer Science at the California Institute of Technology. His first job after graduating was at a small start-up in Pasadena called Xerox.
		static constexpr int PaethPredictor(const int a, const int b, const int c)
		{
			const int p = a + b - c;
			const int pa = util::Abs(p - a);
			const int pb = util::Abs(p - b);
			const int pc = util::Abs(p - c);
			if (pa <= pb && pa <= pc)
				return a;
			else if (pb <= pc)
				return b;
			else
				return c;
		}

		void UnfilterPaeth(scanline_t& scanline)
		{
			for(usys_t i = 0; i < bpx; i++)
			{
				const int up = scanline.previous->pixel_data[i];
				const int p = PaethPredictor(0, up, 0);
				scanline.pixel_data[i] += p;
			}

			for(usys_t i = bpx; i < scanline.pixel_data.Count(); i++)
			{
				const int left = scanline.pixel_data[i - bpx];
				const int ul   = scanline.previous->pixel_data[i - bpx];
				const int up   = scanline.previous->pixel_data[i];
				const int p = PaethPredictor(left, up, ul);
				scanline.pixel_data[i] += p;
			}
		};

		template<typename TSourceStream>
		scanline_t* NextItem(TSourceStream* const source)
		{
			scanline_t* const scanline = source->NextItem();
			if(scanline == nullptr)
				return nullptr;

			switch(scanline->filter_type)
			{
				case EFilterType::NONE:
					break;

				case EFilterType::SUB:
					UnfilterSub(*scanline);
					break;

				case EFilterType::UP:
					UnfilterUp(*scanline);
					break;

				case EFilterType::AVERAGE:
					UnfilterAverage(*scanline);
					break;

				case EFilterType::PAETH:
					UnfilterPaeth(*scanline);
					break;

				default:
					EL_THROW(TException, "invalid/unknown scanline filter type");
			}

			return scanline;
		}

		constexpr TScanlineUnfilter() {}
	};

	template<EColorType color_type, unsigned bpc>
	struct TPixelDecoder
	{
		using TIn = scanline_t;
		using TOut = pixel_t;
		static constexpr auto ncc = ComponentsPerPixel(color_type);
		static constexpr float max_value = (1 << bpc) - 1;
		const TPLTEChunk* const plte;

		const scanline_t* scanline;
		usys_t npx;
		usys_t ipx;
		pixel_t pixel;

		void UnpackPixel()
		{
			if constexpr (bpc < 8)
			{
				if constexpr (color_type == EColorType::INDEXED_COLOR)
				{
					auto rgb = plte->palette[util::bits::GetBitField<bpc, usys_t>(&scanline->pixel_data[0], NEG1, ipx)];
					pixel[0] = static_cast<float>(rgb[0]) / 255.0f;
					pixel[1] = static_cast<float>(rgb[1]) / 255.0f;
					pixel[2] = static_cast<float>(rgb[2]) / 255.0f;
					pixel[3] = 0;
				}
				else
				{
					for(unsigned i = 0; i < ncc; i++)
						pixel[i] = static_cast<float>(util::bits::GetBitField<bpc, u32_t>(&scanline->pixel_data[0], NEG1, ipx * ncc + i)) / max_value;
				}
			}
			else if constexpr (bpc == 8)
			{
				if constexpr (color_type == EColorType::INDEXED_COLOR)
				{
					auto rgb = plte->palette[scanline->pixel_data[ipx]];
					pixel[0] = static_cast<float>(rgb[0]) / 255.0f;
					pixel[1] = static_cast<float>(rgb[1]) / 255.0f;
					pixel[2] = static_cast<float>(rgb[2]) / 255.0f;
					pixel[3] = 0;
				}
				else
				{
					for(unsigned i = 0; i < ncc; i++)
						pixel[i] = static_cast<float>(scanline->pixel_data[ipx * ncc + i]) / max_value;
				}
			}
			else if constexpr (bpc == 16)
			{
				for(unsigned i = 0; i < ncc; i++)
					pixel[i] = static_cast<float>(be16toh(reinterpret_cast<const u16_t*>(&scanline->pixel_data[0])[ipx * ncc + i])) / max_value;
			}

			if constexpr (color_type == EColorType::GRAYSCALE)
			{
				pixel[2] = pixel[1] = pixel[0];
				pixel[3] = 0;
			}
			else if constexpr (color_type == EColorType::GRAYSCALE_ALPHA)
			{
				pixel[3] = pixel[1];
				pixel[2] = pixel[1] = pixel[0];
			}
			else if constexpr (color_type == EColorType::INDEXED_COLOR)
			{
				// nothing to do
			}
			else
			{
				for(unsigned i = ncc; i < 4; i++)
					pixel[i] = 0;
			}
		}

		template<typename TSourceStream>
		const pixel_t* NextItem(TSourceStream* const source)
		{
			if(EL_UNLIKELY(ipx >= npx))
			{
				scanline = source->NextItem();
				if(scanline == nullptr)
					return nullptr;
				npx = scanline->pixel_data.Count() / ncc * bpc / 8;
				EL_ERROR(npx == 0, TException, "scanline has 0 pixels");
				ipx = 0;
			}

			UnpackPixel();
			ipx++;
			return &pixel;
		}

		TPixelDecoder(const TPLTEChunk* const plte) : plte(plte), scanline(nullptr), npx(0), ipx(1)
		{
			EL_ERROR(color_type == EColorType::INDEXED_COLOR && plte == nullptr, TInvalidArgumentException, "plte", "platte required");
		}
	};


	static TList<pixel_t> DecodePixels(array_t<byte_t> pixel_data, const usys_t sz_scanline, const EColorType color_type, const u8_t bit_depth, const TPLTEChunk* const plte, const u32_t width, const u32_t height)
	{
		#define PNG_DECODE_PIXEL_FORMAT(color_type_value, bit_depth_value)  \
			if(color_type == color_type_value && bit_depth == bit_depth_value) \
				return TScanlineReader(pixel_data, sz_scanline) \
					.Transform(TScanlineUnfilter<bit_depth_value, ComponentsPerPixel(color_type_value)>()) \
					.Transform(TPixelDecoder<color_type_value, bit_depth_value>(plte)) \
					.Collect(width * height);

		PNG_DECODE_PIXEL_FORMAT(EColorType::GRAYSCALE, 1);
		PNG_DECODE_PIXEL_FORMAT(EColorType::GRAYSCALE, 2);
		PNG_DECODE_PIXEL_FORMAT(EColorType::GRAYSCALE, 4);
		PNG_DECODE_PIXEL_FORMAT(EColorType::GRAYSCALE, 8);
		PNG_DECODE_PIXEL_FORMAT(EColorType::GRAYSCALE, 16);

		PNG_DECODE_PIXEL_FORMAT(EColorType::INDEXED_COLOR, 1);
		PNG_DECODE_PIXEL_FORMAT(EColorType::INDEXED_COLOR, 2);
		PNG_DECODE_PIXEL_FORMAT(EColorType::INDEXED_COLOR, 4);
		PNG_DECODE_PIXEL_FORMAT(EColorType::INDEXED_COLOR, 8);

		PNG_DECODE_PIXEL_FORMAT(EColorType::TRUECOLOR, 8);
		PNG_DECODE_PIXEL_FORMAT(EColorType::TRUECOLOR, 16);

		PNG_DECODE_PIXEL_FORMAT(EColorType::TRUECOLOR_ALPHA, 8);
		PNG_DECODE_PIXEL_FORMAT(EColorType::TRUECOLOR_ALPHA, 16);

		PNG_DECODE_PIXEL_FORMAT(EColorType::GRAYSCALE_ALPHA, 8);
		PNG_DECODE_PIXEL_FORMAT(EColorType::GRAYSCALE_ALPHA, 16);

		EL_THROW(TInvalidArgumentException, "color_type", "invalid color-type/bot-depth combination");

		#undef PNG_DECODE_PIXEL_FORMAT
	}

	TRasterImage LoadPNG(IBinarySource& stream)
	{
		using namespace io::compression::deflate;
		ReadAndVerifyMagic(stream);

		auto header = TChunkHeader::LoadHeader(stream);
		EL_ERROR(header.Type() != EChunkType::IHDR, TException, "first chunk in PNG file must be a IHDR");
		TIHDRChunk ihdr(stream, header.size);
		stream.Discard(4);	// eat CRC
		std::unique_ptr<TPLTEChunk> plte;
		TList<byte_t> data;

		for(;;)
		{
			header = TChunkHeader::LoadHeader(stream);
			if(header.Type() == EChunkType::IEND)
			{
				// eat CRC
				stream.Discard(4);
				break;
			}

			switch(header.Type())
			{
				case EChunkType::IHDR:
					EL_THROW(TException, "found another IHDR");

				case EChunkType::PLTE:
					EL_ERROR(plte != nullptr, TException, "found another PLTE");
					EL_ERROR(ihdr.color_type != EColorType::INDEXED_COLOR, TException, "platette provided for a color-type that doesn't use a palette");
					plte = New<TPLTEChunk>(stream, header.size, ihdr.bit_depth);
					break;

				case EChunkType::IDAT:
				{
					const usys_t idx_append = data.Count();
					data.SetCount(idx_append + header.size);
					stream.ReadAll(&data[idx_append], header.size);
					break;
				}

				default:
					// ignore
					stream.Discard(header.size);
					break;
			}

			// eat CRC
			stream.Discard(4);
		}

		EL_ERROR(ihdr.color_type == EColorType::INDEXED_COLOR && plte == nullptr, TException, "palette missing");

		data = Inflate(data);

		const unsigned ncc = ComponentsPerPixel(ihdr.color_type);
		const usys_t sz_scanline = 1 + (ihdr.width * ihdr.bit_depth * ncc  + 7) / 8;
		const usys_t sz_data = sz_scanline * ihdr.height;
		EL_ERROR(sz_data != data.Count(), TException, TString::Format("invalid scanline data size - expected %d bytes, but got %d bytes", sz_scanline, data.Count()));

		return TRasterImage({ihdr.width, ihdr.height}, DecodePixels(data, sz_scanline, ihdr.color_type, ihdr.bit_depth, plte.get(), ihdr.width, ihdr.height));
	}
}
