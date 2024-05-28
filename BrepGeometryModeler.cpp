#include "BrepGeometryModeler.h"
#include "AttributeHelper.h"

void BrepGeometryModeler::addMappedItemAsSurface(OdIfc::OdIfcInstancePtr mappedItem)
{
    mGeometries.emplace_back(getSurfaceModel(mappedItem));
    mGeometryTypes.push_back(GeometryTypeEnum::GeometryTypeSurface);
}

void BrepGeometryModeler::addMappedItemAsSolid(OdIfc::OdIfcInstancePtr mappedItem)
{
    mGeometries.emplace_back(getSweptSolid(mappedItem));
    mGeometryTypes.push_back(GeometryTypeEnum::GeometryTypeSolid);
}

void BrepGeometryModeler::postProcessGeometries(const OdString& strBrepFilename)
{
    OdGePoint3dMap vertices;
    std::vector<std::array<std::size_t, 2>> edgeIndices;
    std::vector<double> edgeLengths;
    std::vector<double> faceAreas;
    for (const auto& body : mGeometries)
    {
        FacetModeler::Vertex* vertex = body->vertexList();
        for (std::size_t i = 0; i < body->vertexCount(); ++i, vertex = vertex->next())
        {
            std::size_t id = appendPointGetIdx(vertices, vertex->point());
        }

        FacetModeler::Face* face = body->faceList();
        for (std::size_t i = 0; i < body->faceCount(); ++i, face = face->next())
        {
            FacetModeler::Edge* edge = face->edge();
            for (std::size_t j = 0; j < face->loopEdgeCount(); ++j, edge = edge->next())
            {
                std::array<std::size_t, 2> edgeIdx = { vertices[edge->startPoint()], vertices[edge->endPoint()] };
                edgeIndices.push_back(edgeIdx);
                edgeLengths.push_back(edge->length());
            }
            faceAreas.push_back(face->area());
        }
    }

    // Dump to json
    nlohmann::json brepDoc;

    std::size_t solidIdx = 0;
    for (const auto& geometryType : mGeometryTypes)
    {
        if (geometryType == GeometryTypeEnum::GeometryTypeSurface)
        {
            const nlohmann::json geometry{ {"shells", {0, 1, 2, 93} } };
            brepDoc["geometries"].push_back(geometry);
        }
        else
        {
            const nlohmann::json geometry{ {"solids", solidIdx++ } };
            brepDoc["geometries"].push_back(geometry);
        }
    }

    for (const auto& vertex : vertices)
    {
        const nlohmann::json pos{ {"position", nlohmann::json::array({ vertex.first[0], vertex.first[1], vertex.first[2] }) } };
        brepDoc["vertices"].push_back(pos);
    }

    for (std::size_t i = 0; i < edgeIndices.size(); ++i)
    {
        const nlohmann::json edge{ { "vertices", nlohmann::json::array({edgeIndices[i][0], edgeIndices[i][1] }) },
                                   { "arcLength", edgeLengths[i] } };
        brepDoc["edges"].push_back(edge);
    }

    for (std::size_t i = 0; i < faceAreas.size(); ++i)
    {
        const nlohmann::json face{ {"area", faceAreas[i] } };
        brepDoc["faces"].push_back(face);
    }

    std::ofstream brepFileStream(strBrepFilename.c_str(), std::ofstream::out);
    brepFileStream << std::setw(4) << brepDoc;
}

void BrepGeometryModeler::postProcessTriangles(const OdArray<OdIfcStlTriangleFace>& arrTriangles, const OdString& strBrepFilename)
{
    OdGePoint3dMap vertices;
    std::vector<std::array<std::size_t, 3>> triangleIndices;
    for (const auto& triangle : arrTriangles)
    {
        std::size_t id1 = appendPointGetIdx(vertices, triangle.m_pt1);
        std::size_t id2 = appendPointGetIdx(vertices, triangle.m_pt2);
        std::size_t id3 = appendPointGetIdx(vertices, triangle.m_pt3);

        std::array<std::size_t, 3> arr = { id1, id2, id3 };
        triangleIndices.push_back(arr);
    }

    std::cout << "vertices size = " << vertices.size() << std::endl;
    std::cout << "triangleIndices size = " << triangleIndices.size() << std::endl;

    nlohmann::json brepDoc;
    for (const auto& vertex : vertices)
    {
        const nlohmann::json pos{ "position", nlohmann::json::array({vertex.first[0], vertex.first[1], vertex.first[2]}) };
        brepDoc["vertices"].push_back(pos);
    }

    for (const auto& indices : triangleIndices)
    {
        const nlohmann::json ind{ "indices", nlohmann::json::array({indices[0], indices[1], indices[2]}) };
        brepDoc["faces"].push_back(ind);
    }


    std::ofstream brepFileStream(strBrepFilename.c_str(), std::ofstream::out);
    brepFileStream << std::setw(4) << brepDoc;
}

std::size_t BrepGeometryModeler::appendPointGetIdx(OdGePoint3dMap& vertices, const OdGePoint3d& pt)
{
    std::size_t idx = 0;
    auto it = vertices.find(pt);
    if (it != vertices.end())
    {
        idx = it->second;
    }
    else
    {
        idx = vertices.size();
        vertices.emplace(pt, idx);
    }

    return idx;
}

std::shared_ptr<FacetModeler::Body> BrepGeometryModeler::getSurfaceModel(OdIfc::OdIfcInstancePtr mappedItem)
{
    std::vector<OdIfc::OdIfcInstancePtr> sbsmboundaries = AttributeHelper::getAttributeAsInstanceVector(mappedItem, "sbsmboundary");
    BoundaryFaces boundaryFaces;
    for (auto& sbsmboundary : sbsmboundaries)
    {
        std::vector<OdIfc::OdIfcInstancePtr> faces = AttributeHelper::getAttributeAsInstanceVector(sbsmboundary, "cfsfaces");
        FaceBounds faceBounds;
        for (auto& face : faces)
        {
            std::vector<OdIfc::OdIfcInstancePtr> bounds = AttributeHelper::getAttributeAsInstanceVector(face, "bounds");
            BoundPolygons boundPolygons;
            for (auto& bound : bounds)
            {
                OdIfc::OdIfcInstancePtr boundLoop = AttributeHelper::getAttributeAsInstance(bound, "bound");
                std::vector<OdIfc::OdIfcInstancePtr> polygons = AttributeHelper::getAttributeAsInstanceVector(boundLoop, "polygon");
                PolygonCoordinates polygonCoordinates;
                for (auto& polygon : polygons)
                {
                    polygonCoordinates.push_back(AttributeHelper::getVector<OdGeVector3d>(polygon, "coordinates"));
                }
                boundPolygons.push_back(polygonCoordinates);
            }
            faceBounds.push_back(boundPolygons);
        }
        boundaryFaces.push_back(faceBounds);
    }

    OdGePoint3dMap vertices;
    std::vector<std::array<std::size_t, 3>> triangleIndices;
    std::cout << "printing mapped surface" << std::endl;
    std::cout << "\tlength of boundaryFaces: " << boundaryFaces.size() << std::endl;

    // Fill in the Point3d map in order to prevent duplications
    // And use map indices as triangle indices
    for (const FaceBounds& boundaryFace : boundaryFaces)
    {
        for (const BoundPolygons& boundPolygon : boundaryFace)
        {
            for (const PolygonCoordinates& polyCoords : boundPolygon)
            {
                std::size_t id1 = appendPointGetIdx(vertices, polyCoords[0].asPoint());
                std::size_t id2 = appendPointGetIdx(vertices, polyCoords[1].asPoint());
                std::size_t id3 = appendPointGetIdx(vertices, polyCoords[2].asPoint());
                std::array<std::size_t, 3> arr = { id1, id2, id3 };
                triangleIndices.push_back(arr);
            }
        }
    }

    std::vector<OdGePoint3d> verticesVector(vertices.size());
    for (const auto& vertex : vertices)
    {
        verticesVector[vertex.second] = vertex.first;
    }

    std::vector<OdInt32> faceData;
    for (const auto& indices : triangleIndices)
    {
        faceData.push_back(3);
        faceData.push_back(indices[0]);
        faceData.push_back(indices[1]);
        faceData.push_back(indices[2]);
    }

    if(0)
    {
        std::vector<OdGePoint3d> aVertices{
      OdGePoint3d(77.0, 0.0,  0.0),
      OdGePoint3d(0.0,  70.0, 0.0),
      OdGePoint3d(77.0, 77.0, 0.0),
      OdGePoint3d(0.0,  77.0, 77.0),
        };

        // Create array with face data
        std::vector<OdInt32> aFaceData{
          3, 3, 2, 1,
          3, 1, 2, 0,
          3, 2, 3, 0,
          3, 3, 1, 0,
        };
        auto bd = FacetModeler::Body::createFromMesh(aVertices, aFaceData);
        std::cout << "\tbd.faceCount = " << bd.faceCount() << std::endl;
    }

    /* Don't dump --> int n = 0;
    for (const auto& v : verticesVector)
    {
        std::cout << n++ << " : " << v[0] << ", " << v[1] << ", " << v[2] << std::endl;
    }
    for (std::size_t i = 0; i < faceData.size(); ++i)
    {
        std::cout << faceData[i];
        if (i % 4 == 3)
        {
            std::cout << std::endl;
        }
        else
        {
            std::cout << ", ";
        }
    }*/
    auto surfaceFromFile = FacetModeler::Body::createFromMesh(verticesVector, faceData);
    std::cout << "\tsurfaceFromFile .faceCount = " << surfaceFromFile.faceCount() << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;

    return std::make_shared<FacetModeler::Body>(FacetModeler::Body::createFromMesh(verticesVector, faceData));
}

std::shared_ptr<FacetModeler::Body> BrepGeometryModeler::getSweptSolid(OdIfc::OdIfcInstancePtr mappedItem)
{
    std::cout << "printing mapped solid" << std::endl;
    // Base circle
    OdIfc::OdIfcInstancePtr sweptarea = AttributeHelper::getAttributeAsInstance(mappedItem, "sweptarea");
    OdIfc::OdIfcInstancePtr sweptareaPosition = AttributeHelper::getAttributeAsInstance(sweptarea, "position");
    double r = AttributeHelper::getDouble(sweptarea, "radius");
    OdGeVector2d locationCircleCoord = AttributeHelper::getVector<OdGeVector2d>(sweptareaPosition, "location", "coordinates");
    OdGeVector2d directionratiosrefDirCircle = AttributeHelper::getVector<OdGeVector2d>(sweptareaPosition, "refdirection", "directionratios");
    std::cout << "\tradius = " << r << std::endl;
    std::cout << "\tlocation: " << locationCircleCoord.x << " , " << locationCircleCoord.y << std::endl;
    std::cout << "\tdirection: " << directionratiosrefDirCircle.x << " , " << directionratiosrefDirCircle.y << std::endl;

    // Solid Cylinder
    double depth = AttributeHelper::getDouble(mappedItem, "depth");
    OdGeVector3d dir = AttributeHelper::getVector<OdGeVector3d>(mappedItem, "extrudeddirection", "directionratios");
    OdIfc::OdIfcInstancePtr position = AttributeHelper::getAttributeAsInstance(mappedItem, "position");
    OdGeVector3d center = AttributeHelper::getVector<OdGeVector3d>(position, "location", "coordinates");
    OdGeVector3d z_axis = AttributeHelper::getVector<OdGeVector3d>(position, "axis", "directionratios");
    OdGeVector3d y_axis = AttributeHelper::getVector<OdGeVector3d>(position, "refdirection", "directionratios");
    OdGeVector3d x_axis = z_axis.crossProduct(y_axis);
    OdGeMatrix3d rotation = OdGeMatrix3d();
    rotation.setCoordSystem(center.asPoint(), x_axis, y_axis, z_axis);

    std::cout << "\tdepth: " << depth << std::endl;
    std::cout << "\text.dir: " << dir.x << " , " << dir.y << " , " << dir.z << std::endl;
    std::cout << "\tcenter: " << center.x << " , " << center.y << " , " << center.z << std::endl;
    std::cout << "\tx_axis: " << x_axis.x << " , " << x_axis.y << " , " << x_axis.z << std::endl;
    std::cout << "\ty_axis: " << y_axis.x << " , " << y_axis.y << " , " << y_axis.z << std::endl;
    std::cout << "\tz_axis: " << z_axis.x << " , " << z_axis.y << " , " << z_axis.z << std::endl;
    std::cout << std::endl;

    FacetModeler::DeviationParams devParams;
    std::shared_ptr<FacetModeler::Body> body = createCylinder(
        devParams,
        locationCircleCoord.asPoint(),
        directionratiosrefDirCircle,
        r,
        dir,
        depth,
        rotation);

    return body;
}

std::shared_ptr<FacetModeler::Body> BrepGeometryModeler::createCylinder(const FacetModeler::DeviationParams& devDeviation, const OdGePoint2d& baseLocation, const OdGeVector2d& baseDirection, double radius, const OdGeVector3d& extrusionDirection, double height, const OdGeMatrix3d& rotation)
{
    FacetModeler::Profile2D cBase;            // Create base profile
    cBase.resize(1);                          // With one contour

    cBase.front().appendVertex(baseLocation - baseDirection * radius, 1);
    cBase.front().appendVertex(baseLocation + baseDirection * radius, 1);

    cBase.front().setOrientationAt(0, FacetModeler::efoFront);
    cBase.front().setOrientationAt(1, FacetModeler::efoFront);

    cBase.front().setClosed();                // Close profile
    cBase.front().makeCCW();                  // Make contour outer

    return std::make_shared< FacetModeler::Body>(FacetModeler::Body::extrusion(cBase, rotation, extrusionDirection * height, devDeviation));
}
