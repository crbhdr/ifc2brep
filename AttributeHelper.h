#pragma once

#include "OdaCommon.h"

#include "IfcExamplesCommon.h"
#include "IfcCore.h"
#include "IfcGsManager.h"
#include "IfcGiContext.h"
#include "GiDumper.h"

#include "Modeler/FMMdlBody.h"
#include "FMProfile3D.h"
#include "FMDataSerialize.h"
#include "Modeler/FMMdlIterators.h"

class AttributeHelper
{
public:

    static double getDouble(OdIfc::OdIfcInstancePtr inst, const char* attr) {
        double ret;
        OdRxValue val = inst->getAttr(attr);
        val >> ret;
        return ret;
    }

    template<typename T>
    static T getVector(OdIfc::OdIfcInstancePtr coord, const char* componentsName) {
        T ret;
        OdRxValue components = coord->getAttr(componentsName);
        OdDAI::Aggr* aggr = NULL;
        components >> aggr;
        OdDAI::IteratorPtr iterator = aggr->createIterator();
        int i = 0;
        for (iterator->beginning(); iterator->next(); i++)
        {
            OdRxValue val = iterator->getCurrentMember();
            double coord;
            val >> coord;
            ret[i] = coord;
        }
        return ret;
    }

    template<typename T>
    static T getVector(OdIfc::OdIfcInstancePtr pInst, const char* attrName, const char* componentsName) {
        OdDAIObjectId retId;
        OdRxValue extrudedDirectionVal = pInst->getAttr(attrName);
        if (extrudedDirectionVal.isEmpty()) {
            //std::cerr << std::format("Can't find {}", attrName).c_str() << std::endl;
            return{};
        }
        if (!(extrudedDirectionVal >> retId)) {
            //std::cerr << std::format("Can't find {}", attrName).c_str() << std::endl;
            return{};
        }
        OdIfc::OdIfcInstancePtr direction = retId.openObject();
        if (direction.isNull()) {
            //std::cerr << std::format("Null {}", attrName).c_str() << std::endl;
            return{};
        }
        return getVector<T>(direction, componentsName);
    }

    static OdIfc::OdIfcInstancePtr getAttributeAsInstance(OdIfc::OdIfcInstance* inst, const char* attr)
    {
        OdRxValue val = inst->getAttr(attr);
        OdDAIObjectId id;
        val >> id;
        return id.openObject();
    }

    static std::vector<OdIfc::OdIfcInstancePtr> getAttributeAsInstanceVector(OdIfc::OdIfcInstance* inst, const char* attr)
    {
        std::vector<OdIfc::OdIfcInstancePtr> ret;
        OdRxValue components = inst->getAttr(attr);
        OdDAI::Aggr* aggr = NULL;
        components >> aggr;
        OdDAI::IteratorPtr iterator = aggr->createIterator();
        int i = 0;
        for (iterator->beginning(); iterator->next(); i++)
        {
            OdRxValue val = iterator->getCurrentMember();
            OdDAIObjectId id;
            val >> id;
            ret.push_back(id.openObject());
        }
        return ret;
    }

};

