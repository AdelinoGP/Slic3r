#ifndef slic3r_Geometry_hpp_
#define slic3r_Geometry_hpp_

#include "libslic3r.h"
#include "BoundingBox.hpp"
#include "ExPolygon.hpp"
#include "Polygon.hpp"
#include "Polyline.hpp"

#include "boost/polygon/voronoi.hpp"
using boost::polygon::voronoi_builder;
using boost::polygon::voronoi_diagram;

namespace Slic3r { namespace Geometry {

Polygon convex_hull(Points points);
Polygon convex_hull(const Polygons &polygons);
void chained_path(const Points &points, std::vector<Points::size_type> &retval, Point start_near);
void chained_path(const Points &points, std::vector<Points::size_type> &retval);
template<class T> void chained_path_items(Points &points, T &items, T &retval);
bool directions_parallel(double angle1, double angle2, double max_diff = 0);
template<class T> bool contains(const std::vector<T> &vector, const Point &point);
template<class T> double area(const std::vector<T> &vector);
double rad2deg(double angle);
double rad2deg_dir(double angle);
double deg2rad(double angle);

/// Find the center of the circle corresponding to the vector of Points as an arc.
Point circle_taubin_newton(const Points& input, size_t cycles = 20);
Point circle_taubin_newton(const Points::const_iterator& input_start, const Points::const_iterator& input_end, size_t cycles = 20);

/// Find the center of the circle corresponding to the vector of Pointfs as an arc.
Pointf circle_taubin_newton(const Pointfs& input, size_t cycles = 20);
Pointf circle_taubin_newton(const Pointfs::const_iterator& input_start, const Pointfs::const_iterator& input_end, size_t cycles = 20);

/// Epsilon value
// FIXME: this is a duplicate from libslic3r.h
constexpr double epsilon { 1e-4 };
constexpr coord_t scaled_epsilon { static_cast<coord_t>(epsilon / SCALING_FACTOR) };

double linint(double value, double oldmin, double oldmax, double newmin, double newmax);
bool Point_in_triangle(Pointf pt, Pointf v1, Pointf v2, Pointf v3);
void Project_point_on_plane(Pointf3 v1,  Pointf3 n, Point &pt);
Point* Line_intersection(Pointf3 p1, Pointf3 p2, Pointf3 p3, Pointf3 p4);
bool arrange(
    // input
    size_t num_parts, const Pointf &part_size, coordf_t gap, const BoundingBoxf* bed_bounding_box,
    // output
    Pointfs &positions);

class MedialAxis {
    public:
    Lines lines;
    const ExPolygon* expolygon;
    double max_width;
    double min_width;
    MedialAxis(double _max_width, double _min_width, const ExPolygon* _expolygon = NULL)
        : expolygon(_expolygon), max_width(_max_width), min_width(_min_width) {};
    void build(ThickPolylines* polylines);
    void build(Polylines* polylines);

    private:
    typedef voronoi_diagram<double> VD;
    VD vd;
    std::set<const VD::edge_type*> edges, valid_edges;
    std::map<const VD::edge_type*, std::pair<coordf_t,coordf_t> > thickness;
    void process_edge_neighbors(const VD::edge_type* edge, ThickPolyline* polyline);
    bool validate_edge(const VD::edge_type* edge);
    const Line& retrieve_segment(const VD::cell_type* cell) const;
    const Point& retrieve_endpoint(const VD::cell_type* cell) const;
};

} }

#endif
