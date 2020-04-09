#ifndef slic3r_MultiPoint_hpp_
#define slic3r_MultiPoint_hpp_

#include "libslic3r.h"
#include <algorithm>
#include <vector>
#include "Line.hpp"
#include "Point.hpp"

namespace Slic3r {

class BoundingBox;

class MultiPoint
{
    public:
    Points points;
    
    operator Points() const;
    void scale(double factor);
    void translate(double x, double y);
    void translate(const Point &vector);
    void rotate(double angle);
    void rotate(double angle, const Point &center);
    void reverse();
    Point first_point() const;
    virtual Point last_point() const = 0;
    virtual Lines lines() const = 0;
    double length() const;
    bool is_valid() const { return this->points.size() >= 2; }

    int find_point(const Point &point) const;
    bool has_boundary_point(const Point &point) const;
    BoundingBox bounding_box() const;
    
    // Return true if there are exact duplicates.
    bool has_duplicate_points() const;
    
    // Remove exact duplicates, return true if any duplicate has been removed.
    bool remove_duplicate_points();
    
    void append(const Point &point);
    void append(const Points &points);
    void append(const Points::const_iterator &begin, const Points::const_iterator &end);
    /// Try to insert this point. Returns false if point is on no line.
    bool insert(const Point &point);
    bool intersection(const Line& line, Point* intersection) const;
    /// returns the first intersection with the given line i.e. the intersection
    /// with minimal distance to line.a
    bool first_intersection(const Line& line, Point* intersection, bool* ccw) const;
    /// inserts new points if the distance between two adjacent points is below min_dist. Returns true if new points where inserted.
    bool upsample(const double min_dist);
    std::string dump_perl() const;
    
    static Points _douglas_peucker(const Points &points, const double tolerance);
    
    protected:
    MultiPoint() {};
    explicit MultiPoint(const Points &_points): points(_points) {};
    ~MultiPoint() = default;
};

} // namespace Slic3r

#endif
