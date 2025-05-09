#include "dev_gcode_grbl.hpp"
#include "io_text_encoding_utf8.hpp"
#include "io_text_string.hpp"
#include <math.h>

namespace el1::dev::gcode::grbl
{

	ICommand::~ICommand()
	{
	}

	TDecimal::TDecimal(int v) : TBCD(v, 10, N_INTEGER, N_DECIMAL)
	{
	}

	TDecimal::TDecimal(TString& str) : TDecimal()
	{
		try
		{
			str.Reverse();
			*this = TBCD::FromString(str, DECIMAL_SYMBOLS, '.', '-', '+', false);
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format("while parsing decimal value %q", str));
		}
	}

	TDecimalVector::TDecimalVector()
	{
	}

	TDecimalVector::TDecimalVector(const EUnit unit, const char* const keys, TArgumentMap& args)
	{
		static const auto inch_mul = TBCD(25.4, 10, N_INTEGER, N_DECIMAL);
		for(unsigned i = 0; keys[i] != 0; i++)
		{
			TDecimal* const a = args.Get(keys[i]);
			TDecimal& value = v[i];
			if(a == nullptr)
			{
				value.SetZero();
				value.IsNegative(true);
			}
			else
			{
				value = *a;
				if(unit != EUnit::METRIC)
					value *= inch_mul;
			}
		}
	}

	void TDecimalVector::UpdatePosition(parser_state_t& state)
	{
		const TDecimalVector& ref = state.coord_mode == ECoordMode::ABSOLUTE ? state.wcs[state.idx_wcs] : (state.coord_mode == ECoordMode::RELATIVE ? state.pos : TDecimalVector());

		for(unsigned i = 0; i < 3; i++)
			if(v[i].IsNegative() && v[i].IsZero())
				v[i] = state.pos[i];
			else
				v[i] += ref[i];

		state.pos = *this;
	}

	TArgumentMap ICommand::ParseArgs(TList<TString>& fields, const usys_t idx_start, const char* const mandatory_args, const char* const optional_args)
	{
		TUTF32 key = 0U;
		try
		{
			TArgumentMap m;
			for(usys_t i = 1; i < fields.Count(); i++)
			{
				key = fields[i][0];
				bool known = false;
				for(const char* p = optional_args; *p; p++)
					if(key == *p)
					{
						known = true;
						break;
					}

				if(!known)
					for(const char* p = mandatory_args; *p; p++)
						if(key == *p)
						{
							known = true;
							break;
						}

				EL_ERROR(!known, TException, TString::Format("unknown argument key %q in field #%d (%q)", key, i, fields[i]));

				fields[i].Cut(1,0);
				m.Add(key, TDecimal(fields[i]));
			}

			for(const char* p = mandatory_args; *p; p++)
				EL_ERROR(!m.Contains(*p), TException, TString::Format("argument %q is mandatory", *p));

			return m;
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format("while processing argument %q to command %q", key, fields[0]));
		}
	}

	TString IMacroCommand::ToString() const
	{
		return TString::Join(commands.Pipe().Map([](auto& cmd) { return cmd->ToString(); }).Collect(), "\n");
	}

	TLinearMoveCommand::TLinearMoveCommand(parser_state_t& state, TList<TString>& fields)
	{
		auto args = ParseArgs(fields, 1, "", "XYZF");
		target = TDecimalVector(state.unit, "XYZ", args);

		const bool fast = fields[0] == "G0" || fields[0] == "G00";
		const bool has_xy = !target[0].IsZero() || !target[1].IsZero();
		const bool has_z = !target[2].IsZero();
		const TDecimal& fr_max = has_xy && has_z ? util::Min(state.fr_rapid_xy, state.fr_rapid_z) : (has_xy ? state.fr_rapid_xy : state.fr_rapid_z);

		target.UpdatePosition(state);
		feedrate = args.GetWithDefault('F', fast ? fr_max : state.fr_cmd);
		if(!fast)
			state.fr_cmd = feedrate;
	}

	TLinearMoveCommand::TLinearMoveCommand(TDecimalVector _target, TDecimal _feedrate)
	{
		target = std::move(_target);
		feedrate = std::move(_feedrate);
	}

	TString TLinearMoveCommand::ToString() const
	{
		return TString::Format("G01 X%d Y%d Z%d F%d", target[0], target[1], target[2], feedrate);
	}

	TArcMoveCommand::TArcMoveCommand(parser_state_t& state, TList<TString>& fields)
	{
		auto args = ParseArgs(fields, 1, "", "XYZFIJK");
		start = state.pos;
		target = TDecimalVector(state.unit, "XYZ", args);
		feedrate = state.fr_cmd = args.GetWithDefault('F', state.fr_cmd);
		center = TDecimalVector(state.unit, "IJK", args) + state.pos;
		plane = state.plane;
		rot = MatchStringList(fields[0], "G2", "G02") ? ERotation::CLOCKWISE : ERotation::COUNTER_CLOCKWISE;
		target.UpdatePosition(state);
	}

	TString TArcMoveCommand::ToString() const
	{
		const TDecimalVector rel_center = center - start;
		return TString::Format("G03 X%d Y%d Z%d I%d J%d K%d F%d", target[0], target[1], target[2], rel_center[0], rel_center[1], rel_center[2], feedrate);
	}

	TDwellCommand::TDwellCommand(parser_state_t& state, TList<TString>& fields)
	{
		auto args = ParseArgs(fields, 1, "P", "");
		time = args['P'].ToDouble() / 1000.0;
	}

	TString TDwellCommand::ToString() const
	{
		return TString::Format("G04 P%d", time.ConvertToI(system::time::EUnit::MILLISECONDS));
	}

	TSetWorkCoordOffsetCommand::TSetWorkCoordOffsetCommand(parser_state_t& state, TList<TString>& fields)
	{
		auto args = ParseArgs(fields, 1, "LP", "XYZ");
		preset_index = (u8_t)args['P'].ToUnsignedInt();
		origin = TDecimalVector(state.unit, "XYZ", args);
		const int ref = args['L'].ToSignedInt();
		EL_ERROR(ref != 2 && ref != 20, TException, TString::Format("unknown position reference '%d'", ref));
		if(ref == 20)
			origin += state.pos;
		state.wcs[preset_index] = origin;
	}

	TString TSetWorkCoordOffsetCommand::ToString() const
	{
		return TString::Format("G10 L2 P%d X%d Y%d Z%d", preset_index, origin[0], origin[1], origin[2]);
	}

	TPlaneSelectCommand::TPlaneSelectCommand(parser_state_t& state, TList<TString>& fields)
	{
		if(fields[0] == "G17")
			plane = EPlane::XY;
		else if(fields[0] == "G18")
			plane = EPlane::ZX;
		else if(fields[0] == "G19")
			plane = EPlane::YZ;
		else
			EL_THROW(TException, "invalid plane seclect command");
		state.plane = plane;
	}

	TString TPlaneSelectCommand::ToString() const
	{
		switch(plane)
		{
			case EPlane::XY: return "G17";
			case EPlane::ZX: return "G18";
			case EPlane::YZ: return "G19";
		}
		EL_THROW(TLogicException);
	}

	TUnitSelectCommand::TUnitSelectCommand(parser_state_t& state, TList<TString>& fields)
	{
		if(fields[0] == "G20")
			unit = EUnit::IMPERIAL;
		else if(fields[0] == "G21")
			unit = EUnit::METRIC;
		else
			EL_THROW(TException, "invalid unit seclect command");
		state.unit = unit;
	}

	TString TUnitSelectCommand::ToString() const
	{
		return TString();
	}

	TProbeCommand::TProbeCommand(parser_state_t& state, TList<TString>& fields)
	{
		trigger = MatchStringList(fields[0], "G38.2", "G38.3") ? ETrigger::CONTACT : ETrigger::CLEAR;
		error_on_miss = MatchStringList(fields[0], "G38.2", "G38.4");
	}

	TString TProbeCommand::ToString() const
	{
		const char* code;
		if(trigger == ETrigger::CONTACT && error_on_miss)
			code = "G38.2";
		else if(trigger == ETrigger::CONTACT && !error_on_miss)
			code = "G38.3";
		else if(trigger != ETrigger::CONTACT && error_on_miss)
			code = "G38.4";
		else //if(trigger != ETrigger::CONTACT && !error_on_miss)
			code = "G38.5";

		return TString::Format("%s X%d Y%d Z%d F%d", code, target[0], target[1], target[2], feedrate);
	}

	TSelectWorkCoordOffsetCommand::TSelectWorkCoordOffsetCommand(parser_state_t& state, TList<TString>& fields)
	{
		fields[0].Cut(1,0);
		idx_wcs = fields[0].ToInteger() - 54;
		EL_ERROR(idx_wcs >= 6, TException, "invalid WCS index");
		state.idx_wcs = idx_wcs;
	}

	TString TSelectWorkCoordOffsetCommand::ToString() const
	{
		return TString();
	}

	TCancelActiveCycleCommand::TCancelActiveCycleCommand(parser_state_t& state, TList<TString>& fields)
	{
		// nothing do do
	}

	TString TCancelActiveCycleCommand::ToString() const
	{
		return TString();
	}

	TDrillCommand::TDrillCommand(parser_state_t& state, TList<TString>& fields)
	{
		auto args = ParseArgs(fields, 1, "", "XYZFR");
		drill_down_pos = TDecimalVector(state.unit, "XYZ", args);
		fr_down = state.fr_cmd = args.GetWithDefault('F', state.fr_cmd);
		retract_height = state.drill_return == EDrillReturn::R_LEVEL ? args['R'] : state.pos[2];

		commands.MoveAppend(New<TLinearMoveCommand, ICommand>(
			TDecimalVector(drill_down_pos[0], drill_down_pos[1], retract_height),
			state.fr_rapid_xy
		));

		commands.MoveAppend(New<TLinearMoveCommand, ICommand>(
			TDecimalVector(drill_down_pos[0], drill_down_pos[1], drill_down_pos[2]),
			fr_down
		));

		commands.MoveAppend(New<TLinearMoveCommand, ICommand>(
			TDecimalVector(drill_down_pos[0], drill_down_pos[1], retract_height),
			state.fr_rapid_z
		));
	}

	TSelectCoordModeCommand::TSelectCoordModeCommand(parser_state_t& state, TList<TString>& fields)
	{
		state.coord_mode = mode = fields[0] == "G90" ? ECoordMode::ABSOLUTE : ECoordMode::RELATIVE;
	}

	TString TSelectCoordModeCommand::ToString() const
	{
		return TString();
	}

	TSetDrillReturnCommand::TSetDrillReturnCommand(parser_state_t& state, TList<TString>& fields)
	{
		state.drill_return = drill_return = fields[0] == "G98" ? EDrillReturn::R_LEVEL : EDrillReturn::PREVIOUS_Z;
	}

	TString TSetDrillReturnCommand::ToString() const
	{
		return TString();
	}

	TPauseCommand::TPauseCommand(parser_state_t& state, TList<TString>& fields)
	{
		optional = MatchStringList(fields[0], "M0", "M00");
	}

	TString TPauseCommand::ToString() const
	{
		return optional ? "M00" : "M01";
	}

	TEndOfProgramCommand::TEndOfProgramCommand(parser_state_t& state, TList<TString>& fields)
	{
		// nothing to do
	}

	TString TEndOfProgramCommand::ToString() const
	{
		return "M02";
	}

	TSpindelDirCommand::TSpindelDirCommand(parser_state_t& state, TList<TString>& fields)
	{
		if(MatchStringList(fields[0], "M5", "M05"))
		{
			auto args = ParseArgs(fields, 1, "", "");
			rpm = -1;
			state.spindle_on = spindle_on = false;
		}
		else if(MatchStringList(fields[0], "M3", "M03", "M4", "M04"))
		{
			auto args = ParseArgs(fields, 1, "", "S");
			rpm = state.spindle_rpm = args.GetWithDefault('S', state.spindle_rpm);
			EL_ERROR(rpm.IsNegative(), TException, "spindle start requested, but RPM not set");
			state.spindle_dir = dir = MatchStringList(fields[0], "M3", "M03") ? ERotation::CLOCKWISE : ERotation::COUNTER_CLOCKWISE;
			state.spindle_on = spindle_on = true;
		}
		else
			EL_THROW(TException, TString::Format("unknown spindle command %q", fields[0]));
	}

	TSpindelDirCommand::TSpindelDirCommand(ERotation dir, TDecimal _rpm, bool spindle_on) : dir(dir), rpm(std::move(_rpm)), spindle_on(spindle_on)
	{
	}

	TString TSpindelDirCommand::ToString() const
	{
		if(spindle_on)
			return TString::Format("%s S%d", dir == ERotation::CLOCKWISE ? "M03" : "M04", rpm);
		else
			return "M05";
	}

	TToolChangeCommand::TToolChangeCommand(parser_state_t& state, TList<TString>& fields)
	{
		auto args = ParseArgs(fields, 1, "", "T");

		TDecimalVector return_pos = state.pos;

		if(state.tool_change_pos[2] > state.pos[2])
		{
			return_pos[2] = state.tool_change_pos[2];
			commands.MoveAppend(New<TLinearMoveCommand, ICommand>(
				TDecimalVector(state.pos[0], state.pos[1], state.tool_change_pos[2]),
				state.fr_rapid_z
			));
		}

		commands.MoveAppend(New<TSpindelDirCommand, ICommand>(state.spindle_dir, state.spindle_rpm, false));
		commands.MoveAppend(New<TLinearMoveCommand, ICommand>(
			state.tool_change_pos,
			state.fr_rapid_xy
		));

		// during the pause the operator will have to manually change the tool and perform a Z-probe if required
		commands.MoveAppend(New<TPauseCommand, ICommand>(false));
		commands.MoveAppend(New<TSpindelDirCommand, ICommand>(state.spindle_dir, state.spindle_rpm, true));
		commands.MoveAppend(New<TLinearMoveCommand, ICommand>(
			return_pos,
			state.fr_rapid_xy
		));
	}

	TRealtimeCommand::TRealtimeCommand(parser_state_t& state, TList<TString>& fields)
	{
		EL_NOT_IMPLEMENTED;
	}

	TString TRealtimeCommand::ToString() const
	{
		EL_NOT_IMPLEMENTED;
	}

	TSetVariableCommand::TSetVariableCommand(parser_state_t& state, TList<TString>& fields)
	{
		EL_NOT_IMPLEMENTED;
	}

	TString TSetVariableCommand::ToString() const
	{
		return TString::Format("$%d=%d", key, value);
	}

	TComment::TComment(TString _comment) : comment(std::move(_comment))
	{
	}

	TString TComment::ToString() const
	{
		return TString::Format(";%s", comment);
	}

	std::unique_ptr<ICommand> TGrblParser::ParseCommand(const TString& line)
	{
		try
		{
			auto fields = line.Split(' ', NEG1, true);
			TString& str_cmd = fields[0];
			str_cmd.ToUpper();

			// hack to support FreeCADs broken gcode generator
			if(str_cmd[0] == 'T' && str_cmd[1] >= '0' && str_cmd[1] <= '9' && (fields[1] == "M4" || fields[1] == "M04"))
			{
				fields[1] = str_cmd;
				str_cmd = "M04";
			}

			if(MatchStringList(str_cmd, "G0", "G00", "G1", "G01"))
			{
				return New<TLinearMoveCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "G2", "G02", "G2", "G02"))
			{
				return New<TArcMoveCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "G4", "G04"))
			{
				return New<TDwellCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "G10"))
			{
				return New<TSetWorkCoordOffsetCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "G17", "G18", "G19"))
			{
				return New<TPlaneSelectCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "G20", "G21"))
			{
				return New<TUnitSelectCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "G38.2", "G38.3", "G38.4", "G38.5"))
			{
				return New<TProbeCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "G54", "G55", "G56", "G57", "G58", "G59"))
			{
				return New<TSelectWorkCoordOffsetCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "G80"))
			{
				return New<TCancelActiveCycleCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "G81"))
			{
				return New<TDrillCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "G90", "G91"))
			{
				return New<TSelectCoordModeCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "G98", "G99"))
			{
				return New<TSetDrillReturnCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "M0", "M00", "M1", "M01"))
			{
				return New<TPauseCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "M2", "M02"))
			{
				return New<TEndOfProgramCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "M3", "M03", "M4", "M04", "M5", "M05"))
			{
				return New<TSpindelDirCommand, ICommand>(*state, fields);
			}
			else if(MatchStringList(str_cmd, "M6", "M06"))
			{
				return New<TToolChangeCommand, ICommand>(*state, fields);
			}
			else
				EL_THROW(TException, TString::Format("unknown g-code %q in line %d", str_cmd, state->idx_line));
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format("while processing line %d", state->idx_line));
		}
	}
}
