#include "dev_gcode.hpp"

namespace el1::dev::gcode
{
	static ITextWriter& operator<<(ITextWriter& tw, const v2f_t v)
	{
		tw<<"X"<<v[0]<<" Y"<<v[1];
		return tw;
	}

	void GenerateForLaserEngraver(const TJob& job, ITextWriter& tw, const float fr_min, const float fr_max, const float fr_travel, const float fr_min_grbl, const float fr_max_grbl, const float laser_scale, const float spmm)
	{
		string::TNumberFormatter nf = string::TNumberFormatter::PLAIN_DECIMAL_US_EN;
		nf.config.n_decimal_places = 6;
		nf.config.n_min_integer_places = 1;
		tw<<&nf;

		bool baby_step = false;
		v2f_t pos(0,0);
		float fr_laser_on = 0;
		const float* fr_current = &fr_travel;
		tw<<"G21\nG90\nT0\nM3 S0\n";	// mm, abs pos, tool 0, spindel on with speed 0

		for(auto& cmdptr : job.commands)
		{
			const IPlotterCommand* const cmd = cmdptr.get();
			if(const TGotoCommand* cmd_goto = dynamic_cast<const TGotoCommand*>(cmd))
			{
				if(baby_step)
				{
					EL_NOT_IMPLEMENTED;
				}
				else
				{
					pos = (cmd_goto->ctype == ECoordinateType::ABSOLUTE) ? cmd_goto->endpoint : (pos + cmd_goto->endpoint);
					tw<<"G1 "<<pos<<" F"<<*fr_current<<"\n";
				}
			}
			else if(const TArcMoveCommand* cmd_arc = dynamic_cast<const TArcMoveCommand*>(cmd))
			{
				if(baby_step)
				{
					EL_NOT_IMPLEMENTED;
				}
				else
				{
					pos = (cmd_arc->ctype == ECoordinateType::ABSOLUTE) ? cmd_arc->arc.endpoint : (pos + cmd_arc->arc.endpoint);
					const char* const cmd = (cmd_arc->arc.dir == TArc::EDirection::CLOCKWISE) ? "G2 " : "G3 ";
					tw<<cmd<<pos<<" I"<<cmd_arc->arc.center[0]<<" J"<<cmd_arc->arc.center[0]<<" F"<<*fr_current<<"\n";
				}
			}
			else if(const TPenCommand* cmd_pen = dynamic_cast<const TPenCommand*>(cmd))
			{
				const bool pen_down = cmd_pen->dir == TPenCommand::EDirection::DOWN;
				fr_current = pen_down ? &fr_laser_on : &fr_travel;
			}
			else if(const TToolChangeCommand* cmd_tc = dynamic_cast<const TToolChangeCommand*>(cmd))
			{
				EL_THROW(TInvalidArgumentException, "job", "job contains tool-change commands");
			}
			else if(const TColorChangeCommand* cmd_cc = dynamic_cast<const TColorChangeCommand*>(cmd))
			{
				float power = (cmd_cc->new_color[0] + cmd_cc->new_color[1] + cmd_cc->new_color[2]) / 3.0f;
				fr_laser_on = fr_min + (1.0f - power) * (fr_max - fr_min);
				baby_step = false;

				if(fr_laser_on > fr_max_grbl)
				{
					// we want to go faster than allowed, reduce laser power and clip to fr_max_grbl
					power *= (fr_max_grbl / fr_laser_on);
					fr_laser_on = fr_max_grbl;
				}
				else if(fr_laser_on < fr_min_grbl)
				{
					// we want to go slower than allowed, switch to baby-step mode
					baby_step = true;
				}

				tw<<"S"<<(power * laser_scale)<<"\n";
			}
			else if(const TDelayCommand* cmd_delay = dynamic_cast<const TDelayCommand*>(cmd))
			{
				// ignored
			}
			else
				EL_THROW(TInvalidArgumentException, "job", "job contains unknown commands");
		}

		tw<<"M5\n";
	}
}
