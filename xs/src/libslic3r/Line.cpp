#include "Geometry.hpp"
#include "Line.hpp"
#include "Polyline.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace Slic3r {

std::string
Line::wkt() const
{
    std::ostringstream ss;
    ss << "LINESTRING(" << this->a.x << " " << this->a.y << ","
        << this->b.x << " " << this->b.y << ")";
    return ss.str();
}

Line::operator Lines() const
{
    Lines lines;
    lines.push_back(*this);
    return lines;
}

Line::operator Polyline() const
{
    Polyline pl;
    pl.points.push_back(this->a);
    pl.points.push_back(this->b);
    return pl;
}

void
Line::scale(double factor)
{
    this->a.scale(factor);
    this->b.scale(factor);
}

void
Line::translate(double x, double y)
{
    this->a.translate(x, y);
    this->b.translate(x, y);
}

void
Line::rotate(double angle, const Point &center)
{
    this->a.rotate(angle, center);
    this->b.rotate(angle, center);
}

void
Line::reverse()
{
    std::swap(this->a, this->b);
}

double
Line::length() const
{
    return this->a.distance_to(this->b);
}

Point
Line::midpoint() const
{
    return Point((this->a.x + this->b.x) / 2.0, (this->a.y + this->b.y) / 2.0);
}

void
Line::point_at(double distance, Point* point) const
{
    double len = this->length();
    *point = this->a;
    if (this->a.x != this->b.x)
        point->x = this->a.x + (this->b.x - this->a.x) * distance / len;
    if (this->a.y != this->b.y)
        point->y = this->a.y + (this->b.y - this->a.y) * distance / len;
}

Point
Line::point_at(double distance) const
{
    Point p;
    this->point_at(distance, &p);
    return p;
}

bool
Line::intersection_infinite(const Line &other, Point* point) const
{
    Vector x = this->a.vector_to(other.a);
    Vector d1 = this->vector();
    Vector d2 = other.vector();

    double cross = d1.x * d2.y - d1.y * d2.x;
    if (std::fabs(cross) < EPSILON)
        return false;

    double t1 = (x.x * d2.y - x.y * d2.x)/cross;
    point->x = this->a.x + d1.x * t1;
    point->y = this->a.y + d1.y * t1;
    return true;
}

bool
Line::coincides_with(const Line &line) const
{
    return this->a.coincides_with(line.a) && this->b.coincides_with(line.b);
}

double
Line::distance_to(const Point &point) const
{
    return point.distance_to(*this);
}

double
Line::atan2_() const
{
    return atan2(this->b.y - this->a.y, this->b.x - this->a.x);
}

double
Line::orientation() const
{
    double angle = this->atan2_();
    if (angle < 0) angle = 2*PI + angle;
    return angle;
}

double
Line::direction() const
{
    double atan2 = this->atan2_();
    return (fabs(atan2 - PI) < EPSILON) ? 0
        : (atan2 < 0) ? (atan2 + PI)
        : atan2;
}

bool
Line::parallel_to(double angle) const {
    return Slic3r::Geometry::directions_parallel(this->direction(), angle);
}

bool
Line::parallel_to(const Line &line) const {
    return this->parallel_to(line.direction());
}

bool
Line::overlap_with(const Line &other, Line* line) const
{
    bool result = false;
    if(!this->coincides_with(other)) {
        if(this->a.distance_to(other) < SCALED_EPSILON) {
            line->a = this->a;
            result = true;
        }else if(other.a.distance_to(*this) < SCALED_EPSILON){
            line->a = other.a;
            result = true;
        }
        if(result) {
            result = false;
            if(this->b.distance_to(other) < SCALED_EPSILON) {
                line->b = this->b;
                result = true;
            }else if(other.b.distance_to(*this) < SCALED_EPSILON){
                line->b = other.b;
                result = true;
            }
        }
        // filter lines with contact at only one endpoint
        if(line->length() < EPSILON) result = false;

    }
    return result;
}

bool
Line::contains(const Line &other) const
{
    bool result = false;
    if(!this->coincides_with(other)) {
        if(other.a.distance_to(*this) < SCALED_EPSILON && // other endpoint is on this line?
                !other.a.coincides_with_epsilon(this->a) && // endpoints should not be the same,
                !other.a.coincides_with_epsilon(this->b) && // we are looking for a real overlap
                other.b.distance_to(*this) < SCALED_EPSILON &&
                !other.b.coincides_with_epsilon(this->a) &&
                !other.b.coincides_with_epsilon(this->b)) {
            result = true;
        }
    }
    return result;
}

Vector
Line::vector() const
{
    return Vector(this->b.x - this->a.x, this->b.y - this->a.y);
}

Vector
Line::normal() const
{
    return Vector((this->b.y - this->a.y), -(this->b.x - this->a.x));
}

void
Line::extend_end(double distance)
{
    // relocate last point by extending the segment by the specified length
    Line line = *this;
    line.reverse();
    this->b = line.point_at(-distance);
}

void
Line::extend_start(double distance)
{
    // relocate first point by extending the first segment by the specified length
    this->a = this->point_at(-distance);
}

bool
Line::intersection(const Line& line, Point* intersection) const
{
    double denom = ((double)(line.b.y - line.a.y)*(this->b.x - this->a.x)) -
                   ((double)(line.b.x - line.a.x)*(this->b.y - this->a.y));

    double nume_a = ((double)(line.b.x - line.a.x)*(this->a.y - line.a.y)) -
                    ((double)(line.b.y - line.a.y)*(this->a.x - line.a.x));

    double nume_b = ((double)(this->b.x - this->a.x)*(this->a.y - line.a.y)) -
                    ((double)(this->b.y - this->a.y)*(this->a.x - line.a.x));
    
    if (fabs(denom) < EPSILON) {
        if (fabs(nume_a) < EPSILON && fabs(nume_b) < EPSILON) {
            return false; // coincident
        }
        return false; // parallel
    }

    double ua = nume_a / denom;
    double ub = nume_b / denom;

    if (ua >= 0 && ua <= 1.0f && ub >= 0 && ub <= 1.0f)
    {
        // Get the intersection point.
        intersection->x = this->a.x + ua*(this->b.x - this->a.x);
        intersection->y = this->a.y + ua*(this->b.y - this->a.y);
        return true;
    }
    
    return false;  // not intersecting
}

double
Line::ccw(const Point& point) const
{
    return point.ccw(*this);
}

std::ostream&
operator<<(std::ostream &stm, const Line3 &line3)
{
    return stm << "LINESTRING(" << line3.a.x << " " << line3.a.y << " " << line3.a.z << ","
        << line3.b.x << " " << line3.b.y << " " << line3.b.z << ")";
}

void
Line3::translate(double x, double y, double z)
{
    this->a.translate(x, y, z);
    this->b.translate(x, y, z);
}

double
Line3::length() const
{
    return this->a.distance_to(this->b);
}

Point3
Line3::intersect_plane(coord_t z) const
{
    return Point3(
        this->a.x + (this->b.x - this->a.x) * (z - this->a.z) / (this->b.z - this->a.z),
        this->a.y + (this->b.y - this->a.y) * (z - this->a.z) / (this->b.z - this->a.z),
        z
    );
}

Pointf3
Linef3::intersect_plane(double z) const
{
    return Pointf3(
        this->a.x + (this->b.x - this->a.x) * (z - this->a.z) / (this->b.z - this->a.z),
        this->a.y + (this->b.y - this->a.y) * (z - this->a.z) / (this->b.z - this->a.z),
        z
    );
}

void
Linef3::scale(double factor)
{
    this->a.scale(factor);
    this->b.scale(factor);
}

}
