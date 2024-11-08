#include "io_graphics_image_format_gerber.hpp"
#include "io_text_encoding_utf8.hpp"

namespace el1::io::graphics::format::gerber
{
	using namespace io::text::string;
	using namespace io::text::encoding::utf8;

	const TString& operator>>(const TString& str, v2d_t& v)
	{
		EL_NOT_IMPLEMENTED;
		return str;
	}

	TVectorImage LoadGerber(IBinarySource& src)
	{
		TVectorImage img;
		double unit_convert = 0.0;
		v2d_t pos(0,0);
		TUTF32 zeros;
		TUTF32 polarity;
		TUTF32 coord_mode;
		bool end = false;
		u8_t nix = 0;
		u8_t ndx = 0;
		u8_t niy = 0;
		u8_t ndy = 0;

		src.Pipe()
		.Transform(TUTF8Decoder())
		.Transform(TLineReader())
		.Filter([](auto& s){ return s.Length() > 0; })
		.ForEach([&](auto& line){
			if(end) return;

			if(line.BeginsWith("%LP"))
			{
				line.Parse("%%LP%c*%%", polarity);
				EL_ERROR(polarity != 'C' && polarity != 'D', TException, TString::Format("invalid/unknown layer polarity %q in %q", polarity, line));
			}
			else if(line.BeginsWith("%MO"))
			{
				if(line == "%MOMM*%")
					unit_convert = 1.0;
				else if(line == "%MOIN*%")
					unit_convert = 25.4;
				else
					EL_THROW(TException, TString::Format("unknown unit of measure %q", line));
			}
			else if(line.BeginsWith("%FS"))
			{
				// format statement
				unsigned fx, fy;
				line.Parse("%%FS%c%cX%dY%d*%%", zeros, coord_mode, fx, fy);
				EL_ERROR(zeros != 'L' && zeros != 'T', TException, TString::Format("invalid/unknown zero handling mode %q in format statement %q", zeros, line));
				EL_ERROR(coord_mode != 'A' && coord_mode != 'I', TException, TString::Format("invalid/unknown coordinate mode %q in format statement %q", coord_mode, line));
				EL_ERROR(fx < 10 || fy < 10 || fx > 99 || fy > 99, TException, TString::Format("invalid X/Y-axis format (fx=%d,fy=%d) in format statement %q", fx, fy, line));
				nix = (u8_t)(fx / 10U);
				ndx = (u8_t)(fx % 10U);
				niy = (u8_t)(fy / 10U);
				ndy = (u8_t)(fy % 10U);
			}
			else if(line.BeginsWith("%ADD"))
			{
				// aperture definition
				unsigned code;
				TUTF32 type;
				TString spec;
				line.Parse("%%ADD%d%c,%s*%%", code, type, spec);
				EL_ERROR(type != 'C' && type != 'R' && type != 'O' && type != 'P', TException, TString::Format("unknown shape type %q in aperture definition %q", type, line));
				EL_NOT_IMPLEMENTED;
			}
			else if(line.BeginsWith("%AM"))
			{
				// macro def (polygon definition)
			}
			else if(line.BeginsWith("M02*"))
			{
				// EOF
				end = true;
			}
			else
			{
				if(line.BeginsWith("G01"))
				{
					// linear interpolated move
				}
				else if(line.BeginsWith("G02"))
				{
					// clockwise circular move
				}
				else if(line.BeginsWith("G03"))
				{
					// counterclockwise circular move
				}
				else if(line.BeginsWith("G04"))
				{
					// comment
				}
				else if(line.BeginsWith("G75"))
				{
					// quadrant mode for circular interpolation
				}

				if(line.EndsWith("D01"))
				{
					// move while drawing
				}
				else if(line.EndsWith("D02"))
				{
					// move without drawing
				}
				else if(line.EndsWith("D03"))
				{
					// flash at current position
				}
			}
		});

		return img;
	}
}
