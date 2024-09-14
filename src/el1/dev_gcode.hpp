#pragma once
#include "io_graphics_plotter.hpp"
#include "io_text.hpp"

namespace el1::dev::gcode
{
	using namespace io::graphics::plotter;
	using namespace io::text;

	/**
	* @brief Generates G-code for a GRBL-powered laser engraver.
	*
	* All color vaules are converted to grayscale (`v=(r+g+b)/3`).
	* Alpha channel is ignored.
	* No tool change commands are issued.
	* The plotter commands must use TColorChangeCommands. TToolChangeCommands won't work.
	* By default the laser power is in the range 0.0 to 1.0 - but see `laser_scale`.
	* Unless we hit a machine limit, the laser power is kept at its maximum value and instead
	* the feedrate is varied to control the burn.
	*
	* @param job The plotter job containing the series of plotter commands to be executed.
	* @param tw The output stream for writing the generated G-code.
	* @param fr_min The minimum feed rate, corresponding to the deepest burn (where color intensity = 1.0f).
	* @param fr_max The maximum feed rate, corresponding to the lightest burn (where color intensity = 0.0f).
	* @param fr_travel The feed rate used for non-engraving moves (laser off).
	* @param fr_min_grbl The minimum feed rate supported by the machine. If the feed rate needs to go lower than this value, G4 (dwell) commands will be inserted to slow the process.
	* @param fr_max_grbl The maximum feed rate supported by the machine. If the feed rate needs to go faster than this value, the laser output power will be reduced.
	* @param laser_scale The laser power value will be scaled by this factor to produce a spindel speed value (S).
	* @param spmm Steps per millimeter. Used to generate reliable single step moves in combination with G4 (dwell) commands.
	*/
	void GenerateForLaserEngraver(const TJob& job, ITextWriter& tw, const float fr_min, const float fr_max, const float fr_travel, const float fr_min_grbl = 120.0f, const float fr_max_grbl = 1200.0f, const float laser_scale = 1.0f, const float spmm = 80.0f);
}
