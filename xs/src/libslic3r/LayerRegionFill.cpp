#include "Layer.hpp"
#include "ClipperUtils.hpp"
#include "Fill/Fill.hpp"
#include "Geometry.hpp"
#include "Print.hpp"
#include "PrintConfig.hpp"
#include "Surface.hpp"

namespace Slic3r {

/// Struct for the main attributes of a Surface
/// Used for comparing properties
struct SurfaceGroupAttrib
{
    SurfaceGroupAttrib() : is_solid(false), fw(0.f), pattern(-1) {}
    /// True iff all all three attributes are the same
    bool operator==(const SurfaceGroupAttrib &other) const
        { return is_solid == other.is_solid && fw == other.fw && pattern == other.pattern; }
    bool    is_solid;   ///< if a solid infill should be used
    float   fw;         ///< flow Width
    int     pattern;    ///< pattern is of type InfillPattern, -1 for an unset pattern.
};

/// The LayerRegion at this point of time may contain
/// surfaces of various types (internal/bridge/top/bottom/solid).
/// The infills are generated on the groups of surfaces with a compatible type.
/// Fills an array of ExtrusionPathCollection objects containing the infills generated now
/// and the thin fills generated by generate_perimeters().
void
LayerRegion::make_fill()
{
    this->fills.clear();

    const double fill_density          = this->region()->config.fill_density;
    const Flow   infill_flow           = this->flow(frInfill);
    const Flow   solid_infill_flow     = this->flow(frSolidInfill);
    const Flow   top_solid_infill_flow = this->flow(frTopSolidInfill);
    const coord_t perimeter_spacing    = this->flow(frPerimeter).scaled_spacing();

    SurfaceCollection surfaces;

    // merge adjacent surfaces
    // in case of bridge surfaces, the ones with defined angle will be attached to the ones
    // without any angle (shouldn't this logic be moved to process_external_surfaces()?)
    {
        Polygons polygons_bridged;
        polygons_bridged.reserve(this->fill_surfaces.surfaces.size());
        for (Surfaces::const_iterator it = this->fill_surfaces.surfaces.begin(); it != this->fill_surfaces.surfaces.end(); ++it)
            if (it->is_bridge() && it->bridge_angle >= 0)
                append_to(polygons_bridged, (Polygons)*it);

        // group surfaces by distinct properties (equal surface_type, thickness, thickness_layers, bridge_angle)
        // group is of type SurfaceCollection
        // FIXME: Use some smart heuristics to merge similar surfaces to eliminate tiny regions.
        std::vector<SurfacesConstPtr> groups;
        this->fill_surfaces.group(&groups);

        // merge compatible solid groups (we can generate continuous infill for them)
        {
            // cache flow widths and patterns used for all solid groups
            // (we'll use them for comparing compatible groups)
            std::vector<SurfaceGroupAttrib> group_attrib(groups.size());
            for (size_t i = 0; i < groups.size(); ++i) {
                const Surface &surface = *groups[i].front();
                // we can only merge solid non-bridge surfaces, so discard
                // non-solid or bridge surfaces
                if (!surface.is_solid() || surface.is_bridge()) continue;

                group_attrib[i].is_solid = true;
                group_attrib[i].fw = (surface.surface_type == stTop || surface.surface_type == stTopNonplanar) ? top_solid_infill_flow.width : solid_infill_flow.width;
                group_attrib[i].pattern = (surface.surface_type == stTop || surface.surface_type == stTopNonplanar) ? this->region()->config.top_infill_pattern.value
                    : surface.is_bottom() ? this->region()->config.bottom_infill_pattern.value
                    : ipRectilinear;
            }
            // Loop through solid groups, find compatible groups and append them to this one.
            for (size_t i = 0; i < groups.size(); ++i) {
                if (!group_attrib[i].is_solid)
                    continue;
                for (size_t j = i + 1; j < groups.size();) {
                    if (group_attrib[i] == group_attrib[j]) {
                        // groups are compatible, merge them
                        append_to(groups[i], groups[j]);
                        groups.erase(groups.begin() + j);
                        group_attrib.erase(group_attrib.begin() + j);
                    } else {
                        ++j;
                    }
                }
            }
        }

        // Give priority to oriented bridges. Process the bridges in the first round, the rest of the surfaces in the 2nd round.
        for (size_t round = 0; round < 2; ++ round) {
            for (std::vector<SurfacesConstPtr>::const_iterator it_group = groups.begin(); it_group != groups.end(); ++ it_group) {
                const SurfacesConstPtr &group = *it_group;
                const bool is_oriented_bridge = group.front()->is_bridge() && group.front()->bridge_angle >= 0;
                if (is_oriented_bridge != (round == 0))
                    continue;

                // Make a union of polygons defining the infiill regions of a group, use a safety offset.
                Polygons union_p = union_(to_polygons(group), true);

                // Subtract surfaces having a defined bridge_angle from any other, use a safety offset.
                if (!is_oriented_bridge && !polygons_bridged.empty())
                    union_p = diff(union_p, polygons_bridged, true);

                // subtract any other surface already processed
                //FIXME Vojtech: Because the bridge surfaces came first, they are subtracted twice!
                surfaces.append(
                    diff_ex(union_p, to_polygons(surfaces), true),
                    *group.front()  // template
                );
            }
        }
    }

    // we need to detect any narrow surfaces that might collapse
    // when adding spacing below
    // such narrow surfaces are often generated in sloping walls
    // by bridge_over_infill() and combine_infill() as a result of the
    // subtraction of the combinable area from the layer infill area,
    // which leaves small areas near the perimeters
    // we are going to grow such regions by overlapping them with the void (if any)
    // TODO: detect and investigate whether there could be narrow regions without
    // any void neighbors
    {
        coord_t distance_between_surfaces = std::max(
            std::max(infill_flow.scaled_spacing(), solid_infill_flow.scaled_spacing()),
            top_solid_infill_flow.scaled_spacing()
        );

        Polygons surfaces_polygons = (Polygons)surfaces;
        Polygons collapsed = diff(
            surfaces_polygons,
            offset2(surfaces_polygons, -distance_between_surfaces/2, +distance_between_surfaces/2),
            true
        );

        Polygons to_subtract;
        surfaces.filter_by_type(stInternalVoid, &to_subtract);

        append_to(to_subtract, collapsed);
        surfaces.append(
            intersection_ex(
                offset(collapsed, distance_between_surfaces),
                to_subtract,
                true
            ),
            stInternalSolid
        );
    }

    if (false) {
//        require "Slic3r/SVG.pm";
//        Slic3r::SVG::output("fill_" . $layerm->print_z . ".svg",
//            expolygons      => [ map $_->expolygon, grep !$_->is_solid, @surfaces ],
//            red_expolygons  => [ map $_->expolygon, grep  $_->is_solid, @surfaces ],
//        );
    }

    for (Surfaces::const_iterator surface_it = surfaces.surfaces.begin();
        surface_it != surfaces.surfaces.end(); ++surface_it) {

        const Surface &surface = *surface_it;
        if (surface.surface_type == stInternalVoid)
            continue;

        InfillPattern fill_pattern = this->region()->config.fill_pattern.value;
        double density = fill_density;
        FlowRole role = (surface.surface_type == stTop || surface.surface_type == stTopNonplanar) ? frTopSolidInfill
            : surface.is_solid() ? frSolidInfill
            : frInfill;
        const bool is_bridge = this->layer()->id() > 0 && surface.is_bridge();

        if (surface.is_solid()) {
            density = 100.;
            fill_pattern = (surface.surface_type == stTop) ? this->region()->config.top_infill_pattern.value
                : (surface.is_bottom() && !is_bridge)      ? this->region()->config.bottom_infill_pattern.value
                : ipRectilinear;
        } else if (density <= 0)
            continue;
        
        // get filler object
        #if SLIC3R_CPPVER >= 11
            std::unique_ptr<Fill> f = std::unique_ptr<Fill>(Fill::new_from_type(fill_pattern));
        #else
            std::auto_ptr<Fill> f = std::auto_ptr<Fill>(Fill::new_from_type(fill_pattern));
        #endif

        // switch to rectilinear if this pattern doesn't support solid infill
        if (density > 99 && !f->can_solid())
            #if SLIC3R_CPPVER >= 11
                f = std::unique_ptr<Fill>(Fill::new_from_type(ipRectilinear));
            #else
                f = std::auto_ptr<Fill>(Fill::new_from_type(ipRectilinear));
            #endif
        
        //set fill pattern to rectilinear for all nonplanar surfaces
        if (surface.is_nonplanar()) {
            #if SLIC3R_CPPVER >= 11
                f = std::unique_ptr<Fill>(Fill::new_from_type(ipRectilinear));
            #else
                f = std::auto_ptr<Fill>(Fill::new_from_type(ipRectilinear));
            #endif
        }
            
        f->bounding_box = this->layer()->object()->bounding_box();

        // calculate the actual flow we'll be using for this infill
        coordf_t h = (surface.thickness == -1) ? this->layer()->height : surface.thickness;
        Flow flow = this->region()->flow(
            role,
            h,
            is_bridge || f->use_bridge_flow(),  // bridge flow?
            this->layer()->id() == 0,           // first layer?
            -1,                                 // auto width
            *this->layer()->object()
        );

        // calculate flow spacing for infill pattern generation
        bool using_internal_flow = false;
        if (!surface.is_solid() && !is_bridge) {
            // it's internal infill, so we can calculate a generic flow spacing
            // for all layers, for avoiding the ugly effect of
            // misaligned infill on first layer because of different extrusion width and
            // layer height
            Flow internal_flow = this->region()->flow(
                frInfill,
                h,  // use the calculated surface thickness here for internal infill instead of the layer height to account for infill_every_layers
                false,  // no bridge
                false,  // no first layer
                -1,     // auto width
                *this->layer()->object()
            );
            f->min_spacing = internal_flow.spacing();
            using_internal_flow = true;
        } else {
            f->min_spacing = flow.spacing();
        }

        f->endpoints_overlap = this->region()->config.get_abs_value("infill_overlap",
            (perimeter_spacing + scale_(f->min_spacing))/2);

        f->layer_id = this->layer()->id();
        f->z        = this->layer()->print_z;
        f->angle    = Geometry::deg2rad(this->region()->config.fill_angle.value);

        // Maximum length of the perimeter segment linking two infill lines.
        f->link_max_length = (!is_bridge && density > 80)
            ? scale_(3 * f->min_spacing)
            : 0;

        // Used by the concentric infill pattern to clip the loops to create extrusion paths.
        f->loop_clipping = scale_(flow.nozzle_diameter) * LOOP_CLIPPING_LENGTH_OVER_NOZZLE_DIAMETER;

        // apply half spacing using this flow's own spacing and generate infill
        f->density = density/100;
        f->dont_adjust = false;
        /*
        std::cout << surface.expolygon.dump_perl() << std::endl
            << " layer_id: " << f->layer_id << " z: " << f->z
            << " angle: " << f->angle << " min-spacing: " << f->min_spacing
            << " endpoints_overlap: " << f->endpoints_overlap << std::endl << std::endl;
        */
        Polylines polylines = f->fill_surface(surface);
        if (polylines.empty())
            continue;

        // calculate actual flow from spacing (which might have been adjusted by the infill
        // pattern generator)
        if (using_internal_flow) {
            // if we used the internal flow we're not doing a solid infill
            // so we can safely ignore the slight variation that might have
            // been applied to f->spacing()
        } else {
            flow = Flow::new_from_spacing(f->spacing(), flow.nozzle_diameter, h, is_bridge || f->use_bridge_flow());
        }

        // Save into layer.
        ExtrusionEntityCollection* coll = new ExtrusionEntityCollection();
        coll->no_sort = f->no_sort();
        this->fills.entities.push_back(coll);

        {
            ExtrusionRole role;
            if (is_bridge) {
                role = erBridgeInfill;
            } else if (surface.is_solid()) {
                role = (surface.surface_type == stTop || surface.surface_type == stTopNonplanar) ? erTopSolidInfill : erSolidInfill;
            } else {
                role = erInternalInfill;
            }

            ExtrusionPath templ(role);
            templ.mm3_per_mm    = flow.mm3_per_mm();
            templ.width         = flow.width;
            templ.height        = flow.height;

            coll->append(STDMOVE(polylines), templ);
        }
    }

    // add thin fill regions
    // thin_fills are of C++ Slic3r::ExtrusionEntityCollection, perl type Slic3r::ExtrusionPath::Collection
    // Unpacks the collection, creates multiple collections per path so that they will
    // be individually included in the nearest neighbor search.
    // The path type could be ExtrusionPath, ExtrusionLoop or ExtrusionEntityCollection.
    for (ExtrusionEntitiesPtr::const_iterator thin_fill = this->thin_fills.entities.begin(); thin_fill != this->thin_fills.entities.end(); ++ thin_fill) {
        ExtrusionEntityCollection* coll = new ExtrusionEntityCollection();
        this->fills.entities.push_back(coll);
        coll->append(**thin_fill);
    }
}

} // namespace Slic3r
