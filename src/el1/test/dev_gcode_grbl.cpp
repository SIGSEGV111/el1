#include <gtest/gtest.h>
#include <math.h>
#include <el1/error.hpp>
#include <el1/dev_gcode_grbl.hpp>
#include <el1/io_file.hpp>
#include "util.hpp"

using namespace ::testing;

namespace
{
	using namespace el1::dev::gcode::grbl;
	using namespace el1::io::file;
	using namespace el1::io::text::encoding::utf8;

	static parser_state_t InitState()
	{
		parser_state_t state;
		state.comment = U"";
		state.fr_rapid_xy = 2000;
		state.fr_rapid_z = 200;
		state.tool_change_pos = {0,0,0};
		state.pos = {0,0,0};
		state.fr_cmd = 0;
		state.fr_drill_return = 0;
		state.spindle_rpm = -1;
		state.idx_line = 0;
		state.spindle_dir = ERotation::CLOCKWISE;
		state.spindle_on = false;
		state.plane = EPlane::XY;
		state.unit = el1::dev::gcode::grbl::EUnit::METRIC;
		state.coord_mode = ECoordMode::ABSOLUTE;
		state.drill_return = EDrillReturn::PREVIOUS_Z;
		state.idx_wcs = 0;
		state.idx_tool = 0;
		state.eof = false;
		return state;
	}

	TEST(dev_gcode_grbl, TGrblParser_basics)
	{
		auto state = InitState();
		TGrblParser p(&state);

		auto cmd1 = p.ParseCommand(U"G00 X1 Y2 Z3 F10");
		TLinearMoveCommand& lmov1 = dynamic_cast<TLinearMoveCommand&>(*cmd1.get());
		EXPECT_EQ(lmov1.target[0], 1);
		EXPECT_EQ(lmov1.target[1], 2);
		EXPECT_EQ(lmov1.target[2], 3);
		EXPECT_EQ(lmov1.feedrate, 10);

		EXPECT_EQ(state.pos[0], 1);
		EXPECT_EQ(state.pos[1], 2);
		EXPECT_EQ(state.pos[2], 3);

		p.ParseCommand(U"G91");

		auto cmd2 = p.ParseCommand(U"G1 X1 Y-2 Z5 F100");
		TLinearMoveCommand& lmov2 = dynamic_cast<TLinearMoveCommand&>(*cmd2.get());
		EXPECT_EQ(lmov2.target[0], 2);
		EXPECT_EQ(lmov2.target[1], 0);
		EXPECT_EQ(lmov2.target[2], 8);
		EXPECT_EQ(lmov2.feedrate, 100);

		EXPECT_EQ(state.pos[0], 2);
		EXPECT_EQ(state.pos[1], 0);
		EXPECT_EQ(state.pos[2], 8);
	}

	TEST(dev_gcode_grbl, TGrblParser_FreeCAD_v1_0_0)
	{
		auto state = InitState();
		TFile input("testdata/freecad_v1_0_0.gcode");
		auto cmds = input.Pipe().Transform(TUTF8Decoder()).Transform(TLineReader()).Transform(TGrblParser(&state)).Collect();

		TString expected_x_string(U"114.121");
		TString expected_y_string(U"278.121");
		TString expected_z_string(U"4.000");
		TString expected_feed_string(U"750.000");
		EXPECT_EQ(state.pos[0], TDecimal(expected_x_string));
		EXPECT_EQ(state.pos[1], TDecimal(expected_y_string));
		EXPECT_EQ(state.pos[2], TDecimal(expected_z_string));
		EXPECT_EQ(state.fr_cmd, TDecimal(expected_feed_string));
	}

	TEST(dev_gcode_grbl, TGrblParser_ToString)
	{
		auto state = InitState();
		TGrblParser p(&state);
		auto cmd1 = p.ParseCommand(U"G0 X1.000 Y02.00 Z3 F10");
		TString cmd1_str = cmd1->ToString();
		EXPECT_EQ(cmd1_str, U"G01 X1 Y2 Z3 F10");
	}
}
