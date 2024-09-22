#include "math_polygon.hpp"

namespace el1::math::polygon
{
	int Orientation(const v2d_t &p, const v2d_t &q, const v2d_t &r)
	{
		double val = (q[1] - p[1]) * (r[0] - q[0]) - (q[0] - p[0]) * (r[1] - q[1]);
		if (val == 0) return 0;  // collinear
		return (val > 0) ? 1 : 2; // clockwise or counterclockwise
	}

	void TPolygon::Validate(const vertices_t& vertices)
	{
		EL_NOT_IMPLEMENTED;
	}

	TPolygon TPolygon::ConvexHull() const
	{
		EL_NOT_IMPLEMENTED;
	}

	TPolygon TPolygon::MinkowskiSum(const TPolygon& other) const
	{
		EL_NOT_IMPLEMENTED;
	}

	bool TPolygon::Intersects(const TPolygon& other) const
	{
		EL_NOT_IMPLEMENTED;
	}

	void TPolygon::Apply(const m33_t& transform)
	{
		EL_NOT_IMPLEMENTED;
	}

	void TPolygon::Mirror(const v2d_t& axis)
	{
		EL_NOT_IMPLEMENTED;
	}

	void TPolygon::Translate(const v2d_t& vector)
	{
		for(auto& v : vertices)
			v += vector;
	}

	void TPolygon::Scale(const double factor)
	{
		EL_NOT_IMPLEMENTED;
	}

	void TPolygon::Offset(const double amount)
	{
		EL_NOT_IMPLEMENTED;
	}

	TPolygon TPolygon::Simplify(const double tolerance) const
	{
		EL_NOT_IMPLEMENTED;
	}

	double TPolygon::Area() const
	{
		EL_NOT_IMPLEMENTED;
	}

	double TPolygon::Perimeter() const
	{
		double p = 0.0;
		for(usys_t i = 0; i < vertices.Count(); i++)
			p += (vertices[i] - vertices[i-1]).Magnitude();
		return p;
	}

	v2d_t TPolygon::Centroid() const
	{
		v2d_t c(0,0);
		for(auto& v : vertices)
			c += v;
		c /= vertices.Count();
		return c;
	}

	rect_t TPolygon::BoundingBox() const
	{
		EL_NOT_IMPLEMENTED;
	}

	bool TPolygon::Contains(const v2d_t& point) const
	{
		EL_NOT_IMPLEMENTED;
	}

	TPolygon TPolygon::Rect(const rect_t& rect)
	{
		EL_NOT_IMPLEMENTED;
	}

	TPolygon TPolygon::Triangle(const v2d_t& a, const v2d_t& b, const v2d_t& c)
	{
		EL_NOT_IMPLEMENTED;
	}

	TPolygon TPolygon::Triangle(const double a, const double b, const double c, const v2d_t& pos)
	{
		EL_NOT_IMPLEMENTED;
	}

	TPolygon TPolygon::Circle(const double radius, const usys_t n_segments, const v2d_t& center)
	{
		EL_NOT_IMPLEMENTED;
	}

	TPolygon TPolygon::Ellipse(const double rad_x, const double rad_y, const usys_t n_segments, const v2d_t& center)
	{
		EL_NOT_IMPLEMENTED;
	}

	TPolygon TPolygon::Star(const usys_t points, const double rad_outer, const double rad_inner, const v2d_t& center)
	{
		EL_NOT_IMPLEMENTED;
	}

	TPolygon::TPolygon()
	{
		// nothing to do
	}

	TPolygon::TPolygon(vertices_t _vertices) : vertices(std::move(_vertices))
	{
		Validate(vertices);
	}
}
