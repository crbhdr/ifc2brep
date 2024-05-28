/////////////////////////////////////////////////////////////////////////////// 
// Copyright (C) 2002-2024, Open Design Alliance (the "Alliance"). 
// All rights reserved. 
// 
// This software and its documentation and related materials are owned by 
// the Alliance. The software may only be incorporated into application 
// programs owned by members of the Alliance, subject to a signed 
// Membership Agreement and Supplemental Software License Agreement with the
// Alliance. The structure and organization of this software are the valuable  
// trade secrets of the Alliance and its suppliers. The software is also 
// protected by copyright law and international treaty provisions. Application  
// programs incorporating this software must include the following statement 
// with their copyright notices:
//   
//   This application incorporates Open Design Alliance software pursuant to a license 
//   agreement with Open Design Alliance.
//   Open Design Alliance Copyright (C) 2002-2024 by Open Design Alliance. 
//   All rights reserved.
//
// By use of this software, its documentation or related materials, you 
// acknowledge and accept the above terms.
///////////////////////////////////////////////////////////////////////////////
/************************************************************************/
/* Simple application to load a Ifc file                                */
/* and save as STL geometry collection                                  */
/************************************************************************/

#include "OdaCommon.h"

#include "StaticRxObject.h"
#include "RxDynamicModule.h"

#include "IfcExamplesCommon.h"
#include "IfcCore.h"
#include "IfcGsManager.h"
#include "IfcGiContext.h"

#include "ExIfcHostAppServices.h"
#include "ExGsSimpleDevice.h"

#include "RxVariantValue.h"
#include "Gs/Gs.h"
#include "Gs/GsBaseInclude.h"
#include "ColorMapping.h"
#include "AbstractViewPE.h"

#include "ExPrintConsole.h"
#include "ExKeyPressCatcher.h"

#include "BrepGeometryModeler.h"
#include "AttributeHelper.h"


GS_TOOLKIT_EXPORT void odgsInitialize();
GS_TOOLKIT_EXPORT void odgsUninitialize();

//
// Define module map for statically linked modules:
//
#if !defined(_TOOLKIT_IN_DLL_)
  ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(OdSDAIModule);
  ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(OdIfcCoreModule);
  ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(OdIfc2x3Module);
  ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(OdIfc4Module);
  ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(OdIfcGeomModuleImpl);
  ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(OdIfcFacetModelerModule);
  ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(OdIfcBrepBuilderModule);
  ODRX_DECLARE_STATIC_MODULE_ENTRY_POINT(OdRxThreadPoolService);
  ODRX_BEGIN_STATIC_MODULE_MAP()
    ODRX_DEFINE_STATIC_APPMODULE(OdSDAIModuleName, OdSDAIModule)
    ODRX_DEFINE_STATIC_APPMODULE(OdIfcCoreModuleName, OdIfcCoreModule)
    ODRX_DEFINE_STATIC_APPMODULE(OdIfc2x3ModuleName, OdIfc2x3Module)
    ODRX_DEFINE_STATIC_APPMODULE(OdIfc4ModuleName, OdIfc4Module)
    ODRX_DEFINE_STATIC_APPMODULE(OdIfcGeomModuleName, OdIfcGeomModuleImpl)
    ODRX_DEFINE_STATIC_APPMODULE(OdIfcFacetModelerModuleName, OdIfcFacetModelerModule)
    ODRX_DEFINE_STATIC_APPMODULE(OdIfcBrepBuilderModuleName, OdIfcBrepBuilderModule)
    ODRX_DEFINE_STATIC_APPMODULE(OdThreadPoolModuleName, OdRxThreadPoolService)
  ODRX_END_STATIC_MODULE_MAP()
#endif




unsigned int inverse_aggregates = 0;
unsigned int empty_inverse_aggregates = 0;

void dumpEntity(OdIfc::OdIfcInstance* inst, OdDAI::Entity* entityDef)
{
    if (inst == nullptr)
        return;

    const OdDAI::List<OdDAI::Entity*>& superEntities = entityDef->supertypes();
    OdDAI::ConstIteratorPtr it_super = superEntities.createConstIterator();
    for (std::int16_t itN = 0; it_super->next(); ++itN)
    {
        OdDAI::Entity* pEntity;
        it_super->getCurrentMember() >> pEntity;
        OdString entName2 = pEntity->name();
        odPrintConsoleString(OD_T(" = <%d : found a super type.. dumping it.. %s>\n"), itN, entName2.c_str());
        dumpEntity(inst, pEntity);
    }

    OdString entName = entityDef->name();
    OdString strAbstract = entityDef->instantiable() ? OD_T("INSTANTIABLE") : OD_T("ABSTRACT");
    odPrintConsoleString(OD_T("\t%s %s\n"), strAbstract.c_str(), entName.c_str());
    for (OdDAI::ConstIteratorPtr it = entityDef->attributes().createConstIterator(); it->next();)
    {

        OdDAI::AttributePtr pAttr;
        it->getCurrentMember() >> pAttr;

        OdString strAttrType = OD_T("UNKNOWN");
        switch (pAttr->getAttributeType())
        {
        case OdDAI::AttributeType::Explicit:
        {
            strAttrType = OD_T("EXPLICIT");
        }
        break;
        case OdDAI::AttributeType::Inverse:
        {
            strAttrType = OD_T("INVERSE");
        }
        break;
        case OdDAI::AttributeType::Derived:
        {
            strAttrType = OD_T("DERIVED");
        }
        break;
        }

        const OdAnsiString& attrName = pAttr->name();
        odPrintConsoleString(OD_T("\t\t%s .%hs"), strAttrType.c_str(), attrName.c_str());


        // #define _DUMP_DERIVED_ATTRIBUTES
#ifndef _DUMP_DERIVED_ATTRIBUTES
        if (pAttr->getAttributeType() == OdDAI::AttributeType::Derived)
        {
            odPrintConsoleString(OD_T(" = <Coming Soon>\n"));
            continue;
        }
#endif

        OdRxValue val;
#ifndef _DUMP_DERIVED_ATTRIBUTES
        val = inst->getAttr(attrName);
#else
        val = pDerived.isNull() ? inst->getAttr(attrName) : inst->getDerivedAttr(attrName);
#endif

        bool unset = false;
        const OdRxValueType& vt = val.type();

        if (vt == OdRxValueType::Desc<OdDAIObjectId>::value())
        {
            OdDAIObjectId idVal;
            if (val >> idVal)
            {
                unset = OdDAI::Utils::isUnset(idVal);
                if (!unset)
                {
                    OdUInt64 int64 = idVal.getHandle();
                    odPrintConsoleString(OD_T(" = #%d"), int64);
                }
            }
        }
        else
            if (vt == OdRxValueType::Desc<OdDAI::CompressedGUID>::value())
            {
                OdDAI::CompressedGUID guidVal;
                if (val >> guidVal)
                {
                    unset = OdDAI::Utils::isUnset(guidVal);
                    if (!unset)
                    {
                        odPrintConsoleString(OD_T(" = '%s'"), OdString(guidVal).c_str());
                    }
                }
            }
            else
                if (vt == OdRxValueType::Desc<int>::value())
                {
                    int intVal;
                    if (val >> intVal)
                    {
                        unset = OdDAI::Utils::isUnset(intVal);
                        if (!unset)
                            odPrintConsoleString(OD_T(" = %i"), intVal);
                    }
                }
                else
                    if (vt == OdRxValueType::Desc<double>::value())
                    {
                        double dblVal;
                        if (val >> dblVal)
                        {
                            unset = OdDAI::Utils::isUnset(dblVal);
                            if (!unset)
                                odPrintConsoleString(OD_T(" = %.6f"), dblVal);
                        }
                    }
                    else
                        if (vt == OdRxValueType::Desc<const char*>::value())
                        {
                            const char* strVal;
                            if (val >> strVal)
                            {
                                unset = OdDAI::Utils::isUnset(strVal);
                                if (!unset)
                                {
                                    OdString strW = strVal;
                                    odPrintConsoleString(OD_T(" = '%s'"), strW.c_str());
                                }
                            }
                        }
                        else
                            if (vt.isEnum())
                            {
                                const char* strVal = nullptr;
                                if (val >> strVal)
                                {
                                    unset = (strVal == nullptr);
                                    if (!unset)
                                    {
                                        odPrintConsoleString(OD_T(" = .%s."), strVal);
                                    }
                                }

                                // or:

                                //OdDAI::EnumValueInfo enumVal;
                                //if (val >> enumVal)
                                //{
                                //  unset = (enumVal.value == NULL);
                                //  if (!unset)
                                //  {
                                //    odPrintConsoleString(L" = .%hs.", enumVal.value);
                                //  }
                                //}
                            }
                            else
                                if (vt.isSelect())
                                {
                                    OdTCKind selectKind;
                                    if (val >> selectKind)
                                    {
                                        unset = (selectKind == tkNull);
                                        if (!unset)
                                        {
                                            OdString typePath = val.typePath();
                                            odPrintConsoleString(OD_T(" = %s("), typePath.c_str());

                                            switch (selectKind)
                                            {
                                            case tkObjectId: // An object identifier's value.
                                            {
                                                OdDAIObjectId idVal;
                                                if (val >> idVal)
                                                {
                                                    OdUInt64 int64;
                                                    int64 = idVal.getHandle();
                                                    odPrintConsoleString(OD_T("#%d"), int64);
                                                }
                                                break;
                                            }
                                            case tkLong: // An unsigned 32-bit integer value.
                                            {
                                                int intVal;
                                                if (val >> intVal)
                                                    odPrintConsoleString(OD_T("%i"), intVal);
                                                break;
                                            }
                                            case tkBoolean: // A boolean value.
                                            {
                                                bool boolVal;
                                                if (val >> boolVal)
                                                {
                                                    OdString strBool;
                                                    strBool = boolVal ? OD_T("true") : OD_T("false");
                                                    odPrintConsoleString(OD_T("%s"), strBool.c_str());
                                                }
                                                break;
                                            }
                                            case tkDouble: // A double value.
                                            {
                                                double dVal;
                                                if (val >> dVal)
                                                    odPrintConsoleString(OD_T("%.6f"), dVal);
                                                break;
                                            }
                                            case tkBinary: // A binary value.
                                            case tkLogical: // A logical value.
                                                break;
                                            case tkString:
                                            {
                                                OdAnsiString strVal;
                                                if (val >> strVal)
                                                {
                                                    OdString wcsVal = strVal;
                                                    odPrintConsoleString(OD_T("'%s'"), wcsVal.c_str());
                                                }
                                                break;
                                            }
                                            case tkSequence:
                                            {
                                                odPrintConsoleString(OD_T("TODO: kSequence not implemented yet"));
                                                break;
                                            }
                                            default:
                                                odPrintConsoleString(OD_T("Not implemented yet."));
                                            }
                                            odPrintConsoleString(OD_T(")"));
                                        }
                                    }
                                }
                                else
                                    //if (vt == OdRxValueType::Desc<OdDAIObjectIds>::value()
                                    //  || vt == OdRxValueType::Desc<OdDAI::Aggr<OdDAIObjectId>* >::value())
                                    if (vt.isAggregate())
                                    {
                                        OdDAI::Aggr* aggr = NULL;
                                        if (val >> aggr)
                                        {
                                            unset = aggr->isNil();
                                            if (!unset)
                                            {
                                                odPrintConsoleString(OD_T(" = "));

                                                OdDAI::AggrType aggrType = aggr->aggrType();
                                                switch (aggrType)
                                                {
                                                case OdDAI::aggrTypeArray:
                                                    odPrintConsoleString(OD_T("ARRAY"));
                                                    break;
                                                case OdDAI::aggrTypeBag:
                                                    odPrintConsoleString(OD_T("BAG"));
                                                    break;
                                                case OdDAI::aggrTypeList:
                                                    odPrintConsoleString(OD_T("LIST"));
                                                    break;
                                                case OdDAI::aggrTypeSet:
                                                    odPrintConsoleString(OD_T("SET"));
                                                    break;
                                                }

                                                odPrintConsoleString(OD_T("["));
                                                OdDAI::IteratorPtr iterator = aggr->createIterator();
                                                for (iterator->beginning(); iterator->next();)
                                                {
                                                    OdRxValue val = iterator->getCurrentMember();
                                                    OdString strVal = val.toString();
                                                    odPrintConsoleString(OD_T(" %s "), strVal.c_str());
                                                }
                                                odPrintConsoleString(OD_T("]"));
                                            }
                                        }
                                    }
                                    else
                                    {
                                        //
                                        // Deprecated: OdArray instead of Aggregate
                                        //
                                        OdDAIObjectIds idsVal;
                                        if (val >> idsVal)
                                        {
                                            ++inverse_aggregates;
                                            unset = (idsVal.size() == 0);
                                            if (!unset)
                                            {
                                                OdUInt64 int64;
                                                int64 = idsVal[0].getHandle();
                                                odPrintConsoleString(OD_T(" = (#%d"), int64);
                                                for (unsigned int i = 1; i < idsVal.size(); ++i)
                                                {
                                                    int64 = idsVal[i].getHandle();
                                                    odPrintConsoleString(OD_T(", #%d"), int64);
                                                }
                                                odPrintConsoleString(OD_T(")"));
                                            }
                                            else
                                                ++empty_inverse_aggregates;
                                        }
                                    }

        if (unset)
            odPrintConsoleString(OD_T(" = UNSET"));

        odPrintConsoleString(OD_T("\n"));
    }
}


/************************************************************************/
/* Main                                                                 */
/************************************************************************/
#if defined(OD_USE_WMAIN)
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
  int   nRes = 0;

#ifdef OD_HAVE_CCOMMAND_FUNC
  argc = ccommand(&argv);
#endif

  OdStaticRxObject<MyServices> svcs;
  odPrintConsoleString(OD_T("\nExIfcVectorize sample program. Copyright (c) 2022, Open Design Alliance\n"));
  bool bInvalidArgs = (argc < 2);

  if (bInvalidArgs)
  {
    bInvalidArgs = true;
    nRes  = 1;
  }

  if (bInvalidArgs)    
  {
    odPrintConsoleString(OD_T("\n\tusage: ExIfcVectorize <filename> [stlFilename] [-DO]"));
    odPrintConsoleString(OD_T("\n\t-DO disables progress meter output.\n"));
    return nRes;
  }

  OdString szSource = argv[1];
  OdString strBrepFilename = OdString::kEmpty;

  if (argc > 2)
  {
    strBrepFilename = argv[2];
    if (strBrepFilename == OD_T("-DO"))
      strBrepFilename = OdString::kEmpty;
  }

#if !defined(_TOOLKIT_IN_DLL_)
  ODRX_INIT_STATIC_MODULE_MAP();
#endif

  odrxInitialize(&svcs);
  odgsInitialize();
  odIfcInitialize(false /* No CDA */, true);

  try
  {
    OdIfcFilePtr pDatabase = svcs.createDatabase();
    
    if (pDatabase->readFile(szSource) != eOk)
    {
      throw OdError( eCantOpenFile );
    }

    BrepGeometryModeler brepGeometryModeler;

    OdIfcModelPtr pModel = pDatabase->getModel();
    OdDAI::InstanceIteratorPtr it = pModel->newIterator();
    OdIfc::OdIfcInstancePtr pInst;
    unsigned int entIdx;
    for (entIdx = 0; !it->done(); it->step(), ++entIdx)
    {
        // Opens an instance
        pInst = it->id().openObject();

        if (!pInst.isNull())
        {
            const char* globalid;
            pInst->getAttr("globalid") >> globalid;
            OdString strGlobalid = globalid;
            if (strGlobalid != "0f7I2_mxX3JOk$Z$4oj$LI") continue;

            // Don't dump --> dumpEntity(pInst, pInst->getInstanceType());

            OdIfc::OdIfcInstancePtr objectPlacement = AttributeHelper::getAttributeAsInstance(pInst, "objectplacement");
                OdIfc::OdIfcInstancePtr relativePlacement = AttributeHelper::getAttributeAsInstance(objectPlacement, "relativeplacement");
                    OdIfc::OdIfcInstancePtr location = AttributeHelper::getAttributeAsInstance(relativePlacement, "location");
                    OdIfc::OdIfcInstancePtr axis = AttributeHelper::getAttributeAsInstance(relativePlacement, "axis");
                    OdIfc::OdIfcInstancePtr refdirection = AttributeHelper::getAttributeAsInstance(relativePlacement, "refdirection");
                    {
                        OdGeVector3d coordinates = AttributeHelper::getVector<OdGeVector3d>(location, "coordinates");
                        OdGeVector3d directionratiosAxis = AttributeHelper::getVector<OdGeVector3d>(axis, "directionratios");
                        OdGeVector3d directionratiosRefdirection = AttributeHelper::getVector<OdGeVector3d>(refdirection, "directionratios");
                        // Don't dump --> std::cout << coordinates.x << " , " << coordinates.y << " , " << coordinates.z << std::endl;
                        // Don't dump --> std::cout << directionratiosAxis.x << " , " << directionratiosAxis.y << " , " << directionratiosAxis.z << std::endl;
                        // Don't dump --> std::cout << directionratiosRefdirection.x << " , " << directionratiosRefdirection.y << " , " << directionratiosRefdirection.z << std::endl;
                    }

            OdIfc::OdIfcInstancePtr representation = AttributeHelper::getAttributeAsInstance(pInst, "representation");                                          // Don't dump --> dumpEntity(representation, representation->getInstanceType());
                std::vector<OdIfc::OdIfcInstancePtr> representations = AttributeHelper::getAttributeAsInstanceVector(representation, "representations");            // Don't dump --> dumpEntity(representations[0], representations[0]->getInstanceType());
                    std::vector<OdIfc::OdIfcInstancePtr> items = AttributeHelper::getAttributeAsInstanceVector(representations[0], "items");                            // Don't dump --> dumpEntity(items[0], items[0]->getInstanceType());
                        // Surface Model
                        {
                            OdIfc::OdIfcInstancePtr mappingsource = AttributeHelper::getAttributeAsInstance(items[0], "mappingsource");                                         // Don't dump --> dumpEntity(mappingsource, mappingsource->getInstanceType());
                            OdIfc::OdIfcInstancePtr mappedrepresentation = AttributeHelper::getAttributeAsInstance(mappingsource, "mappedrepresentation");                      // Don't dump --> dumpEntity(mappedrepresentation, mappedrepresentation->getInstanceType());
                            std::vector<OdIfc::OdIfcInstancePtr> mappedItems = AttributeHelper::getAttributeAsInstanceVector(mappedrepresentation, "items");
                            for (const auto& mappedItem : mappedItems)
                            {
                                brepGeometryModeler.addMappedItemAsSurface(mappedItem);
                            }
                        }
                        // SweptSolid
                        {
                            OdIfc::OdIfcInstancePtr mappingsource = AttributeHelper::getAttributeAsInstance(items[1], "mappingsource");
                            OdIfc::OdIfcInstancePtr mappedrepresentation = AttributeHelper::getAttributeAsInstance(mappingsource, "mappedrepresentation");
                            std::vector<OdIfc::OdIfcInstancePtr> mappedItems = AttributeHelper::getAttributeAsInstanceVector(mappedrepresentation, "items");
                            for (const auto& mappedItem : mappedItems)
                            {
                                brepGeometryModeler.addMappedItemAsSolid(mappedItem);
                            }
                        }
            std::cout << "length of items: " << items.size() << std::endl;
        }
    }

    brepGeometryModeler.postProcessGeometries(strBrepFilename); // BC!!! not only the front element, process all elements
    
  }
  catch (OdError& e)
  {
    odPrintConsoleString(OD_T("\n\nError: %ls"), e.description().c_str());
    nRes = -1;
  }
  catch (...)
  {
    odPrintConsoleString(OD_T("\n\nUnexpected error."));
    nRes = -1;
    throw;
  }

  odIfcUninitialize();
  odgsUninitialize();
  odrxUninitialize();

  return nRes;
}