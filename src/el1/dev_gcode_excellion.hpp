#pragma once
#include "io_types.hpp"
#include "io_text.hpp"
#include "io_collection_list.hpp"
#include "io_collection_map.hpp"
#include "io_stream.hpp"
#include "math_vector.hpp"

namespace el1::dev::gcode::excellion
{
	using namespace io::stream;
	using namespace io::types;
	using namespace io::text;
	using namespace io::collection::list;
	using namespace io::collection::map;

	/**
	 * @brief Represents a tool used for drilling, characterized by its diameter.
	 */
	struct TTool
	{
		double diameter;
	};

	/**
	 * @brief Represents a drilling point on the board, with a position and an associated tool.
	 */
	struct TDrillPoint
	{
		math::vector::TVector<double, 2> pos;
		double diameter;
		u32_t tool_index;
	};

	/**
	 * @brief Represents an Excellon drill file, containing a toolset and a list of drill points.
	 */
	struct TExcellionFile
	{
		TSortedMap<u32_t, TTool> toolset;
		TList<TDrillPoint> holes;

		u32_t SelectTool(const double diameter) const EL_GETTER;

		/**
		 * @brief Re-maps all existing drill holes to a new toolset.
		 *
		 * For each drill hole, the function selects the best matching tool from the new toolset
		 * based on the tool diameter.
		 *
		 * @param new_toolset The new set of tools to be used for mapping.
		 */
		void ReplaceToolset(TSortedMap<u32_t, TTool> new_toolset);

		/**
		 * @brief Converts the Excellon drill definitions into GRBL-compatible G-code.
		 *
		 * The resulting G-code can be used to operate CNC machines or drill systems that
		 * accept GRBL commands.
		 *
		 * @param tw Output stream to write the generated G-code.
		 */
		void ConvertToGrbl(ITextWriter& tw, const float z_toolchange, const float z_hole2hole, const float z_drill, const float fr_travel, const float fr_drill, const float fr_retract) const;

		/**
		 * @brief Constructs an empty Excellon file with no tools or drill points.
		 */
		TExcellionFile();

		/**
		 * @brief Constructs an Excellon file by reading its contents from a binary source.
		 *
		 * @param src The binary source from which to read the file data.
		 */
		TExcellionFile(IBinarySource& src);
	};
}
