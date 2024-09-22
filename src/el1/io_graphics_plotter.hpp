#pragma once
#include "io_graphics_image.hpp"
#include "io_collection_list.hpp"
#include "system_time.hpp"

namespace el1::io::graphics::plotter
{
	using namespace io::types;
	using namespace io::graphics::image;

	/**
	 * @brief Base class for all plotter commands.
	 */
	struct IPlotterCommand
	{
		virtual ~IPlotterCommand() {}
	};

	/**
	 * @brief Enum representing coordinate type.
	 *        - ABSOLUTE: Coordinates are given in absolute values.
	 *        - RELATIVE: Coordinates are given relative to the current position.
	 */
	enum class ECoordinateType : u8_t
	{
		ABSOLUTE,
		RELATIVE
	};

	/**
	 * @brief Command to move the plotter to a specified endpoint.
	 *
	 * @param endpoint The destination point to move to.
	 * @param ctype Specifies whether the endpoint is absolute or relative.
	 */
	struct TGotoCommand : IPlotterCommand
	{
		v2d_t endpoint;
		ECoordinateType ctype;

		constexpr TGotoCommand(const v2d_t endpoint, const ECoordinateType ctype)
			: endpoint(endpoint), ctype(ctype) {}
	};

	/**
	 * @brief Command to perform an arc movement.
	 *
	 * @param arc Defines the arc to follow.
	 * @param ctype Specifies whether the arc movement is absolute or relative.
	 */
	struct TArcMoveCommand : IPlotterCommand
	{
		TArc arc;
		ECoordinateType ctype;

		TArcMoveCommand(TArc arc, const ECoordinateType ctype)
			: arc(std::move(arc)), ctype(ctype) {}
	};

	/**
	 * @brief Command to control the pen (or laser) state.
	 *        - UP: Lifts the pen or turns off the laser.
	 *        - DOWN: Lowers the pen or turns on the laser.
	 *
	 * @param dir The direction of the pen (UP/DOWN).
	 * @param stroke_width The width of the stroke or the tool's engagement depth.
	 */
	struct TPenCommand : IPlotterCommand
	{
		enum class EDirection : u8_t
		{
			UP,
			DOWN
		};

		EDirection dir;
		float stroke_width;

		constexpr TPenCommand(const EDirection dir, const float stroke_width)
			: dir(dir), stroke_width(stroke_width) {}
	};

	/**
	 * @brief Command to change the active tool.
	 *
	 * @param tool_index Index of the new tool to switch to.
	 */
	struct TToolChangeCommand : IPlotterCommand
	{
		u32_t tool_index;

		constexpr TToolChangeCommand(const u32_t tool_index)
			: tool_index(tool_index) {}
	};

	/**
	 * @brief Command to change the color used by the plotter.
	 *
	 * @param new_color The new color to apply.
	 */
	struct TColorChangeCommand : IPlotterCommand
	{
		pixel_t new_color;

		constexpr TColorChangeCommand(const pixel_t new_color)
			: new_color(new_color) {}
	};

	/**
	 * @brief Command to introduce a delay in the plotter job.
	 *
	 * @param delay The amount of time to wait before the next command is executed.
	 */
	struct TDelayCommand : IPlotterCommand
	{
		system::time::TTime delay;

		constexpr TDelayCommand(const system::time::TTime delay)
			: delay(delay) {}
	};

	/**
	 * @brief Command to introduce a pause in the plotter job.
	 *        The machine will stop and execution needs to be manually resumed by the operator.
	 */
	struct TPauseCommand : IPlotterCommand
	{
		constexpr TPauseCommand() {}
	};

	/**
	 * @brief Class for managing a color-to-tool mapping (palette).
	 *        Used for selecting the closest matching tool for a given color.
	 */
	struct TPalette
	{
		TList<pixel_t> tools;

		/**
		 * @brief Selects the closest matching tool index based on the given color.
		 *
		 * @param color The color to match against the palette.
		 * @return The index of the tool corresponding to the closest color.
		 */
		u32_t Select(const pixel_t& color) const EL_GETTER;
	};

	/**
	 * @brief Represents a plotter job consisting of a series of commands.
	 */
	struct TJob
	{
		TList<std::unique_ptr<IPlotterCommand>> commands;

		template<typename C, typename ... A>
		inline void AddCmd(A&& ... a)
		{
			commands.MoveAppend(New<C, IPlotterCommand>(a ...));
		}

		/**
		 * @brief Creates a plotter job from an image.
		 *
		 * @param image The source image to convert into a plotter job.
		 * @param dpmm Dots per millimeter for scaling the image.
		 * @param palette (Optional) A palette for color-to-tool mapping. If provided, colors are converted to tool indices.
		 * @return A plotter job with commands based on the input image.
		 */
		static TJob FromImage(const IImage& image, const float dpmm, const TPalette* const palette);
	};
}
