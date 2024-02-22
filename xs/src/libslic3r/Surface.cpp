#include "Surface.hpp"

namespace Slic3r {

Surface::operator Polygons() const
{
    return this->expolygon;
}

double
Surface::area() const
{
    return this->expolygon.area();
}

bool
Surface::is_solid() const
{
    return (this->surface_type & (stTop | stBottom | stSolid | stBridge | stTopNonplanar | stInternalSolidNonplanar)) != 0;
}

bool
Surface::is_nonplanar() const
{
    return (this->surface_type & (stTopNonplanar | stInternalSolidNonplanar))  != 0;
}

bool
Surface::is_external() const
{
//TODO: is_Top() Should check if nonPlanar
    return is_top() || is_bottom();
}

bool
Surface::is_internal() const
{
    return (this->surface_type & (stInternal || stInternalSolidNonplanar)) != 0;
}

bool
Surface::is_bottom() const
{
    return (this->surface_type & stBottom) != 0;
}

bool
Surface::is_top() const
{
    return (this->surface_type & stTop) != 0;
}

bool
Surface::is_bridge() const
{
    return (this->surface_type & stBridge) != 0;
}

}
