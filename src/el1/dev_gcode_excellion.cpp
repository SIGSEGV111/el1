#include "dev_gcode_excellion.hpp"
#include "io_text_encoding_utf8.hpp"
#include "io_text_string.hpp"
#include <math.h>

namespace el1::dev::gcode::excellion
{
	using namespace io::text::string;
	using namespace io::text::encoding::utf8;

	TExcellionFile::TExcellionFile()
	{
		// nothing to do
	}

	TExcellionFile::TExcellionFile(IBinarySource& src)
	{
		double unit_convert = 0.0;
		bool header = true;
		bool end = false;
		s64_t active_tool = NEG1;

		src.Pipe()
		.Transform(TUTF8Decoder())
		.Transform(TLineReader())
		.Map([](auto& s){ return s.Split(';', 2)[0].Trim(); })
		.Filter([](auto& s){ return s.Length() > 0; })
		.ForEach( [&](auto& line) {
			if(end) return;
			if(header)
			{
				if(line.BeginsWith("METRIC") || line.BeginsWith("INCH"))
				{
					auto tokens = line.Split(',');
					if(tokens[0] == "METRIC")
						unit_convert = 1.0f;
					else //if(token[0] == "INCH")
						unit_convert = 25.4f;

					if(tokens.Count() > 1)
					{
						auto power = tokens[-1].Split('.', 2)[1].Length();
						auto scale = pow(10.0, power);
						unit_convert /= scale;
					}
				}
				else if(line == "%")
					header = false;
				else if(line == "M48")
					; // start of program
				else if(line.BeginsWith("T"))
				{
					// tool definition
					auto tokens = line.Split('C', 2);
					auto index = tokens[0].SliceSL(1).ToInteger();
					auto dia = tokens[1].ToDouble();
					toolset.Add(index, {dia});
				}
				else
					EL_THROW(TException, TString::Format("unknown header directive %q", line));
			}
			else
			{
				if(line == "G90")
					; // abspos mode
				else if(line == "G05")
					; // pause - ignored
				else if(line == "G91")
					EL_THROW(TException, "relative position mode not supported");
				else if(line.BeginsWith("T"))
				{
					// tool select
					active_tool = line.SliceSL(1).ToInteger();

				}
				else if(line.BeginsWith("X"))
				{
					// hole pos
					auto sposs = line.SliceSL(1).Split('Y');
					auto x = sposs[0].ToDouble() * unit_convert;
					auto y = sposs[1].ToDouble() * unit_convert;
					EL_ERROR(active_tool < 0, TException, "encountered drill command, but no tool selected yet");
					EL_ERROR(unit_convert == 0.0, TException, "encountered drill command, but unit/decimal format not defined yet");
					auto& tool = toolset[active_tool];
					holes.Append({{x,y}, tool.diameter, (u32_t)active_tool});
				}
				else if(line == "M30")
					end = true;
				else
					EL_THROW(TException, TString::Format("unknown body directive %q", line));
			}
		});
	}

	u32_t TExcellionFile::SelectTool(const double diameter) const
	{
		u32_t idx_best = toolset.Items()[0].key;
		const TTool* best = &toolset.Items()[0].value;

		for(auto& kv : toolset.Items())
			if(util::Abs(kv.value.diameter - diameter) < util::Abs(best->diameter - diameter))
			{
				idx_best = kv.key;
				best = &kv.value;
			}

		return idx_best;
	}

	void TExcellionFile::ReplaceToolset(TSortedMap<u32_t, TTool> new_toolset)
	{
		toolset = std::move(new_toolset);
		for(auto& hole : holes)
			hole.tool_index = SelectTool(hole.diameter);
	}

	void TExcellionFile::ConvertToGrbl(ITextWriter& tw, const float z_toolchange, const float z_hole2hole, const float z_drill, const float fr_travel, const float fr_drill, const float fr_retract) const
	{
		string::TNumberFormatter nf = string::TNumberFormatter::PLAIN_DECIMAL_US_EN;
		nf.config.n_decimal_places = 6;
		nf.config.n_min_integer_places = 1;
		tw<<&nf;

		for(auto& kv : toolset.Items())
			if(holes.Pipe().Filter([&kv](auto& hole){ return hole.tool_index == kv.key; }).Count() > 0)
			{
				tw<<"G1 Z"<<z_toolchange<<" F"<<fr_retract<<"\n";
				tw<<"S0\n";
				tw<<"G1 X0 Y0 F"<<fr_travel<<"\n";
				tw<<"M0 change tool to drill bit with diameter "<<kv.value.diameter<<"mm\n";;

				for(auto& hole : holes)
					if(hole.tool_index == kv.key)
					{
						tw<<"G1 X"<<hole.pos[0]<<" Y"<<hole.pos[1]<<" F"<<fr_travel<<"\n";
						tw<<"G1 Z0.25 F"<<fr_retract<<"\n";
						tw<<"G1 Z"<<z_drill<<" F"<<fr_drill<<"\n";
						tw<<"G1 Z"<<z_hole2hole<<" F"<<fr_retract<<"\n";
					}
			}

		tw<<&string::TNumberFormatter::PLAIN_DECIMAL_US_EN;
	}
}
