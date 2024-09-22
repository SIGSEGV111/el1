#pragma once
#include "math_vector.hpp"
#include "math_matrix.hpp"
#include "io_collection_list.hpp"

namespace el1::math::polygon
{
	using namespace io::types;
	using namespace math::vector;
	using namespace math::matrix;

	using m33_t = TMatrix<double,3,3>;
	using v2d_t = TVector<double,2>;
	using vertices_t = io::collection::list::TList<v2d_t>;

	struct rect_t
	{
		v2d_t pos;
		v2d_t size;
	};

	/**
	* Determines the orientation of three points in a 2D plane.
	*
	* This function computes the orientation of an ordered triplet (p, q, r) of points in the
	* plane using the cross product of vectors (p to q) and (p to r). The sign of the cross
	* product determines the general direction the points turn as they are traversed in order.
	*
	* @param p The first point in the triplet, serving as the reference.
	* @param q The second point in the triplet, used to form the first vector with p.
	* @param r The third point in the triplet, used to form the second vector with p.
	* @return An integer representing the orientation of the triplet:
	*         - 0 if the points are collinear,
	*         - 1 if the points are in a clockwise turn,
	*         - 2 if the points are in a counterclockwise turn.
	*/
	int Orientation(const v2d_t &p, const v2d_t &q, const v2d_t &r);

	/**
	 * @brief Class representing a polygon with a list of vertices.
	 *
	 * This class provides functionalities for manipulating the vertices of the polygon,
	 * ensuring validity (e.g., no self-intersecting edges), computing convex hulls, and
	 * performing geometric operations like the Minkowski sum and intersection tests.
	 */
	class TPolygon
	{
		protected:
			vertices_t vertices;

			static void Validate(const vertices_t& vertices);

		public:
			const vertices_t& Vertices() const { return vertices; }

			/**
			 * @brief Allows modification of the vertices using a lambda function.
			 *
			 * This method accepts a lambda function which is passed a reference
			 * to the list of vertices, allowing modifications. After the lambda
			 * modifies the vertices, the polygon's validity is checked. If the
			 * modification results in an invalid polygon, an exception is thrown
			 * and the changes are reverted.
			 *
			 * @param lambda A lambda function that modifies the vertices.
			 */
			template<typename L>
			void Modify(L lambda);

			/**
			 * @brief Computes the convex hull of the polygon.
			 *
			 * This method calculates the convex hull of the polygon, returning a new polygon
			 * that represents the convex hull of the current polygon.
			 *
			 * @return A new TPolygon representing the convex hull of this polygon.
			 */
			TPolygon ConvexHull() const EL_GETTER;

			/**
			 * @brief Computes the Minkowski sum of this polygon with another polygon.
			 *
			 * This method performs the Minkowski sum operation between the current polygon
			 * and the specified polygon. The result is a new polygon representing the Minkowski sum.
			 *
			 * @param other The other polygon to add to this polygon.
			 * @return A new TPolygon representing the Minkowski sum of the two polygons.
			 */
			TPolygon MinkowskiSum(const TPolygon& other) const EL_GETTER;

			/**
			 * @brief Tests whether this polygon intersects with another polygon.
			 *
			 * This method checks if the current polygon and the specified polygon intersect each other.
			 *
			 * @param other The polygon to test for intersection.
			 * @return True if the polygons intersect, false otherwise.
			 */
			bool Intersects(const TPolygon& other) const EL_GETTER;

			/**
			* @brief Applies a 3x3 transformation matrix to the polygon.
			*
			* This method applies a general 3x3 transformation matrix to the polygon.
			* The matrix can represent a variety of transformations, such as rotation,
			* scaling, translation, or any combination of these in homogeneous coordinates.
			* The transformation is applied to all vertices of the polygon.
			*
			* @param transform The 3x3 transformation matrix to apply.
			*/
			void Apply(const m33_t& transform);

			/**
			 * @brief Reflects the polygon across a given axis.
			 *
			 * This method mirrors the polygon across a specified axis, effectively flipping
			 * its vertices across that axis.
			 *
			 * @param axis A vector that defines the axis of reflection.
			 */
			void Mirror(const v2d_t& axis);

			/**
			* @brief Translates (shifts) the polygon by a given vector.
			*
			* This method moves all vertices of the polygon by applying a translation vector.
			* The entire polygon is shifted by the same amount in the direction specified by the vector,
			* without altering its shape, size, or orientation.
			*
			* @param vector The 2D vector specifying the direction and magnitude of the translation.
			*/
			void Translate(const v2d_t& vector);

			/**
			 * @brief Scales the polygon by a given factor.
			 *
			 * This method scales the polygon, either uniformly or non-uniformly, by a given
			 * scaling factor. The vertices of the polygon are scaled relative to the origin.
			 *
			 * @param factor The scaling factor to apply to the polygon.
			 */
			void Scale(const double factor);

			/**
			* @brief Offsets the polygon by moving each vertex along the direction of the averaged adjacent edge normals.
			*
			* This method adjusts the polygon's size by offsetting its edges either outward or inward.
			* Each vertex is moved along the bisector of the angle formed by the two adjacent edges at that vertex.
			* A positive value for the offset moves the vertices outward (expanding the polygon),
			* while a negative value pulls the vertices inward (contracting the polygon).
			*
			* @param amount The distance by which to offset the edges. Positive values expand the polygon outward,
			*               and negative values contract it inward.
			*/
			void Offset(const double amount);

			/**
			 * @brief Simplifies the polygon by reducing the number of vertices.
			 *
			 * This method reduces the number of vertices in the polygon while preserving the
			 * overall shape within a specified tolerance.
			 *
			 * @param tolerance The tolerance value for vertex reduction.
			 * @return A simplified version of the polygon.
			 */
			TPolygon Simplify(const double tolerance) const EL_GETTER;

			/**
			 * @brief Calculates the area of the polygon.
			 *
			 * This method calculates the total area enclosed by the polygon using the
			 * coordinates of its vertices.
			 *
			 * @return The area of the polygon.
			 */
			double Area() const EL_GETTER;

			/**
			 * @brief Calculates the perimeter of the polygon.
			 *
			 * This method calculates the total length of the polygon's boundary,
			 * which is the sum of the lengths of its edges.
			 *
			 * @return The perimeter of the polygon.
			 */
			double Perimeter() const EL_GETTER;

			/**
			 * @brief Computes the centroid (geometric center) of the polygon.
			 *
			 * This method calculates the centroid, which is the average position of all the vertices
			 * and represents the geometric center of the polygon.
			 *
			 * @return A 2D vector representing the centroid of the polygon.
			 */
			v2d_t Centroid() const EL_GETTER;

			/**
			 * @brief Computes the axis-aligned bounding box (AABB) of the polygon.
			 *
			 * This method returns the smallest rectangle that encloses the entire polygon,
			 * aligned with the coordinate axes.
			 *
			 * @return A rectangle representing the polygon's bounding box.
			 */
			rect_t BoundingBox() const EL_GETTER;

			/**
			 * @brief Checks whether a given point lies inside the polygon.
			 *
			 * This method tests if a specified point is contained within the polygon, using
			 * point-in-polygon algorithms.
			 *
			 * @param point The point to test.
			 * @return True if the point is inside the polygon, false otherwise.
			 */
			bool Contains(const v2d_t& point) const EL_GETTER;

			/**
			* @brief Creates a rectangular polygon from the given rectangle dimensions.
			*
			* This method generates a polygon in the shape of a rectangle using the
			* specified position and size from the `rect_t` structure.
			*
			* @param rect A struct containing the position (bottom-left corner) and size (width, height) of the rectangle.
			* @return A polygon representing the rectangle.
			*/
			static TPolygon Rect(const rect_t& rect);

			/**
			* @brief Creates a triangular polygon from three specified vertices.
			*
			* This method generates a triangle polygon using three given points in 2D space.
			* The points are assumed to be given in counterclockwise order to ensure the polygon's orientation.
			*
			* @param a The first vertex of the triangle.
			* @param b The second vertex of the triangle.
			* @param c The third vertex of the triangle.
			* @return A polygon representing the triangle.
			*/
			static TPolygon Triangle(const v2d_t& a, const v2d_t& b, const v2d_t& c);

			/**
			* @brief Constructs a triangle polygon from the lengths of its sides, with an optional position offset.
			*
			* This method creates a triangle using the lengths of its sides. The triangle
			* is positioned so that side `a` is aligned with the X-axis, and the sides are
			* labeled counterclockwise. The triangle is normally positioned with point A at
			* the origin (0,0), but the `pos` parameter allows the triangle to be shifted
			* to a specified location. The method assumes the side lengths provided can form
			* a valid triangle based on the triangle inequality theorem - otherwise an
			* exception is thrown.
			*
			* @param a The length of the first side, aligned with the X-axis.
			* @param b The length of the second side (counterclockwise from `a`).
			* @param c The length of the third side (counterclockwise from `b`).
			* @param pos The position offset for the triangle. Point A will be placed at this location instead of (0,0).
			* @return A polygon representing the triangle.
			*/
			static TPolygon Triangle(const double a, const double b, const double c, const v2d_t& pos = v2d_t(0,0));

			/**
			* @brief Creates a polygon approximating a circle.
			*
			* This method generates a polygon in the shape of a circle, approximated using
			* a specified number of vertices. The circle is centered at the origin by default.
			*
			* @param radius The radius of the circle.
			* @param n_segments The number of vertices to approximate the circle (higher values create smoother circles).
			* @param center The center point of the circle (default is the origin).
			* @return A polygon representing the circle.
			*/
			static TPolygon Circle(const double radius, const usys_t n_segments = 32, const v2d_t& center = v2d_t(0, 0));

			/**
			* @brief Creates a polygon approximating an ellipse.
			*
			* This method generates a polygon in the shape of an ellipse, with a specified number
			* of vertices for approximation. The ellipse is centered at the origin by default.
			*
			* @param rad_x The radius along the X-axis.
			* @param rad_y The radius along the Y-axis.
			* @param n_segments The number of vertices to approximate the ellipse (higher values create smoother ellipses).
			* @param center The center point of the ellipse (default is the origin).
			* @return A polygon representing the ellipse.
			*/
			static TPolygon Ellipse(const double rad_x, const double rad_y, const usys_t n_segments = 32, const v2d_t& center = v2d_t(0, 0));

			/**
			* @brief Creates a star-shaped polygon with alternating outer and inner radii.
			*
			* This method generates a star-shaped polygon with alternating vertices at the
			* outer and inner radii. The polygon is centered at the origin by default.
			*
			* @param points The number of points (or "spikes") on the star.
			* @param outerRadius The radius of the outer vertices.
			* @param innerRadius The radius of the inner vertices.
			* @param center The center point of the star (default is the origin).
			* @return A polygon representing the star shape.
			*/
			static TPolygon Star(const usys_t points, const double rad_outer, const double rad_inner, const v2d_t& center = v2d_t(0, 0));

			/**
			 * @brief Constructs an empty polygon.
			 *
			 * This constructor initializes an empty polygon with no vertices.
			 */
			TPolygon();

			/**
			 * @brief Constructs a polygon from a list of vertices.
			 *
			 * This constructor creates a polygon from the provided list of vertices.
			 * The list is validated to ensure the polygon is well-formed (i.e., no self-intersections).
			 *
			 * @param vertices The list of vertices to construct the polygon from.
			 */
			TPolygon(vertices_t vertices);
	};

	template<typename L>
	void TPolygon::Modify(L lambda)
	{
		vertices_t v = vertices;
		lambda(v);
		Validate(v);
		vertices = std::move(v);
	}
}
