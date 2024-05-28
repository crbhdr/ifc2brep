# ifc2brep
* Convert specific IFC objects into custom BRep format

# Prerequisite
* Download the ODA IFC SDK

# Build
* Place the project under IFC Examples (local dir: vc16\Platforms\vc16_amd64dll\Ifc\Examples), next to the other examples ExIfcExtractData, ExIfcVectorize etc..
* Add the project under the solution IFC_vc16_amd64dll_ex.sln (local dir: vc16\Platforms\vc16_amd64dll)
* Build the project

# Run
* Go to the executable directory (local dir: /vc16/exe/vc16_amd64dll)
* Run `./IfcGeometriesProj.exe <input_ifc_filename> <output_brep_filename>`