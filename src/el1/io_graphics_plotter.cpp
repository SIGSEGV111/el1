#include "io_graphics_plotter.hpp"

namespace el1::io::graphics::plotter
{
	static v2f_t PixelToMm(const float dpmm, const pos2i_t px)
	{
		return (v2f_t)px * dpmm;
	}

	u32_t TPalette::Select(const pixel_t& color) const
	{
		float best_dist = tools[0].Distance(color);
		u32_t select = 0;

		for(usys_t i = 1; i < tools.Count(); i++)
		{
			const float dist = tools[i].Distance(color);
			if(dist < best_dist)
			{
				select = i;
				best_dist = dist;
			}
		}

		return select;
	}

	static TJob FromRasterImage(const TRasterImage& image, const float dpmm, const TPalette* const palette)
	{
		TJob job;
		const pos2i_t size = (pos2i_t)image.Size();
		pos2i_t scan_pos(0,0);
		s32_t dir = 1;
		pos2i_t pen_pos = scan_pos;
		pos2i_t prev_sp = scan_pos;
		pixel_t current_color = image[scan_pos];
		const float stroke_width = 1.0f / dpmm;
		bool pen = false;

		job.commands.MoveAppend(New<TPenCommand, IPlotterCommand>(TPenCommand::EDirection::UP, stroke_width));
		job.commands.MoveAppend(palette ? New<TToolChangeCommand, IPlotterCommand>(palette->Select(current_color)) : New<TColorChangeCommand, IPlotterCommand>(current_color));
		job.commands.MoveAppend(New<TGotoCommand, IPlotterCommand>(PixelToMm(dpmm, scan_pos), ECoordinateType::ABSOLUTE));

		for(; scan_pos[1] < size[1]; scan_pos[1]++)
		{
			if(pen)
			{
				job.commands.MoveAppend(New<TGotoCommand, IPlotterCommand>(PixelToMm(dpmm, prev_sp), ECoordinateType::ABSOLUTE));
				job.commands.MoveAppend(New<TPenCommand, IPlotterCommand>(TPenCommand::EDirection::UP, stroke_width));
				job.commands.MoveAppend(New<TGotoCommand, IPlotterCommand>(PixelToMm(dpmm, scan_pos), ECoordinateType::ABSOLUTE));
				job.commands.MoveAppend(New<TPenCommand, IPlotterCommand>(TPenCommand::EDirection::DOWN, stroke_width));
				pen_pos = scan_pos;
			}

			if(!pen && current_color[3] < 1.0f)
			{
				job.commands.MoveAppend(New<TPenCommand, IPlotterCommand>(TPenCommand::EDirection::DOWN, stroke_width));
				pen = true;
			}

			for(; scan_pos[0] < size[0] && scan_pos[0] >= 0; scan_pos[0] += dir)
			{
				const pixel_t& new_color = image[scan_pos];
				if(new_color != current_color)
				{
					if(pen_pos != prev_sp && pen)
					{
						job.commands.MoveAppend(New<TGotoCommand, IPlotterCommand>(PixelToMm(dpmm, prev_sp), ECoordinateType::ABSOLUTE));
						pen_pos = prev_sp;
					}

					if(pen)
					{
						job.commands.MoveAppend(New<TPenCommand, IPlotterCommand>(TPenCommand::EDirection::UP, stroke_width));
						pen = false;
					}

					current_color = new_color;
					if(current_color[3] < 1.0f)
					{
						job.commands.MoveAppend(palette ? New<TToolChangeCommand, IPlotterCommand>(palette->Select(current_color)) : New<TColorChangeCommand, IPlotterCommand>(current_color));

						job.commands.MoveAppend(New<TGotoCommand, IPlotterCommand>(PixelToMm(dpmm, scan_pos), ECoordinateType::ABSOLUTE));
						pen_pos = scan_pos;

						if(!pen)
						{
							job.commands.MoveAppend(New<TPenCommand, IPlotterCommand>(TPenCommand::EDirection::DOWN, stroke_width));
							pen = true;
						}
					}
				}

				prev_sp = scan_pos;
			}

			if(scan_pos[0] >= size[0])
				scan_pos[0] = size[0] - 1;
			else if(scan_pos[0] < 0)
				scan_pos[0] = 0;

			if(scan_pos != pen_pos)
			{
				job.commands.MoveAppend(New<TGotoCommand, IPlotterCommand>(PixelToMm(dpmm, scan_pos), ECoordinateType::ABSOLUTE));
				pen_pos = scan_pos;
			}

			dir *= -1;
		}

		return job;
	}

	TJob TJob::FromImage(const IImage& image, const float dpmm, const TPalette* const palette)
	{
		if(auto raster = dynamic_cast<const TRasterImage*>(&image))
			return FromRasterImage(*raster, dpmm, palette);

		EL_NOT_IMPLEMENTED;
	}
}
