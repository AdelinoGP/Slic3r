#ifndef slic3r_Layer_hpp_
#define slic3r_Layer_hpp_

#include "libslic3r.h"
#include "Flow.hpp"
#include "SurfaceCollection.hpp"
#include "ExtrusionEntityCollection.hpp"
#include "ExPolygonCollection.hpp"
#include "PolylineCollection.hpp"
#include <boost/thread.hpp>


namespace Slic3r {

typedef std::pair<coordf_t,coordf_t> t_layer_height_range;
typedef std::map<t_layer_height_range,coordf_t> t_layer_height_ranges;

class Layer;
class PrintRegion;
class PrintObject;


// TODO: make stuff private
class LayerRegion
{
    friend class Layer;

    public:
    Layer* layer()                      { return this->_layer;  };
    const Layer* layer() const          { return this->_layer;  };
    PrintRegion* region()               { return this->_region; };
    const PrintRegion* region() const   { return this->_region; };

    /// Collection of surfaces generated by slicing the original geometry
    /// Divided by type top/bottom/internal
    SurfaceCollection slices;

    /// Collection of extrusion paths/loops filling gaps
    ExtrusionEntityCollection thin_fills;

    /// Collection of surfaces for infill generation
    SurfaceCollection fill_surfaces;

    /// Collection of expolygons representing the bridged areas (thus not
    /// needing support material)
    Polygons bridged;

    /// Collection of polylines representing the unsupported bridge edges
    PolylineCollection unsupported_bridge_edges;

    /// Ordered collection of extrusion paths/loops to build all perimeters
    /// (this collection contains only ExtrusionEntityCollection objects)
    ExtrusionEntityCollection perimeters;

    /// Ordered collection of extrusion paths to fill surfaces
    /// (this collection contains only ExtrusionEntityCollection objects)
    ExtrusionEntityCollection fills;

    /// Flow object which provides methods to predict material spacing.
    Flow flow(FlowRole role, bool bridge = false, double width = -1) const;
    /// Merges this->slices
    void merge_slices();
    /// Preprocesses fill surfaces
    void prepare_fill_surfaces();
    /// Generates and stores the perimeters and thin fills
    void make_perimeters(const SurfaceCollection &slices, SurfaceCollection* fill_surfaces);
    /// Generate infills for a LayerRegion.
    void make_fill();
    /// Processes external surfaces for bridges and top/bottom surfaces
    void process_external_surfaces();
    /// Gets the smallest fillable area
    double infill_area_threshold() const;

    private:
    /// Pointer to associated Layer
    Layer *_layer;
    /// Pointer to associated PrintRegion
    PrintRegion *_region;
    /// Mutex object for slices.
    mutable boost::mutex _slices_mutex;

    ///Constructor
    LayerRegion(Layer *layer, PrintRegion *region)
        : _layer(layer), _region(region) {};
    ///Destructor
    ~LayerRegion() {};
};

/// A std::vector of LayerRegion Pointers
typedef std::vector<LayerRegion*> LayerRegionPtrs;

class Layer {
    friend class PrintObject;

    public:
    /// ID number
    size_t id() const;
    /// Setter for this->_id
    void set_id(size_t id);
    /// Getter for _object
    PrintObject* object()               { return this->_object; };
    const PrintObject* object() const   { return this->_object; };

    Layer *upper_layer;     ///< Pointer to layer above
    Layer *lower_layer;     ///< Pointer to layer below
    LayerRegionPtrs regions;    ///< Vector of pointers to the LayerRegions of this layer
    bool slicing_errors;    ///< Presence of slicing errors
    coordf_t slice_z;       ///< Z used for slicing in unscaled coordinates
    coordf_t print_z;       ///< Z used for printing in unscaled coordinates
    coordf_t height;        ///< layer height in unscaled coordinates

    ExPolygonCollection slices; ///< collection of expolygons generated by slicing the original geometry;
                                ///< also known as 'islands' (all regions and surface types are merged here)

    /// Returns the number of regions
    size_t region_count() const;
    /// Gets a region at a specific id
    LayerRegion* get_region(size_t idx) { return this->regions.at(idx); };
    /// Gets a region at a specific id as const
    const LayerRegion* get_region(size_t idx) const { return this->regions.at(idx); };

    /// Adds a PrintRegion
    LayerRegion* add_region(PrintRegion* print_region);
    /// Merge all regions' slices to get islands
    void make_slices();
    /// Merges all of the LayerRegions' slices
    void merge_slices();
    /// Template which iterates over all of the LayerRegion for internally containing the argument
    template <class T> bool any_internal_region_slice_contains(const T &item) const;
    /// Template which iterates over all of the LayerRegion for containing on the bottom the argument
    template <class T> bool any_bottom_region_slice_contains(const T &item) const;
    /// Creates the perimeters cummulatively for all layer regions sharing the same parameters influencing the perimeters.
    void make_perimeters();
    /// Makes fills for all the LayerRegion
    void make_fills();
    /// Marks Nonplanar Surfaces inside the layers
    void detect_nonplanar_layers();
    /// Projects nonplanar surfaces downwards regarding the structure of the stl mesh.
    void project_nonplanar_surfaces();
    ///project a nonplanar path
    void project_nonplanar_path(ExtrusionPath* path);
    /// Determines the type of surface (top/bottombridge/bottom/internal) each region is
    void detect_surfaces_type();
    /// Processes the external surfaces
    void process_external_surfaces();

    /// polymorphic id
    virtual bool is_support() const { return false;}
    
    protected:
    size_t _id;     ///< sequential number of layer, 0-based
    PrintObject* _object; ///< Associated PrintObject

    /// Constructor
    Layer(size_t id, PrintObject *object, coordf_t height, coordf_t print_z,
        coordf_t slice_z);
    /// Destructor
    virtual ~Layer();

    /// Deletes all regions
    void clear_regions();
    /// Deletes a specific region
    void delete_region(int idx);
};


class SupportLayer : public Layer {
    friend class PrintObject;

    public:
    /// Collection of support islands.
    /// Populated in SupportMaterial.pm in sub generate_toolpaths
    ExPolygonCollection support_islands;
    /// Collection of support fills.
    /// Populated in SupportMaterial.pm in sub generate_toolpaths
    ExtrusionEntityCollection support_fills;
    /// Collection of support interface fills.
    /// Populated in SupportMaterial.pm in sub generate_toolpaths
    ExtrusionEntityCollection support_interface_fills;

    /// polymorphic id
    bool is_support() const override { return true;}

    protected:
    /// Constructor
    SupportLayer(size_t id, PrintObject *object, coordf_t height,
        coordf_t print_z, coordf_t slice_z)
        : Layer(id, object, height, print_z, slice_z) {};
    /// Destructor
    virtual ~SupportLayer() {};
};


}

#endif
