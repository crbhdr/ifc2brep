#pragma once

#include "OdaCommon.h"

#include "StaticRxObject.h"
#include "RxDynamicModule.h"

#include "IfcExamplesCommon.h"
#include "IfcCore.h"
#include "IfcGsManager.h"
#include "IfcGiContext.h"
#include "GiDumper.h"

#include "Modeler/FMMdlBody.h"
#include "FMProfile3D.h"
#include "FMDataSerialize.h"
#include "Modeler/FMMdlIterators.h"

#include <array>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <json/single_include/nlohmann/json.hpp>

class BrepGeometryModeler
{
public:
	BrepGeometryModeler() = default;
	~BrepGeometryModeler() = default;

	void addMappedItemAsSurface(OdIfc::OdIfcInstancePtr mappedItem);
	void addMappedItemAsSolid(OdIfc::OdIfcInstancePtr mappedItem);

    void postProcessGeometries(const OdString& strBrepFilename);

    void postProcessTriangles(const OdArray<OdIfcStlTriangleFace>& arrTriangles, const OdString& strBrepFilename);

private:
    using PolygonCoordinates = std::vector<OdGeVector3d>;
    using BoundPolygons = std::vector<PolygonCoordinates>;
    using FaceBounds = std::vector<BoundPolygons>;
    using BoundaryFaces = std::vector<FaceBounds>;

    struct OdGePoint3dCompare
    {
        bool operator() (const OdGePoint3d& lhs, const OdGePoint3d& rhs) const
        {
            return (lhs[0] < rhs[0]) ? true : (lhs[0] > rhs[0]) ? false :
                (lhs[1] < rhs[1]) ? true : (lhs[1] > rhs[1]) ? false :
                (lhs[2] < rhs[2]) ? true : false;
        }
    };
    using OdGePoint3dMap = std::map<OdGePoint3d, std::size_t, OdGePoint3dCompare>;

    std::size_t appendPointGetIdx(OdGePoint3dMap& vertices, const OdGePoint3d& pt);

    std::shared_ptr<FacetModeler::Body> getSurfaceModel(OdIfc::OdIfcInstancePtr mappedItem);

	std::shared_ptr<FacetModeler::Body> getSweptSolid(OdIfc::OdIfcInstancePtr mappedItem);

    std::shared_ptr<FacetModeler::Body> createCylinder(
        const FacetModeler::DeviationParams& devDeviation,
        const OdGePoint2d& baseLocation,
        const OdGeVector2d& baseDirection,
        double radius,
        const OdGeVector3d& extrusionDirection,
        double height,
        const OdGeMatrix3d& rotation);

	std::vector<std::shared_ptr<FacetModeler::Body>> mGeometries;
	std::vector<int> mGeometryTypes;
};

