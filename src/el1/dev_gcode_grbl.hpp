#pragma once
#include "error.hpp"
#include "io_types.hpp"
#include "io_text.hpp"
// #include "io_text_parser.hpp"
#include "io_bcd.hpp"
#include "io_collection_list.hpp"
#include "io_collection_map.hpp"
#include "io_stream.hpp"
#include "math_vector.hpp"

namespace el1::dev::gcode::grbl
{
	using namespace io::stream;
	using namespace io::types;
	using namespace io::text;
	using namespace io::bcd;
	using namespace io::collection::list;
	using namespace io::collection::map;
	using namespace system::time;

	static const u8_t N_INTEGER = 6;
	static const u8_t N_DECIMAL = 6;

	/**
	 * @brief Arc interpolation plane selector.
	 * G17 = XY, G18 = ZX, G19 = YZ.
	 */
	enum class EPlane : u8_t
	{
		XY,
		ZX,
		YZ
	};

	/**
	 * @brief Unit selection.
	 * G21 = metric (mm), G20 = imperial (inches).
	 */
	enum class EUnit : u8_t
	{
		METRIC,
		IMPERIAL
	};

	/**
	 * @brief Coordinate mode.
	 * G90 = absolute, G91 = relative, G53 = machine absolute.
	 */
	enum class ECoordMode : u8_t
	{
		RELATIVE,
		ABSOLUTE,
		MACHINE
	};

	/**
	 * @brief Arc rotation direction.
	 * G2 = clockwise, G3 = counter-clockwise.
	 */
	enum class ERotation : u8_t
	{
		CLOCKWISE,
		COUNTER_CLOCKWISE
	};

	enum class EDrillReturn : u8_t
	{
		PREVIOUS_Z,
		R_LEVEL
	};

	struct TDecimal : TBCD
	{
		using TBCD::operator=;
		using TBCD::operator+;
		using TBCD::operator-;
		using TBCD::operator+=;
		using TBCD::operator-=;

		TDecimal(int v = 0);
		TDecimal(TString& str);
	};

	using TArgumentMap = TSortedMap<TUTF32, TDecimal>;
	struct machine_state_t;

	struct TDecimalVector : math::vector::TVector<TDecimal, 3>
	{
		TDecimalVector();
		TDecimalVector(const EUnit unit, const char* const keys, TArgumentMap& args);

		template<typename ... A>
		TDecimalVector(A ... a) : math::vector::TVector<TDecimal, 3>(a...) {}

		void UpdatePosition(machine_state_t& state);
	};

	/**
	 * @brief Current machine state snapshot.
	 */
	struct machine_state_t
	{
		TString comment;	///< Active comment, if applicable.
		TDecimalVector wcs[6];		///< Work coordinate offsets G54–G59.
		TDecimal fr_rapid_xy;
		TDecimal fr_rapid_z;
		TDecimalVector fr_accel;	///< Max acceleration per axis (mm/min²).
		TDecimalVector n_stp_mm;	///< Number of motor steps per mm.
		TDecimalVector tool_change_pos;	///< Tool change parking position.
		TDecimalVector pos;	///< Current machine position.
		TDecimal fr_cmd;	///< Current feedrate for G1 (mm/min).
		TDecimal fr_drill_return;	///< Feedrate for G98/G99 return (mm/min).
		TDecimal spindle_rpm;
		bool spindle_on;
		ERotation spindle_dir;
		EPlane plane;	///< Current arc plane (G17–G19).
		EUnit unit;		///< Current input unit (mm or inches).
		ECoordMode coord_mode;	///< Absolute, relative, or machine (G90/G91/G53).
		EDrillReturn drill_return;
		u8_t idx_wcs;	///< Active WCS index.
		u8_t idx_tool;	///< Active tool number.
	};

	/**
	 * @brief Base interface for all G-code commands.
	 */
	struct ICommand
	{
		virtual ~ICommand();
		virtual TString ToString() const = 0;
		static TArgumentMap ParseArgs(TList<TString>& fields, const usys_t idx_start, const char* const mandatory_args, const char* const optional_args);
	};

	/**
	 * @brief A macro consisting of multiple G-code commands.
	 */
	struct IMacroCommand : ICommand
	{
		TList<std::unique_ptr<ICommand>> commands;
		TString ToString() const final override;
	};

	/**
	 * @brief Base interface for motion commands with feedrate.
	 */
	struct IMoveCommand : ICommand
	{
		TDecimalVector target;	///< Target position in machine coordinates.
		TDecimal feedrate;	///< Feedrate in mm/min.

		virtual ~IMoveCommand() {}
	};

	/**
	 * @brief G0, G1 — Linear motion command.
	 */
	struct TLinearMoveCommand : IMoveCommand
	{
		TLinearMoveCommand(machine_state_t& state, TList<TString>& fields);
		TLinearMoveCommand(TDecimalVector target, TDecimal feedrate);
		TString ToString() const final override;
	};

	/**
	 * @brief G2, G3 — Arc motion command.
	 */
	struct TArcMoveCommand : IMoveCommand
	{
		TDecimalVector start;	// start position in machine coordinates.
		TDecimalVector center;	///< Arc center in machine coordinates.
		EPlane plane;	///< Plane of the arc.
		ERotation rot;	///< Arc direction.

		TArcMoveCommand(machine_state_t& state, TList<TString>& fields);
		TString ToString() const final override;
	};

	/**
	 * @brief G4 — Dwell for a given time.
	 */
	struct TDwellCommand : ICommand
	{
		TTime time;

		TDwellCommand(machine_state_t& state, TList<TString>& fields);
		TString ToString() const final override;
	};

	/**
	 * @brief G10 — Set work coordinate offset.
	 */
	struct TSetWorkCoordOffsetCommand : ICommand
	{
		TDecimalVector origin; ///< New WCS origin in machine coordinates.
		u8_t preset_index; ///< Index of WCS preset (G54=0 ... G59=5).

		TSetWorkCoordOffsetCommand(machine_state_t& state, TList<TString>& fields);
		TString ToString() const final override;
	};

	/**
	 * @brief G17, G18, G19 — Set active plane for arc operations.
	 */
	struct TPlaneSelectCommand : ICommand
	{
		EPlane plane;

		TPlaneSelectCommand(machine_state_t& state, TList<TString>& fields);
		TString ToString() const final override;
	};

	/**
	 * @brief G20, G21 — Set input unit system.
	 */
	struct TUnitSelectCommand : ICommand
	{
		EUnit unit;

		TUnitSelectCommand(machine_state_t& state, TList<TString>& fields);
		TString ToString() const final override;
	};

	/**
	 * @brief G38.x — Probe command (contact or clear).
	 */
	struct TProbeCommand : IMoveCommand
	{
		/*
			G38.2	Probe toward target, stop on contact (error if no contact).
			G38.3	Probe toward target, stop on contact (no error if no contact).
			G38.4	Probe away from target, stop on loss of contact (error if no loss).
			G38.5	Probe away from target, stop on loss of contact (no error if no loss).
		*/

		enum class ETrigger : u8_t
		{
			CONTACT,
			CLEAR
		};

		ETrigger trigger;
		bool error_on_miss; ///< True = alarm if probe not triggered.

		TProbeCommand(machine_state_t& state, TList<TString>& fields);
		TString ToString() const final override;
	};

	/**
	 * @brief G54–G59 — Select active work coordinate system.
	 */
	struct TSelectWorkCoordOffsetCommand : ICommand
	{
		u8_t idx_wcs;

		TSelectWorkCoordOffsetCommand(machine_state_t& state, TList<TString>& fields);
		TString ToString() const final override;
	};

	/**
	 * @brief G80 — Cancel active drilling or probing cycle.
	 */
	struct TCancelActiveCycleCommand : ICommand
	{
		TCancelActiveCycleCommand(machine_state_t& state, TList<TString>& fields);
		TString ToString() const final override;
	};

	/**
	 * @brief G81 — Simple drilling cycle (no peck).
	 */
	struct TDrillCommand : IMacroCommand
	{
		TDecimalVector drill_down_pos;	// final drill position into the hole
		TDecimal fr_down;	// feedrate for the downward motion
		TDecimal retract_height;

		TDrillCommand(machine_state_t& state, TList<TString>& fields);
	};

	/**
	 * @brief G90, G91 — Select absolute or relative positioning.
	 */
	struct TSelectCoordModeCommand : ICommand
	{
		ECoordMode mode;

		TSelectCoordModeCommand(machine_state_t& state, TList<TString>& fields);
		TString ToString() const final override;
	};

	/**
	 * @brief G98, G99 — Return mode for drill cycles.
	 */
	struct TSetDrillReturnCommand : ICommand
	{
		EDrillReturn drill_return;

		TSetDrillReturnCommand(machine_state_t& state, TList<TString>& fields);
		TString ToString() const final override;
	};

	/**
	 * @brief M0, M1 — Program pause (optional or required).
	 */
	struct TPauseCommand : ICommand
	{
		bool optional;

		TPauseCommand(machine_state_t& state, TList<TString>& fields);
		TPauseCommand(bool optional) : optional(optional) {}
		TString ToString() const final override;
	};

	/**
	 * @brief M2 — End of program.
	 */
	struct TEndOfProgramCommand : ICommand
	{
		TEndOfProgramCommand(machine_state_t& state, TList<TString>& fields);
		TString ToString() const final override;
	};

	/**
	 * @brief M3, M4, M5 — Spindle control.
	 * M3 = CW, M4 = CCW, M5 = Stop.
	 */
	struct TSpindelDirCommand : ICommand
	{
		ERotation dir;
		TDecimal rpm;
		bool spindle_on;

		TSpindelDirCommand(machine_state_t& state, TList<TString>& fields);
		TSpindelDirCommand(ERotation dir, TDecimal rpm, bool spindle_on);
		TString ToString() const final override;
	};

	/**
	 * @brief M6 — Tool change request.
	 */
	struct TToolChangeCommand : IMacroCommand
	{
		u32_t index; ///< Tool index as reported by FreeCAD (Tn).

		TToolChangeCommand(machine_state_t& state, TList<TString>& fields);
	};

	/**
	 * @brief Real-time command (non-G-code), e.g., ~, !, ?, Ctrl-X.
	 */
	struct TRealtimeCommand : ICommand
	{
		enum class ECode
		{
			RESUME,				///< ~ — Resume from feed hold.
			PAUSE,				///< ! — Feed hold.
			RESET,				///< Ctrl-X — Soft reset.
			REPORT_STATUS,		///< ? — Status report.
			CLEAR_ALARM,		///< Ctrl-C — Unlock alarm state.
			ENTER_SIMULATION,	///< Ctrl-D — Check mode (parse only).
			REPORT_GCODE_STATE,///< Ctrl-G — Report active G-code modes.
			HOME				///< Ctrl-L — Trigger homing cycle.
		};

		ECode code;

		TRealtimeCommand(machine_state_t& state, TList<TString>& fields);
		TString ToString() const final override;
	};

	struct TSetVariableCommand : ICommand, kv_pair_tt<u32_t, TDecimal>
	{
		TSetVariableCommand(machine_state_t& state, TList<TString>& fields);
		TString ToString() const final override;
	};

	/**
	 * @brief No-op command. Represents comments or M117 messages.
	 */
	struct TComment : ICommand
	{
		TString comment;
		TComment(TString comment);
		TString ToString() const final override;
	};

	/**
	 * @brief GRBL-compatible G-code parser.
	 * Requires each line to begin with Gxx, Mxx, or $var=.
	 */
	class TGrblParser
	{
		public:
			using TIn = TString;
			using TOut = std::unique_ptr<ICommand>;

			TList<std::unique_ptr<ICommand>> out_queue;
			TOut tmp;
			machine_state_t* state;
			unsigned idx_line;
			bool eof;

			std::unique_ptr<ICommand> ParseCommand(const TString& line);

			template<typename TSourceStream>
			std::unique_ptr<ICommand>* NextItem(TSourceStream* const source)
			{
				if(EL_UNLIKELY(out_queue.Count() > 0))
					return &(tmp = out_queue.PopHead());

				if(EL_UNLIKELY(eof))
					return nullptr;

				TString str_cmd;
				for(;;)
				{
					TString* const line = source->NextItem();
					if(EL_UNLIKELY(line == nullptr))
						return nullptr;

					idx_line++;
					line->Trim();
					if(EL_LIKELY(line->Length() > 0))
					{
						if(EL_UNLIKELY((*line)[0] == '('))
						{
							line->Cut(1,1);
							out_queue.MoveAppend(New<TComment, ICommand>(std::move(*line)));
						}
						else
						{
							if(EL_UNLIKELY(line->Contains(';')))
							{
								auto fields = line->Split(';', 2, false);
								for(auto f : fields) f.Trim();
								if(fields[0].Length() > 0)
								{
									if(fields.Count() > 1)
										out_queue.MoveAppend(New<TComment, ICommand>(std::move(fields[1])));
									str_cmd = std::move(fields[0]);
									break;
								}
							}
							else
							{
								str_cmd = std::move(*line);
								break;
							}
						}
					}
				}

				tmp = ParseCommand(str_cmd);

				if(EL_UNLIKELY(dynamic_cast<TEndOfProgramCommand*>(tmp.get()) != nullptr))
					eof = true;

				if(EL_LIKELY(out_queue.Count() == 0))
					return &tmp;
				out_queue.MoveAppend(std::move(tmp));
				return &(tmp = out_queue.PopHead());
			}

			TGrblParser(TGrblParser&&) = default;
			TGrblParser(const TGrblParser&) = delete;

			TGrblParser(machine_state_t* const state) : state(state), idx_line(0), eof(false)
			{
			}
	};
}


// static auto CreateParser()
	// {
	// 	auto digits_str = Repeat(CharRange('0','9'), 1, 4);
	// 	auto integer_str = (CharRange('1','9') + Optional(digits_str)) || '0'_P;
	// 	auto sign_str = CharList('+','-');
	// 	auto decimal = Translate([](TString sign, TString decimal_str) {
	// 		auto bcd = TBCD::FromString(decimal_str.Reverse(), DECIMAL_SYMBOLS);
	// 		bcd.IsNegative(sign == "-");
	// 		return bcd;
	// 	}, Optional(sign_str), integer_str + Optional('.'_P + digits_str));
 //
	// 	auto space = Discard(OneOrMore(' '_P));
	// 	auto arg = Translate<arg_t>(space + CharList('X', 'Y', 'Z', 'I', 'J', 'K', 'F', 'P', 'R', 'Q'), decimal);
	// 	auto args = Translate([](TList<arg_t> args){ args.Sort(ESortOrder::ASCENDING, [](auto a, auto b) { if(a.key.code > b.key.code) return 1; if(b.key .code> a.key.code) return -1; return 0; }); return TSortedMap<TUTF32, TDecimal>(args); }, OneOrMore(arg));
 //
	// 	auto linear_move = TranslateCast<TLinearMoveCommand, ICommand>(("G0"_P || "G1"_P), args);
	// 	auto arc_move = TranslateCast<TArcMoveCommand, ICommand>(("G2"_P || "G3"_P), args);
	// 	auto dwell = TranslateCast<TDwellCommand, ICommand>("G4"_P, arg);
	// 	auto set_wcs = TranslateCast<TSetWorkCoordOffsetCommand, ICommand>("G10"_P, "L2"_P || "L20"_P, args);
	// 	auto select_plane = TranslateCast<TPlaneSelectCommand, ICommand>("G17"_P || "G18"_P || "G19"_P);
	// 	auto select_unit = TranslateCast<TUnitSelectCommand, ICommand>("G20"_P || "G21"_P);
	// 	auto probe = TranslateCast<TProbeCommand, ICommand>("G38."_P + CharRange('2','5'), args);
	// 	auto select_wcs = TranslateCast<TSelectWorkCoordOffsetCommand, ICommand>("G5"_P + CharRange('4','9'));
	// 	auto cancle_cycle = TranslateCast<TCancelActiveCycleCommand, ICommand>("G80"_P);
	// 	auto drill = TranslateCast<TDrillCommand, ICommand>("G81"_P || "G83"_P, args);
	// 	auto select_coord_mode = TranslateCast<TSelectCoordModeCommand, ICommand>("G90"_P || "G91"_P);
	// 	auto set_drill_return = TranslateCast<TSetDrillReturnCommand, ICommand>("G98"_P || "G99"_P);
	// 	auto pause = TranslateCast<TPauseCommand, ICommand>("M0"_P || "M1"_P);
	// 	auto eop = TranslateCast<TEndOfProgramCommand, ICommand>("M2"_P);
	// 	auto spindle = TranslateCast<TSpindelDirCommand, ICommand>("M3"_P || "M4"_P || "M5"_P);
	// 	auto tool_change = TranslateCast<TToolChangeCommand, ICommand>("M6"_P);
	// 	auto rt = TranslateCast<TRealtimeCommand, ICommand>(CharList('~', '!', '?', '\x18', '\x03', '\x07', '\x0B'));
	// 	auto set_var = TranslateCast<TSetVariableCommand, ICommand>('$'_P, digits_str, '='_P, decimal);
 //
	// 	return linear_move || arc_move || dwell || set_wcs || select_plane || select_unit || probe || select_wcs || cancle_cycle || drill || select_coord_mode || set_drill_return || pause || eop || spindle || tool_change || rt || set_var;
	// }
