#pragma once
#include "io_types.hpp"
#include "io_collection_list.hpp"
#include "io_stream.hpp"
#include "io_graphics_color.hpp"
#include "io_file.hpp"
#include "math_matrix.hpp"
#include "math_polygon.hpp"

namespace el1::io::graphics::image
{
	using namespace io::types;
	using namespace io::stream;
	using namespace io::collection::list;
	using namespace io::graphics::color;
	using namespace math::matrix;
	using namespace math::vector;
	using namespace math::polygon;

	using size2i_t = math::vector::TVector<u32_t, 2>;
	using pos2i_t = math::vector::TVector<s32_t, 2>;
	using pixel_t = color::rgba_t<float>;

	struct IImage;
	class TRasterImage;
	class TVectorImage;

	enum class EImageFormat
	{
		UNKNOWN,
		PNG,
		JPG,
		GIF,
		BMP,
		TIFF,
		WEBP,
		SVG,
		PPM,
		PGM,
		PBM,
	};

	EImageFormat DetectImageFormat(const array_t<const byte_t> header);

	struct IImage
	{
		static std::unique_ptr<IImage> Load(io::file::TFile& file);
		static std::unique_ptr<IImage> Load(const array_t<const byte_t> data);
		static std::unique_ptr<IImage> Load(IBinarySource& stream, const EImageFormat format);

		virtual void Invert() = 0;

		virtual ~IImage() {}
	};

	/**
	* Base interface for shapes that can be rendered to a raster image.
	* Each shape has a color and supports rendering and bounding box calculations.
	*/
	struct IShape
	{
		/**
		* The color of the shape in RGBA format.
		* Values are expected to be in the range 0.0 to 1.0 for each component.
		*/
		pixel_t color;

		/**
		* Renders the shape onto a raster image at the specified position and rotation.
		*
		* @param img The raster image where the shape will be rendered.
		*/
		virtual void Render(TRasterImage& img, const m33_t& transformation) const = 0;

		/**
		* Calculates the axis-aligned bounding box of the shape.
		*
		* @return The bounding box in 2D space.
		*/
		virtual rect_t BoundingBox(const m33_t& transformation = m33_t::Identity()) const EL_GETTER = 0;

		constexpr IShape(pixel_t color) : color(color) {}
		virtual ~IShape() {}
	};

	struct IShape1d : IShape
	{
		float stroke_width;

		// returns the vector to the endpoint
		virtual v2d_t Endpoint() const EL_GETTER = 0;

		constexpr IShape1d(pixel_t color, float stroke_width) : IShape(color), stroke_width(stroke_width) {}
	};

	struct IShape2d : IShape
	{
	};

	/**
	* Represents a composite shape made of multiple patches (sub-shapes) combined through various operations.
	* The patches can be added, subtracted, or overlaid on each other with different blending modes and affected areas.
	*/
	struct TAperture : public IShape2d
	{
		enum class EOp : u8_t
		{
			/**
			* Adds the RGB values of the two shapes together.
			* The alpha value of the second shape is subtracted from the first.
			*
			* Example:
			* Shape 1 (R=0.2, G=0.3, B=0.4, A=1.0)
			* Shape 2 (R=0.5, G=0.2, B=0.1, A=0.5)
			* Resulting color: (R=0.7, G=0.5, B=0.5), Alpha: 0.5
			*/
			ADD,

			/**
			* Subtracts the RGB values of the second shape from the first.
			* The alpha value of the second shape is added to the alpha of the first.
			*
			* Example:
			* Shape 1 (R=0.7, G=0.6, B=0.8, A=0.8)
			* Shape 2 (R=0.4, G=0.2, B=0.3, A=0.3)
			* Resulting color: (R=0.3, G=0.4, B=0.5), Alpha: 1.0
			*/
			SUB,

			/**
			* The second shape is overlaid on top of the first.
			* The alpha values are blended to reveal underlying shapes in transparent areas.
			* For details see `io_graphics_image.md`.
			*/
			OVERLAY
		};

		enum class EArea : u8_t
		{
			/**
			* Only the intersection of the two shapes is affected by the operation.
			* Non-intersecting areas of the shapes are left unchanged.
			*/
			INTERSECTION,

			/**
			* The operation affects the union of the two shapes.
			* This means both intersecting and non-intersecting areas of the shapes are considered.
			*/
			UNION
		};

		struct patch_t
		{
			m33_t transformation;
			std::unique_ptr<IShape> shape;
			EOp op;
			EArea area;
		};

		TList<patch_t> patches;

		void Render(TRasterImage& img, const m33_t& transformation) const final override;
		rect_t BoundingBox(const m33_t& transformation = m33_t::Identity()) const final override EL_GETTER;
	};

	/**
	* Represents an ellipse shape.
	* The size determines the width and height of the ellipse.
	*/
	struct TEllipse : IShape2d
	{
		/**
		* Size of the ellipse, where width and height are represented.
		* Negative values will flip the shape along the corresponding axis.
		*/
		v2d_t size;

		void Render(TRasterImage& img, const m33_t& transformation) const final override;
		rect_t BoundingBox(const m33_t& transformation = m33_t::Identity()) const final override EL_GETTER;
	};

	/**
	* Represents a rectangle shape.
	* The size determines the width and height of the rectangle.
	*/
	struct TRectangle : IShape2d
	{
		v2d_t size;

		void Render(TRasterImage& img, const m33_t& transformation) const final override;
		rect_t BoundingBox(const m33_t& transformation = m33_t::Identity()) const final override EL_GETTER;
	};

	/**
	* Represents a polygon shape.
	* The polygon is defined by a number of vertices in 2D space.
	*/
	struct TPolygon : IShape2d, math::polygon::TPolygon
	{
		void Render(TRasterImage& img, const m33_t& transformation) const final override;
		rect_t BoundingBox(const m33_t& transformation = m33_t::Identity()) const final override EL_GETTER;
	};

	/**
	* Represents a straight line shape.
	*/
	struct TLine : IShape1d
	{
		v2d_t endpoint;

		void Render(TRasterImage& img, const m33_t& transformation) const final override;
		rect_t BoundingBox(const m33_t& transformation = m33_t::Identity()) const final override EL_GETTER;
		v2d_t Endpoint() const final override EL_GETTER;
	};

	/**
	* Represents an arc.
	* The arc is described by a vector from the startpoint to the center and a nother vector from the startpoint to the endpoint.
	* Both start and endpoint are on the circumference.
	*/
	struct TArc : IShape1d
	{
		enum class EDirection : u8_t
		{
			CLOCKWISE,
			COUNTER_CW
		};

		v2d_t center;
		v2d_t endpoint;
		EDirection dir;

		float Radius() const EL_GETTER { return center.Magnitude(); }
		float Angle() const EL_GETTER;

		void Render(TRasterImage& img, const m33_t& transformation) const final override;
		rect_t BoundingBox(const m33_t& transformation = m33_t::Identity()) const final override EL_GETTER;
		v2d_t Endpoint() const final override EL_GETTER;

		constexpr TArc(pixel_t color, float stroke_width, v2d_t center, v2d_t endpoint, EDirection dir) : IShape1d(color, stroke_width), center(center), endpoint(endpoint), dir(dir) {}
	};

	struct TBezierCurve : IShape1d
	{
		TList<v2d_t> control_points;

		void Render(TRasterImage& img, const m33_t& transformation) const final override;
		rect_t BoundingBox(const m33_t& transformation = m33_t::Identity()) const final override EL_GETTER;
		v2d_t Endpoint() const final override EL_GETTER;
	};

	struct TPath : IShape1d
	{
		TList<std::unique_ptr<IShape1d>> elements;

		void Render(TRasterImage& img, const m33_t& transformation) const final override;
		rect_t BoundingBox(const m33_t& transformation = m33_t::Identity()) const final override EL_GETTER;
		v2d_t Endpoint() const final override EL_GETTER;
	};

	class TVectorImage : public IImage
	{
		public:
			struct element_t
			{
				v2d_t pos;
				std::unique_ptr<IShape> shape;
			};

			using shape_list_t = TList<element_t>;

			shape_list_t shapes;
			pixel_t background;

			void Invert() final override;
			TRasterImage Render(const float dpmm);
	};

	class TRasterImage : public IImage
	{
		protected:
			TList<pixel_t> pixels;
			size2i_t size;

		public:
			size2i_t AbsPos(const pos2i_t) const EL_GETTER;
			usys_t Pos2Index(const pos2i_t) const EL_GETTER;

			array_t<pixel_t> Pixels() EL_GETTER;
			array_t<const pixel_t> Pixels() const EL_GETTER;

			pixel_t& operator[](const pos2i_t pos) EL_GETTER;
			const pixel_t& operator[](const pos2i_t pos) const EL_GETTER;

			size2i_t Size() const EL_GETTER;
			void Size(const size2i_t new_size, const pixel_t background = {0,0,0,1});

			u32_t Width() const EL_GETTER { return size[0]; }
			u32_t Height() const EL_GETTER { return size[1]; }

			void Draw(const v2d_t position, const float rotation, TAperture::EOp op, IShape& shape);

			void Invert() final override;

			TRasterImage(TRasterImage&&) = default;
			TRasterImage(const TRasterImage&) = default;
			TRasterImage(const size2i_t size, const pixel_t background = {0,0,0,1});
			TRasterImage(const size2i_t size, TList<pixel_t> pixels);
	};
}
