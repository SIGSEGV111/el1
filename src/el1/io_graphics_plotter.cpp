#include "io_graphics_plotter.hpp"

namespace el1::io::graphics::plotter
{
	static v2d_t PixelToMm(const float dpmm, const pos2i_t px)
	{
		return (v2d_t)px * (1.0f / dpmm);
	}

	u32_t TPalette::Select(const pixel_t& color) const
	{
		EL_ERROR(tools.Count() >= 4294967295UL, TLogicException);
		float best_dist = tools[0].Distance(color);
		usys_t select = 0;

		for(usys_t i = 1; i < tools.Count(); i++)
		{
			const float dist = tools[i].Distance(color);
			if(dist < best_dist)
			{
				select = i;
				best_dist = dist;
			}
		}

		return (u32_t)select;
	}

	static void AddColorChangeCmd(TJob& job, const TPalette* const palette, const pixel_t& color)
	{
		if(palette)
			job.AddCmd<TToolChangeCommand>(palette->Select(color));
		else
			job.AddCmd<TColorChangeCommand>(color);
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
		bool pen_down = false;

		job.AddCmd<TPenCommand>(TPenCommand::EDirection::UP, stroke_width);
		AddColorChangeCmd(job, palette, current_color);
		job.AddCmd<TGotoCommand>(PixelToMm(dpmm, scan_pos), ECoordinateType::ABSOLUTE);

		for(; scan_pos[1] < size[1]; scan_pos[1]++)
		{
			// new line, if we are drawing, we need to move the pen up one line without drawing
			if(pen_down)
			{
				job.AddCmd<TGotoCommand>(PixelToMm(dpmm, prev_sp), ECoordinateType::ABSOLUTE);
				job.AddCmd<TPenCommand>(TPenCommand::EDirection::UP, stroke_width);
				job.AddCmd<TGotoCommand>(PixelToMm(dpmm, scan_pos), ECoordinateType::ABSOLUTE);
				job.AddCmd<TPenCommand>(TPenCommand::EDirection::DOWN, stroke_width);
				pen_pos = scan_pos;
			}

			// if(!pen_down && current_color.Alpha() < 1.0f)
			// {
			// 	job.AddCmd<TPenCommand>(TPenCommand::EDirection::DOWN, stroke_width);
			// 	pen_down = true;
			// }

			for(; scan_pos[0] < size[0] && scan_pos[0] >= 0; scan_pos[0] += dir)
			{
				const pixel_t& new_color = image[scan_pos];
				if(new_color != current_color)
				{
					if(pen_down)
					{
						if(pen_pos != prev_sp)
						{
							job.AddCmd<TGotoCommand>(PixelToMm(dpmm, prev_sp), ECoordinateType::ABSOLUTE);
							pen_pos = prev_sp;
						}

						job.AddCmd<TPenCommand>(TPenCommand::EDirection::UP, stroke_width);
						pen_down = false;
					}

					current_color = new_color;
					if(current_color.Alpha() < 1.0f)
					{
						AddColorChangeCmd(job, palette, current_color);
						job.AddCmd<TGotoCommand>(PixelToMm(dpmm, scan_pos), ECoordinateType::ABSOLUTE);
						pen_pos = scan_pos;

						job.AddCmd<TPenCommand>(TPenCommand::EDirection::DOWN, stroke_width * (1.0f - current_color.Alpha()));
						pen_down = true;
					}
				}

				prev_sp = scan_pos;
			}

			if(scan_pos[0] >= size[0])
				scan_pos[0] = size[0] - 1;
			else if(scan_pos[0] < 0)
				scan_pos[0] = 0;

			dir *= -1;

			// if(scan_pos != pen_pos)
			// {
			// 	job.AddCmd<TGotoCommand>(PixelToMm(dpmm, scan_pos), ECoordinateType::ABSOLUTE);
			// 	pen_pos = scan_pos;
			// }
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
