// EnergyPlus, Copyright (c) 1996-2019, The Board of Trustees of the University of Illinois,
// The Regents of the University of California, through Lawrence Berkeley National Laboratory
// (subject to receipt of any required approvals from the U.S. Dept. of Energy), Oak Ridge
// National Laboratory, managed by UT-Battelle, Alliance for Sustainable Energy, LLC, and other
// contributors. All rights reserved.
//
// NOTICE: This Software was developed under funding from the U.S. Department of Energy and the
// U.S. Government consequently retains certain rights. As such, the U.S. Government has been
// granted for itself and others acting on its behalf a paid-up, nonexclusive, irrevocable,
// worldwide license in the Software to reproduce, distribute copies to the public, prepare
// derivative works, and perform publicly and display publicly, and to permit others to do so.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
// (1) Redistributions of source code must retain the above copyright notice, this list of
//     conditions and the following disclaimer.
//
// (2) Redistributions in binary form must reproduce the above copyright notice, this list of
//     conditions and the following disclaimer in the documentation and/or other materials
//     provided with the distribution.
//
// (3) Neither the name of the University of California, Lawrence Berkeley National Laboratory,
//     the University of Illinois, U.S. Dept. of Energy nor the names of its contributors may be
//     used to endorse or promote products derived from this software without specific prior
//     written permission.
//
// (4) Use of EnergyPlus(TM) Name. If Licensee (i) distributes the software in stand-alone form
//     without changes from the version obtained under this License, or (ii) Licensee makes a
//     reference solely to the software portion of its product, Licensee must refer to the
//     software as "EnergyPlus version X" software, where "X" is the version number Licensee
//     obtained under this License and may not use a different name for the software. Except as
//     specifically required in this Section (4), Licensee shall not use in a company name, a
//     product name, in advertising, publicity, or other promotional activities any name, trade
//     name, trademark, logo, or other designation of "EnergyPlus", "E+", "e+" or confusingly
//     similar designation, without the U.S. Department of Energy's prior written consent.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// C++ Headers
#include <cassert>
#include <cmath>
#include <iostream>

// ObjexxFCL Headers
#include <ObjexxFCL/Array.functions.hh>
#include <ObjexxFCL/Fmath.hh>
//#include <ObjexxFCL/gio.hh>
#include <ObjexxFCL/string.functions.hh>

// EnergyPlus Headers
#include <EnergyPlus/BranchNodeConnections.hh>
#include <EnergyPlus/Construction.hh>
#include <EnergyPlus/Data/EnergyPlusData.hh>
#include <EnergyPlus/DataBranchAirLoopPlant.hh>
#include <EnergyPlus/DataConversions.hh>
#include <EnergyPlus/DataEnvironment.hh>
#include <EnergyPlus/DataGlobals.hh>
#include <EnergyPlus/DataHVACGlobals.hh>
#include <EnergyPlus/DataHeatBalFanSys.hh>
#include <EnergyPlus/DataHeatBalSurface.hh>
#include <EnergyPlus/DataHeatBalance.hh>
#include <EnergyPlus/DataIPShortCuts.hh>
#include <EnergyPlus/DataPrecisionGlobals.hh>
#include <EnergyPlus/DataSizing.hh>
#include <EnergyPlus/DataSurfaceLists.hh>
#include <EnergyPlus/DataSurfaces.hh>
#include <EnergyPlus/FluidProperties.hh>
#include <EnergyPlus/General.hh>
#include <EnergyPlus/GlobalNames.hh>
#include <EnergyPlus/HeatBalanceSurfaceManager.hh>
#include <EnergyPlus/IndoorIceRink.hh>
#include <EnergyPlus/InputProcessing/InputProcessor.hh>
#include <EnergyPlus/NodeInputManager.hh>
#include <EnergyPlus/OutputProcessor.hh>
#include <EnergyPlus/Plant/DataPlant.hh>
#include <EnergyPlus/PlantUtilities.hh>
#include <EnergyPlus/Plant/PlantLocation.hh>
#include <EnergyPlus/Psychrometrics.hh>
#include <EnergyPlus/ScheduleManager.hh>
#include <EnergyPlus/UtilityRoutines.hh>
#include <EnergyPlus/ZoneTempPredictorCorrector.hh>

namespace EnergyPlus {
namespace IceRink {
    // USE STATEMENTS:
    // Use statements for data only modules
    // Using/Aliasing

    // Data
    // MODULE PARAMETER DEFINITIONS:
    // System types:
    static std::string const BlankString;
    std::string const cRink("IceRink:Indoor");
    std::string const cResurfacer("IceRink:Resurfacer");

    Real64 InletTemp;
    Real64 OutletTemp;
    Real64 FlowRate;
    Real64 HeatFlux;
    Real64 ResurfacingLoad;

    // Control types:
    int const SurfaceTempControl(1);     // Controls system using ice surface temperature
    int const BrineOutletTempControl(2); // Controls system using brine outlet temperature
    bool FirstTimeInit(true);

    int NumOfRinks(0);
    int NumOfResurfacers(0);

    bool GetInput(true);

    // Object Data
    Array1D<IceRinkData> Rink;
    Array1D<Real64> QRadSysSrcAvg;      // Average source over the time step for a particular radiant surface
    Array1D<Real64> LastQRadSysSrc;     // Need to keep the last value in case we are still iterating
    Array1D<Real64> LastSysTimeElapsed; // Need to keep the last value in case we are still iterating
    Array1D<Real64> LastTimeStepSys;    // Need to keep the last value in case we are still iterating
    using DataHeatBalFanSys::QRadSysSource;
    using DataSurfaces::TotSurfaces;

    // Functions:
    void clear_state()
    {
        NumOfRinks = 0;
        QRadSysSrcAvg.deallocate();
        GetInput = true;
        Rink.deallocate();
        LastQRadSysSrc.deallocate();
        LastSysTimeElapsed.deallocate();
        LastTimeStepSys.deallocate();
        FirstTimeInit = true;
    }

    PlantComponent *IceRinkData::factory(EnergyPlusData &state, std::string const &objectName)
    {
        // Process the input data for ice rinks if it hasn't been done yet
        if (GetInput) {
            GetIndoorIceRink(state);
            // GetResurfacer();
            GetInput = false;
        }
        // Now look for this particular rink in the list
        for (auto &rink : Rink) {
            if (rink.Name == objectName) {
                return &rink;
            }
        }
        // If it is not found, fatal error
        ShowFatalError(state, "IceRinkFactory: Error getting inputs for rink named: " + objectName);
        // Shut up the compiler
        return nullptr;
    }

    void IceRinkData::simulate(EnergyPlusData &state,
                               [[maybe_unused]] const PlantLocation &calledFromLocation,
                               [[maybe_unused]] bool FirstHVACIteration,
                               Real64 &CurLoad,
                               bool const RunFlag)
    {
        this->initialize(state);
        if (this->RinkType_Num == DataPlant::TypeOf_IceRink) {
            this->calculateIceRink(state, CurLoad, false); //"false" is for unit testing
        }

        this->update();
        this->report(RunFlag);
    }

    void GetIndoorIceRink(EnergyPlusData &state)
    {
        // Using/Aliasing
        using namespace DataIPShortCuts; // Data for field names, blank numerics
        using BranchNodeConnections::TestCompSet;
        using DataGlobalConstants::ScheduleAlwaysOn;
        using DataHeatBalance::Zone;
        using namespace DataSurfaces;
        using FluidProperties::FindGlycol;
        using FluidProperties::FindRefrigerant;
        using NodeInputManager::GetOnlySingleNode;
        using ScheduleManager::GetScheduleIndex;
        using UtilityRoutines::FindItemInList;
        using namespace DataLoopNode;
        using namespace DataSurfaceLists;

        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("GetIndoorIceRink: ");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        static bool ErrorsFound(false); // Set to true if errors in input,
        int IOStatus;                   // Used in GetObjectItem
        int Item;                       // Item to be "gotten"
        int NumAlphas;                  // Number of Alphas for each GetObjectItem call
        int NumNumbers;                 // Number of Numbers for each GetObjectItem call
        // struct IceRinkData OutputData;

        NumOfRinks = inputProcessor->getNumObjectsFound(state, cRink);

        if (NumOfRinks <= 0) ShowFatalError(state, "No Rink objects found in input.");
        // GetInput = false;

        if (allocated(Rink)) Rink.deallocate();
        Rink.allocate(NumOfRinks);

        // Obtain all the user data related to rinks
        for (Item = 1; Item <= NumOfRinks; ++Item) {
            cCurrentModuleObject = "IceRink:Indoor";
            inputProcessor->getObjectItem(state,
                                          cCurrentModuleObject,
                                          Item,
                                          cAlphaArgs,
                                          NumAlphas,
                                          rNumericArgs,
                                          NumNumbers,
                                          IOStatus,
                                          _,
                                          _,
                                          cAlphaFieldNames,
                                          cNumericFieldNames);

            Rink(Item).Name = cAlphaArgs(1);
            Rink(Item).SchedName = cAlphaArgs(2);
            if (lAlphaFieldBlanks(2)) {
                Rink(Item).SchedPtr = ScheduleAlwaysOn;
            } else {
                Rink(Item).SchedPtr = GetScheduleIndex(state, cAlphaArgs(2));
                if (Rink(Item).SchedPtr == 0) {
                    ShowSevereError(state, cAlphaFieldNames(2) + " not found for " + cAlphaArgs(1));
                    ShowContinueError(state, "Missing " + cAlphaFieldNames(2) + " is " + cAlphaArgs(2));
                    ErrorsFound = true;
                }
            }

            Rink(Item).ZoneName = cAlphaArgs(3);
            Rink(Item).ZonePtr = FindItemInList(cAlphaArgs(3), Zone);
            if (Rink(Item).ZonePtr == 0) {
                ShowSevereError(state, RoutineName + "Invalid " + cAlphaFieldNames(3) + " = " + cAlphaArgs(3));
                ShowContinueError(state, "Occurs in " + cCurrentModuleObject + " = " + cAlphaArgs(1));
                ErrorsFound = true;
            }

            Rink(Item).SurfaceName = cAlphaArgs(4);
            Rink(Item).SurfacePtr = FindItemInList(cAlphaArgs(4), Surface);
            if ((Rink(Item).SurfacePtr == 0) || (Surface(Rink(Item).SurfacePtr).Class != SurfaceClass::Floor) ||
                (Surface(Rink(Item).SurfacePtr).Class == SurfaceClass::Window) || (Surface(Rink(Item).SurfacePtr).IsRadSurfOrVentSlabOrPool) ||
                (!state.dataConstruction->Construct(Surface(Rink(Item).SurfacePtr).Construction).SourceSinkPresent)) {
                ShowSevereError(state, RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\", Invalid Surface");
                ShowContinueError(state,
                                  cAlphaFieldNames(4) + "=\"" + cAlphaArgs(4) +
                                      "\" either is not defines as a floor or is defined as a window or is a part of ventilated slab or has no "
                                      "source/sink defines in it");

                ErrorsFound = true;
            }

            /*if (Surface(Rink(Item).SurfacePtr).Zone != Rink(Item).ZonePtr) {
                ShowSevereError("Surface referenced in " + cCurrentModuleObject +
                                " not in same zone as Refrigeration System, surface=" + Surface(Rink(Item).SurfacePtr).Name);
                ShowContinueError("Surface in Zone=" + Zone(Surface(Rink(Item).SurfacePtr).Zone).Name + " Direct refrigeration System in " +
                                  cAlphaFieldNames(3) + " = " + cAlphaArgs(3));
                ShowContinueError("Occurs in " + cCurrentModuleObject + " = " + cAlphaArgs(1));
                ErrorsFound = true;
            }*/

            Rink(Item).TubeDiameter = rNumericArgs(1);
            Rink(Item).TubeLength = rNumericArgs(2);

            if (UtilityRoutines::SameString(cAlphaArgs(5), "BOTC")) {
                Rink(Item).ControlStrategy = BrineOutletTempControl;
            } else if (UtilityRoutines::SameString(cAlphaArgs(5), "STC")) {
                Rink(Item).ControlStrategy = SurfaceTempControl;
            }

            Rink(Item).hrstofreeze = rNumericArgs(3); // shouldn't the min and max RefrigMassFlow be based on the plant?
            Rink(Item).deltatemp = rNumericArgs(4);   // shouldn't the min and max RefrigMassFlow be based on the plant?

            Rink(Item).RefrigInNode = cAlphaArgs(6);
            Rink(Item).InNode = GetOnlySingleNode(state,
                                                  cAlphaArgs(6),
                                                  ErrorsFound,
                                                  cCurrentModuleObject,
                                                  cAlphaArgs(1),
                                                  NodeType_Unknown,
                                                  NodeConnectionType_Inlet,
                                                  1,
                                                  ObjectIsNotParent);
            if (Rink(Item).InNode == 0) {
                ShowSevereError(state, "Invalid " + cAlphaFieldNames(6) + '=' + cAlphaArgs(6));
                ShowContinueError(state, "Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            Rink(Item).RefrigOutNode = cAlphaArgs(7);
            Rink(Item).OutNode = GetOnlySingleNode(state,
                                                   cAlphaArgs(7),
                                                   ErrorsFound,
                                                   cCurrentModuleObject,
                                                   cAlphaArgs(1),
                                                   NodeType_Unknown,
                                                   NodeConnectionType_Outlet,
                                                   1,
                                                   ObjectIsNotParent);
            if (Rink(Item).OutNode == 0) {
                ShowSevereError(state, "Invalid " + cAlphaFieldNames(7) + '=' + cAlphaArgs(7));
                ShowContinueError(state, "Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            TestCompSet(state, cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(6), cAlphaArgs(7), "Refrigerant Nodes");

            Rink(Item).RefrigSetptSched = cAlphaArgs(8);
            Rink(Item).RefrigSetptSchedPtr = GetScheduleIndex(state, cAlphaArgs(8));
            if ((Rink(Item).RefrigSetptSchedPtr == 0) && (!lAlphaFieldBlanks(8))) {
                ShowSevereError(state, cAlphaFieldNames(8) + " not found: " + cAlphaArgs(8));
                ShowContinueError(state, "Occurs in " + cCurrentModuleObject + " = " + cAlphaArgs(1));
                ErrorsFound = true;
            }

            Rink(Item).IceSetptSched = cAlphaArgs(9);
            Rink(Item).IceSetptSchedPtr = GetScheduleIndex(state, cAlphaArgs(9));
            if ((Rink(Item).IceSetptSchedPtr == 0) && (!lAlphaFieldBlanks(9))) {
                ShowSevereError(state, cAlphaFieldNames(9) + " not found: " + cAlphaArgs(9));
                ShowContinueError(state, "Occurs in " + cCurrentModuleObject + " = " + cAlphaArgs(1));
                ErrorsFound = true;
            }

            Rink(Item).PeopleHeatGainSchedName = cAlphaArgs(10);
            Rink(Item).PeopleHeatGainSchedPtr = GetScheduleIndex(state, cAlphaArgs(10));
            if ((Rink(Item).PeopleHeatGainSchedPtr == 0) && (!lAlphaFieldBlanks(10))) {
                ShowSevereError(state, cAlphaFieldNames(10) + " not found: " + cAlphaArgs(10));
                ShowContinueError(state, "Occurs in " + cCurrentModuleObject + " = " + cAlphaArgs(1));
                ErrorsFound = true;
            }

            Rink(Item).PeopleSchedName = cAlphaArgs(11);
            Rink(Item).PeopleSchedPtr = GetScheduleIndex(state, cAlphaArgs(11));
            if ((Rink(Item).PeopleSchedPtr == 0) && (!lAlphaFieldBlanks(11))) {
                ShowSevereError(state, cAlphaFieldNames(11) + " not found: " + cAlphaArgs(11));
                ShowContinueError(state, "Occurs in " + cCurrentModuleObject + " = " + cAlphaArgs(1));
                ErrorsFound = true;
            }

            Rink(Item).MaxNumOfPeople = rNumericArgs(5);
            if (Rink(Item).MaxNumOfPeople < 0) {
                ShowWarningError(
                    state, RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) + " was entered with negative people.  This is not allowed.");
                ShowContinueError(state, "The number of people has been reset to zero.");
                Rink(Item).MaxNumOfPeople = 0.0;
            }

            Rink(Item).LengthRink = rNumericArgs(6);
            if (Rink(Item).LengthRink <= 0.0) {
                ShowWarningError(state,
                                 RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) +
                                     " was entered with zero or negetive rink length. This is not allowed");
                ShowContinueError(state, "The rink length has been reset to 60.");
                Rink(Item).LengthRink = 60.0;
            }

            Rink(Item).WidthRink = rNumericArgs(7);
            if (Rink(Item).WidthRink <= 0.0) {
                ShowWarningError(state,
                                 RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) +
                                     " was entered with zero or negetive rink width. This is not allowed");
                ShowContinueError(state, "The rink width has been reset to 30.");
                Rink(Item).WidthRink = 30.0;
            }

            // Rink(Item).DepthRink = rNumericArgs(8); // this is not used!!
            // if (Rink(Item).DepthRink <= 0.0) {
            //    ShowWarningError(RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) +
            //                     " was entered with zero or negetive rink depth. This is not allowed");
            //    ShowContinueError("The rink depth has been reset to 1.");
            //    Rink(Item).DepthRink = 1.0;
            //}

            Rink(Item).WaterTemp = rNumericArgs(8);
            if (Rink(Item).WaterTemp <= 0.0) {
                ShowWarningError(state,
                                 RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) +
                                     " was entered with zero or negetive rink depth. This is not allowed");
                ShowContinueError(state, "The rink water has been reset to 22");
                Rink(Item).WaterTemp = 22.0;
            }

            Rink(Item).IceThickness = rNumericArgs(9);
            if (Rink(Item).IceThickness <= 0.0) {
                ShowWarningError(state,
                                 RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) +
                                     " was entered with zero or negetive ice thickness. This is not allowed");
                ShowContinueError(state, "The rink ice thickness has been reset to 0.1.");
                Rink(Item).IceThickness = 0.0254;
            }

            Rink(Item).FloodWaterTemp = rNumericArgs(10);
            if (Rink(Item).FloodWaterTemp <= 0.0) {
                ShowWarningError(state,
                                 RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) +
                                     " was entered with zero or negetive flood water temperature. This is not allowed");
                ShowContinueError(state, "The floor water temperature has been reset to 15.0 C.");
                Rink(Item).FloodWaterTemp = 15.0;
            }

            Rink(Item).IceSetPointTemp = rNumericArgs(11);
            if (Rink(Item).IceSetPointTemp >= 0.0) {
                ShowWarningError(state,
                                 RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) +
                                     " was entered with zero or positive ice rink setpoint temperature. This is not allowed");
                ShowContinueError(state, "The ice rink setpoint temperature has been reset to -3 C.");
                Rink(Item).IceSetPointTemp = -3;
            }
        }

        if (ErrorsFound) {
            ShowFatalError(state, "Errors found in input.");
        }
    }

    void GetResurfacer(EnergyPlusData &state)
    {
        // Using/Aliasing
        using namespace DataIPShortCuts; // Data for field names, blank numerics
        using ScheduleManager::GetScheduleIndex;

        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("GetIndoorIceRink: ");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        static bool ErrorsFound(false); // Set to true if errors in input,
        int IOStatus;                   // Used in GetObjectItem
        int Item;                       // Item to be "gotten"
        int NumAlphas;                  // Number of Alphas for each GetObjectItem call
        int NumNumbers;                 // Number of Numbers for each GetObjectItem call

        NumOfResurfacers = inputProcessor->getNumObjectsFound(state, cResurfacer);

        Rink.allocate(NumOfResurfacers);

        for (Item = 1; Item <= NumOfResurfacers; ++Item) {
            cCurrentModuleObject = "IceRink:Resurfacer";

            inputProcessor->getObjectItem(state,
                                          cCurrentModuleObject,
                                          Item,
                                          cAlphaArgs,
                                          NumAlphas,
                                          rNumericArgs,
                                          NumNumbers,
                                          IOStatus,
                                          _,
                                          _,
                                          cAlphaFieldNames,
                                          cNumericFieldNames);

            Rink(Item).NameR = cAlphaArgs(1);
            Rink(Item).ResurfacingSchedName = cAlphaArgs(2);
            if (lAlphaFieldBlanks(2)) {
                Rink(Item).ResurfacingSchedPtr = 0;
            } else {
                Rink(Item).ResurfacingSchedPtr = GetScheduleIndex(state, cAlphaArgs(2));
            }

            Rink(Item).NoOfResurfEvents = rNumericArgs(1);
            for (auto &R : Rink) {
                R.NoOfResurfEvents = Rink(Item).NoOfResurfEvents;
            }

            if (lNumericFieldBlanks(1)) {
                Rink(Item).NoOfResurfEvents = 1;
            }
            if (Rink(Item).NoOfResurfEvents < 0) {
                ShowSevereError(state, cNumericFieldNames(1) + "not found for " + cAlphaArgs(1));
                ErrorsFound = true;
            }

            Rink(Item).ResurfacingWaterTemp = rNumericArgs(2);
            if (Rink(Item).ResurfacingWaterTemp <= 0.0) {
                ShowWarningError(state,
                                 RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) +
                                     " was entered with zero or negetive resurfacing water temperature. This is not allowed");
                ShowContinueError(state, "The resurfacing water temperature has been reset to 15.0.");
                Rink(Item).ResurfacingWaterTemp = 15.0;
            }

            Rink(Item).InitWaterTemp = rNumericArgs(3);
            Rink(Item).TankCapacity = rNumericArgs(4);
            if (Rink(Item).TankCapacity <= 0) {
                ShowWarningError(state,
                                 RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) +
                                     " was entered with zero or negetive resurfacer tank capacity. This is not allowed");
                ShowContinueError(state, "The resurfacer tank capacity has been reset to 3.0.");
                Rink(Item).ResurfacingWaterTemp = 3.0;
            }
        }

        if (ErrorsFound) {
            ShowFatalError(state, "Errors found in input.");
        }
    }

    void IceRinkData::initialize(EnergyPlusData &state)
    {
        // Using/Aliasing

        using DataLoopNode::Node;
        using DataPlant::PlantLoop;
        using FluidProperties::GetSpecificHeatGlycol;

        // SUBROUTINE PARAMETER DEFINITIONS:
        Real64 const DesignVelocity(0.5); // Hypothetical design max pipe velocity [m/s]
        static std::string const RoutineName("initialize");
        static bool MyEnvrnFlagGeneral(true);

        // INTERFACE BLOCK SPECIFICATIONS
        // na

        // DERIVED TYPE DEFINITIONS
        // na

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        bool errFlag;
        int SurfNum = this->SurfacePtr;

        if ((this->MyFlag) && allocated(DataPlant::PlantLoop)) {
            errFlag = false;
            this->setupOutputVariables(state);
            PlantUtilities::ScanPlantLoopsForObject(
                state, this->Name, DataPlant::TypeOf_IceRink, this->LoopNum, this->LoopSide, this->BranchNum, this->CompNum, errFlag, _, _, _, _, _);
            if (errFlag) {
                ShowFatalError(state, "Initialize: Program terminated due to previous condition(s).");
            }
            Real64 rho = FluidProperties::GetDensityGlycol(state,
                                                           DataPlant::PlantLoop(this->LoopNum).FluidName,
                                                           DataPrecisionGlobals::constant_zero,
                                                           DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                                           RoutineName);
            // Real64 Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
            //                                                   DataPrecisionGlobals::constant_zero,
            //                                                   DataPlant::PlantLoop(this->LoopNum).FluidIndex,
            //                                                   RoutineName);

            this->DesignMassFlowRate = DataGlobalConstants::Pi / 4.0 * pow_2(this->TubeDiameter) * DesignVelocity * rho *
                                       this->TubeLength; // Tube Length or  piping circuits
            // this->DesignCapacity = this->DesignMassFlowRate * Cp * 10.0;                                     // assume 10C delta T?
            // this->coeffs.RefrigMassFlow = 0.01;
            // this->resurf.IceTemperature = -2;
            // for initial test
            // this->coeffs.PastIceTemp = -4;          // for initial test

            PlantUtilities::InitComponentNodes(0.0,
                                               this->DesignMassFlowRate,
                                               this->InNode,
                                               this->OutNode,
                                               this->LoopNum,
                                               this->LoopSide,
                                               this->BranchNum,
                                               this->CompNum); // initialization

            PlantUtilities::RegisterPlantCompDesignFlow(this->InNode, this->DesignMassFlowRate / rho); // this registers volume flowrate

            // Request the mass flow rate from the plant component flow utility routine
            PlantUtilities::SetComponentFlowRate(
                state, this->PastRefrigMassFlow, InNode, OutNode, this->LoopNum, this->LoopSide, this->BranchNum, this->CompNum);

            this->MyFlag = false;
        }

        if (FirstTimeInit) {

            QRadSysSrcAvg.dimension(TotSurfaces, 0.0);
            LastQRadSysSrc.dimension(TotSurfaces, 0.0);
            LastSysTimeElapsed.dimension(TotSurfaces, 0.0);
            LastTimeStepSys.dimension(TotSurfaces, 0.0);
            this->PastRefrigMassFlow = 0.01;
            this->RefrigTempIn = -10; // to be changed to be automatic
            this->CpRefrig = GetSpecificHeatGlycol(
                state, PlantLoop(this->LoopNum).FluidName, this->RefrigTempIn, PlantLoop(this->LoopNum).FluidIndex, RoutineName);
            this->Qsrcmax = -(IceRinkFreezing(state));
            this->maxmdot = abs(this->Qsrcmax) / (this->CpRefrig * this->deltatemp);
            // this->Qsource = 1;

            FirstTimeInit = false;
        }

        if (this->MyEnvrnFlag && state.dataGlobal->BeginEnvrnFlag) {

            // this->QSrc = 0.0;
            QRadSysSrcAvg = 0.0;
            LastQRadSysSrc = 0.0;
            LastSysTimeElapsed = 0.0;
            LastTimeStepSys = 0.0;
            this->FreezingLoad = 0.0;
            // this->Qsource = 0;
            this->Qsource2 = 0;
            this->Qsetpoint = 0;
            MyEnvrnFlagGeneral = false;
        }

        if (state.dataGlobal->BeginTimeStepFlag) {
            QRadSysSrcAvg(SurfNum) = 0.0;
        }

        // this->CoolEnergy = 0.0;
        // this->CoolPower = 0.0;

        if (!(state.dataGlobal->BeginEnvrnFlag)) this->MyEnvrnFlag = true;

        this->TotalSurfaceArea = this->LengthRink * this->WidthRink;
        InletTemp = DataLoopNode::Node(this->InNode).Temp;
        OutletTemp = DataLoopNode::Node(this->OutNode).Temp;
        FlowRate = DataLoopNode::Node(this->InNode).MassFlowRate;
    }

    void IceRinkData::setupOutputVariables(EnergyPlusData &state)
    {

        // Set up output variables CurrentModuleObject='IceRink:Indoor'
        if (this->RinkType_Num == DataPlant::TypeOf_IceRink) {
            SetupOutputVariable(
                state, "Rink Refrigerant Inlet Temperature", OutputProcessor::Unit::C, this->RefrigInletTemp, "System", "Average", this->Name);
            SetupOutputVariable(state, "Refrig out temp", OutputProcessor::Unit::C, this->TRefigOutCheck, "System", "Average", this->Name);
            SetupOutputVariable(state, "CpRefrig", OutputProcessor::Unit::None, this->CpRefrig, "System", "Average", this->Name);
            SetupOutputVariable(
                state, "Rink Refrigerant Mass Flow Rate", OutputProcessor::Unit::kg_s, this->RefrigMassFlow, "System", "Average", this->Name);
            SetupOutputVariable(state, "Max mass flowrate", OutputProcessor::Unit::kg_s, this->maxmdot, "System", "Average", this->Name);
            SetupOutputVariable(state, "Rink Temperature", OutputProcessor::Unit::C, this->IceTemperature, "System", "Average", this->Name);
            SetupOutputVariable(state, "previous TH22", OutputProcessor::Unit::C, this->Tsurfin1, "System", "Average", this->Name);
            SetupOutputVariable(state, "current TH22", OutputProcessor::Unit::C, this->Tsurfin2, "System", "Average", this->Name);
            SetupOutputVariable(state, "Tsource", OutputProcessor::Unit::C, this->Tsrc, "System", "Average", this->Name);
            SetupOutputVariable(state, "Qsource max", OutputProcessor::Unit::W, this->Qsrcmax, "System", "Average", this->Name);
            SetupOutputVariable(state, "Qsetpoint", OutputProcessor::Unit::W, this->Qsetpoint, "System", "Average", this->Name);
            // SetupOutputVariable("Rink Qsource",
            //                    OutputProcessor::Unit::J,
            //                    this->resurf.Qsource,
            //                    "System",
            //                    "average",
            //                    this->Name,
            //                    _,
            //                    "Electric",
            //                    "REFRIGERATION",
            //                    _,
            //                    "System");
            // SetupOutputVariable("Rink Qsource", OutputProcessor::Unit::W, this->Qsource, "System", "Average", this->Name);
            SetupOutputVariable(state, "Qsrc after HB update", OutputProcessor::Unit::W, this->Qsource2, "System", "Average", this->Name);
            SetupOutputVariable(state, "Qsrc", OutputProcessor::Unit::W, this->Q, "System", "Average", this->Name);

            SetupOutputVariable(state, "Ca", OutputProcessor::Unit::None, this->coeffs.Ca, "System", "Average", this->Name);
            SetupOutputVariable(state, "Cb", OutputProcessor::Unit::None, this->coeffs.Cb, "System", "Average", this->Name);
            SetupOutputVariable(state, "Cc", OutputProcessor::Unit::None, this->coeffs.Cc, "System", "Average", this->Name);
            SetupOutputVariable(state, "Cd", OutputProcessor::Unit::None, this->coeffs.Cd, "System", "Average", this->Name);
            SetupOutputVariable(state, "Ce", OutputProcessor::Unit::None, this->coeffs.Ce, "System", "Average", this->Name);
            SetupOutputVariable(state, "Cf", OutputProcessor::Unit::None, this->coeffs.Cf, "System", "Average", this->Name);
            SetupOutputVariable(state, "Cg", OutputProcessor::Unit::None, this->coeffs.Cg, "System", "Average", this->Name);
            SetupOutputVariable(state, "Ch", OutputProcessor::Unit::None, this->coeffs.Ch, "System", "Average", this->Name);
            SetupOutputVariable(state, "Ci", OutputProcessor::Unit::None, this->coeffs.Ci, "System", "Average", this->Name);
            SetupOutputVariable(state, "Cj", OutputProcessor::Unit::None, this->coeffs.Cj, "System", "Average", this->Name);
            SetupOutputVariable(state, "Ck", OutputProcessor::Unit::None, this->coeffs.Ck, "System", "Average", this->Name);
            SetupOutputVariable(state, "Cl", OutputProcessor::Unit::None, this->coeffs.Cl, "System", "Average", this->Name);

            SetupOutputVariable(state, "Rho_w", OutputProcessor::Unit::kg_m3, this->RhoWater, "System", "Average", this->Name);
            SetupOutputVariable(state, "Cp_w", OutputProcessor::Unit::J_kg, this->CpWater, "System", "Average", this->Name);
            SetupOutputVariable(state, "Effectiveness", OutputProcessor::Unit::None, this->Effectiveness, "System", "Average", this->Name);

            SetupOutputVariable(state, "Rink Refrigeration Rate", OutputProcessor::Unit::W, this->CoolPower, "System", "Sum", this->Name);
            SetupOutputVariable(state,
                                "Rink Refrigeration Energy",
                                OutputProcessor::Unit::J,
                                this->CoolEnergy,
                                "System",
                                "Sum",
                                this->Name,
                                _,
                                "Electric",
                                "REFRIGERATION",
                                _,
                                "System");
        }
        // for (auto &R : Resurfacer) {
        //    SetupOutputVariable("Resurfacer heat rate", OutputProcessor::Unit::J, R.QResurfacing, "System", "Average", this->Name);
        //    SetupOutputVariable("Resurfacer hot water",
        //                        OutputProcessor::Unit::J,
        //                        R.EHeatingWater,
        //                        "System",
        //                        "Sum",
        //                        this->Name,
        //                        _,
        //                        "Gas",
        //                        "WaterSystems",
        //                        _,
        //                        "System");
        //}
    }

    Real64 IceRinkData::PeopleHG(EnergyPlusData &state)
    {
        static std::string const RoutineName("PeopleHG");

        this->PeopleHeatGain = ScheduleManager::GetCurrentScheduleValue(state, this->PeopleHeatGainSchedPtr);
        if (this->PeopleHeatGain < 0.0) {
            ShowWarningError(state,
                             RoutineName + ": Ice Rink =\"" + this->Name + " People Heat Gain Schedule =\"" + this->PeopleHeatGainSchedName +
                                 " has a negative value.  This is not allowed.");
            ShowContinueError(state, "The heat gain per person has been reset to zero.");
            this->PeopleHeatGain = 0.0;
        }

        this->NumOfPeople = ScheduleManager::GetCurrentScheduleValue(state, this->PeopleSchedPtr);
        if (this->NumOfPeople < 0.0) {
            ShowWarningError(state,
                             RoutineName + ": Ice Rink =\"" + this->Name + " People Schedule =\"" + this->PeopleSchedName +
                                 " has a negative value.  This is not allowed.");
            ShowContinueError(state, "The number of people has been reset maximum people capacity.");
            this->NumOfPeople = this->MaxNumOfPeople;
        }

        this->TotalPeopleHG = this->PeopleHeatGain * this->NumOfPeople;

        return (this->TotalPeopleHG);
    }

    Real64 IceRinkData::IceRinkFreezing(EnergyPlusData &state)
    {
        Real64 QFusion(333550.00);
        Real64 CpIce(2108.00);
        static std::string const RoutineName("IceRinkFreezing");
        using DataHVACGlobals::TimeStepSys;
        using ScheduleManager::GetCurrentScheduleValue;

        Real64 FreezingLoad;
        Real64 RhoWater = FluidProperties::GetDensityGlycol(state, "WATER", this->WaterTemp, this->WaterIndex, RoutineName);
        Real64 CpWater = FluidProperties::GetSpecificHeatGlycol(state, "WATER", this->WaterTemp, this->WaterIndex, RoutineName);
        Real64 Volume = this->LengthRink * this->WidthRink * this->IceThickness;

        Real64 QFreezing = RhoWater * Volume * // To find the maximum flowrate (design calculation/worst case scenario) - replace volume
                                               // with volume of ice rink arena & deltaT -> room temperature and ice setpoint
                           ((CpWater * this->WaterTemp) + (QFusion) - (CpIce * this->IceSetPointTemp)) / (hrstofreeze * this->SecInHour);
        FreezingLoad = QFreezing;

        return FreezingLoad;
    }

    Real64 IceRinkData::RinkResurfacer(EnergyPlusData &state, Real64 &ResurfacingLoad)
    {

        static std::string const RoutineName("IceRinkResurfacer");
        using DataHeatBalSurface::TH;
        Real64 QFusion(333550.00);
        Real64 CpIce(2108.00);
        Real64 RhoWater = FluidProperties::GetDensityGlycol(state, "WATER", this->ResurfacingWaterTemp, this->WaterIndex, RoutineName);
        Real64 CpWater = FluidProperties::GetSpecificHeatGlycol(state, "WATER", this->ResurfacingWaterTemp, this->WaterIndex, RoutineName);
        Real64 SurfTemp;

        int SurfNum = this->SurfacePtr;
        SurfTemp = TH(1, 2, SurfNum);

        QResurfacing = this->NoOfResurfEvents * 0.001 * RhoWater * this->TankCapacity *
                       ((CpWater * this->ResurfacingWaterTemp) + (QFusion) - (CpIce * SurfTemp));
        EHeatingWater = this->NoOfResurfEvents * 0.001 * this->TankCapacity * RhoWater * CpWater * (this->ResurfacingWaterTemp - this->InitWaterTemp);
        ResurfacingLoad = QResurfacing;
        return (ResurfacingLoad);
    }

    Real64 IceRinkData::calcEffectiveness(EnergyPlusData &state,
                                          Real64 const Temperature,    // Temperature of refrigerant entering the floor radiant system, in C))
                                          Real64 const RefrigMassFlow) // Mass flow rate of refrigerant in the floor radiant system, in kg/s
    {
        // Using/Aliasing

        using DataGlobalConstants::Pi;
        using DataPlant::PlantLoop;

        // Return value
        Real64 CalcEffectiveness; // Function return variable

        // FUNCTION PARAMETER DEFINITIONS:
        Real64 const MaxLaminarRe(2300.0); // Maximum Reynolds number for laminar flow
        static std::string const RoutineName("IceRink:calcEffectiveness");
        Real64 const MaxExpPower(50.0);

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:

        Real64 NuseltNum; // Nuselt number (dimensionless)
        // printf("Temp is %f", Temperature);
        Real64 SpecificHeat = FluidProperties::GetSpecificHeatGlycol(
            state, PlantLoop(this->LoopNum).FluidName, Temperature, PlantLoop(this->LoopNum).FluidIndex, RoutineName);
        Real64 Conductivity = FluidProperties::GetConductivityGlycol(
            state, PlantLoop(this->LoopNum).FluidName, Temperature, PlantLoop(this->LoopNum).FluidIndex, RoutineName);
        Real64 Viscosity = FluidProperties::GetViscosityGlycol(
            state, PlantLoop(this->LoopNum).FluidName, Temperature, PlantLoop(this->LoopNum).FluidIndex, RoutineName);
        Real64 Density = FluidProperties::GetDensityGlycol(
            state, PlantLoop(this->LoopNum).FluidName, Temperature, PlantLoop(this->LoopNum).FluidIndex, RoutineName);

        // Calculate the Reynold's number from RE=(4*Mdot)/(Pi*Mu*Diameter)
        Real64 ReynoldsNum =
            4.0 * RefrigMassFlow / (Pi * Viscosity * this->TubeDiameter); // why circuits number were used - it doesn't match the equation??

        Real64 PrantlNum = Viscosity * SpecificHeat / Conductivity;

        // Calculate the Nusselt number based on what flow regime one is in. h = (k)(Nu)/D
        if (ReynoldsNum >= MaxLaminarRe) { // Turbulent flow --> use Dittus-Boelter equation
            NuseltNum = 0.023 * std::pow(ReynoldsNum, 0.8) * std::pow(PrantlNum, 0.3);
        } else { // Laminar flow --> use constant surface temperature relation
            NuseltNum = 3.66;
        }

        Real64 NTU = Pi * Conductivity * NuseltNum * this->TubeLength / (RefrigMassFlow * SpecificHeat);

        if (NTU > MaxExpPower) {
            CalcEffectiveness = 1.0;
        } else {
            CalcEffectiveness = 1.0 - std::exp(-NTU);
        }

        return CalcEffectiveness;
    }

    void IceRinkData::calculateIceRink(EnergyPlusData &state, Real64 &LoadMet, bool is_test) // should this begin in the second timestep?
    {

        // Using/Aliasing

        static bool MyOneTimeFlag(true);
        bool MyDayFlag = true;
        using DataGlobalConstants::Pi;
        using DataGlobalConstants::SecInHour;
        using DataHeatBalFanSys::CTFTsrcConstPart;
        using DataHeatBalFanSys::QRadSysSource;
        using DataHeatBalFanSys::RadSysTiHBConstCoef;
        using DataHeatBalFanSys::RadSysTiHBQsrcCoef;
        using DataHeatBalFanSys::RadSysTiHBToutCoef;
        using DataHeatBalFanSys::RadSysToHBConstCoef;
        using DataHeatBalFanSys::RadSysToHBQsrcCoef;
        using DataHeatBalFanSys::RadSysToHBTinCoef;
        using DataHeatBalSurface::TempSource;
        using DataHeatBalSurface::TempSurfIn;
        using DataHeatBalSurface::TH;
        using DataHVACGlobals::TimeStepSys;
        using DataLoopNode::Node;
        using DataPlant::PlantLoop;
        using DataSurfaces::HeatTransferModel_CTF;
        using DataSurfaces::Surface;
        using FluidProperties::GetSpecificHeatGlycol;
        using PlantUtilities::SetComponentFlowRate;
        using ScheduleManager::GetCurrentScheduleValue;

        static std::string const RoutineName("IceRink:calculateDirectIceRink");
        Real64 QRadSysSourceMax;
        Real64 ResurfacingLoad;
        Real64 Freezing;
        this->operation = GetCurrentScheduleValue(state, this->IceSetptSchedPtr);

        Real64 TSource;
        Real64 circuits = 40; // assuming rink length is 100 and there is a refrigeration pipe every 2.5m.
        Real64 PipeArea = (Pi * this->TubeLength * this->TubeDiameter) * circuits; // pipe surface area

        int ZoneNum = this->ZonePtr;
        int ControlStrategy = this->ControlStrategy;
        int RefrigNodeIn = this->InNode;
        int SurfNum = this->SurfacePtr;

        if (RefrigNodeIn == 0) {
            ShowSevereError(state, "Illegal inlet node for the refrigerant in the direct system");
            ShowFatalError(state, "Preceding condition causes termination");
        }

        if (ControlStrategy == BrineOutletTempControl) {

            // Setting the set point temperature of refrigerant outlet
            if (this->RefrigSetptSchedPtr > 0) {
                this->RefrigSetptTemp = GetCurrentScheduleValue(state, this->RefrigSetptSchedPtr);
            } else {
                ShowSevereError(state, "Illegal pointer to brine outlet control strategy");
                ShowFatalError(state, "Preceding condition causes termination");
            }
        } else if (ControlStrategy == SurfaceTempControl) {

            // Setting the set point temperature of ice surface
            if (this->IceSetptSchedPtr > 0) {
                this->IceSetptTemp = GetCurrentScheduleValue(state, this->IceSetptSchedPtr);
            } else {
                ShowSevereError(state, "Illegal pointer to surface temperature control strategy");
                ShowFatalError(state, "Preceding condition causes termination");
            }
        } else {
            ShowSevereError(state, "Illegal input for control strategy");
            ShowFatalError(state, "Preceding condition causes termination");
        }

        // if (this->RefrigMassFlow > 0) SetComponentFlowRate(this->RefrigMassFlow, this->InNode, this->OutNode, this->LoopNum, this->LoopSide,
        // this->BranchNum, this->CompNum);
        // this->coeffs.RefrigMassFlow =
        //    this->coeffs.PastRefrigMassFlow; // this is just for hardwiring massflowrate - the original code:Node(RefrigNodeIn).MassFlowRate
        // this->resurf.IceTemperature = this->coeffs.PastIceTemp;
        this->RefrigTempIn = Node(RefrigNodeIn).Temp;

        // Real64 Ca; // Coefficients to relate the inlet water temperature to the heat source
        // Real64 Cb;
        // Real64 Cc;
        // Real64 Cd;
        // Real64 Ce;
        // Real64 Cf;
        // Real64 Cg;
        // Real64 Ch;
        // Real64 Ci;
        // Real64 Cj;
        // Real64 Ck;
        // Real64 Cl;

        if (!is_test) {
            this->RefrigMassFlow =
                this->PastRefrigMassFlow; // this is just for hardwiring massflowrate - the original code:Node(RefrigNodeIn).MassFlowRate
            // this->resurf.IceTemperature = this->coeffs.PastIceTemp;
            this->RefrigTempIn = Node(RefrigNodeIn).Temp;
            if (!is_test) {
                int ConstrNum = DataSurfaces::Surface(SurfNum).Construction; // Pointer to the construction in the Construct derived type
                this->coeffs.Ca = RadSysTiHBConstCoef(SurfNum);
                this->coeffs.Cb = RadSysTiHBToutCoef(SurfNum);
                this->coeffs.Cc = RadSysTiHBQsrcCoef(SurfNum);

                this->coeffs.Cd = RadSysToHBConstCoef(SurfNum);
                this->coeffs.Ce = RadSysToHBTinCoef(SurfNum);
                this->coeffs.Cf = RadSysToHBQsrcCoef(SurfNum);

                this->coeffs.Cg = CTFTsrcConstPart(SurfNum);
                this->coeffs.Ch = state.dataConstruction->Construct(ConstrNum).CTFTSourceQ(0);
                this->coeffs.Ci = state.dataConstruction->Construct(ConstrNum).CTFTSourceIn(0);
                this->coeffs.Cj = state.dataConstruction->Construct(ConstrNum).CTFTSourceOut(0);
                this->coeffs.Area = DataSurfaces::Surface(SurfNum).Area;
            }

            this->coeffs.Ck = this->coeffs.Cg + ((this->coeffs.Ci * (this->coeffs.Ca + this->coeffs.Cb * this->coeffs.Cd) +
                                                  this->coeffs.Cj * (this->coeffs.Cd + this->coeffs.Ce * this->coeffs.Ca)) /
                                                 (1.0 - this->coeffs.Ce * this->coeffs.Cb));

            /* Ck = Cg + ((Ci * (Ca + Cb * Cd) + Cj * (Cd + Ce * Ca)) / (1.0 - Ce * Cb));*/
            this->coeffs.Cl = this->coeffs.Ch + ((this->coeffs.Ci * (this->coeffs.Cc + this->coeffs.Cb * this->coeffs.Cf) +
                                                  this->coeffs.Cj * (this->coeffs.Cf + this->coeffs.Ce * this->coeffs.Cc)) /
                                                 (1.0 - this->coeffs.Ce * this->coeffs.Cb));
            /* Cl = Ch + ((Ci * (Cc + Cb * Cf) + Cj * (Cf + Ce * Cc)) / (1.0 - Ce * Cb));*/
        }

        // if (Surface(SurfNum).HeatTransferAlgorithm == HeatTransferModel_CTF) {

        //    int ConstrNum = DataSurfaces::Surface(SurfNum).Construction;
        //    Ca = RadSysTiHBConstCoef(SurfNum);
        //    Cb = RadSysTiHBToutCoef(SurfNum);
        //    Cc = RadSysTiHBQsrcCoef(SurfNum);

        //    Cd = RadSysToHBConstCoef(SurfNum);
        //    Ce = RadSysToHBTinCoef(SurfNum);
        //    Cf = RadSysToHBQsrcCoef(SurfNum);

        //    Cg = CTFTsrcConstPart(SurfNum);
        //    Ch = DataHeatBalance::Construct(ConstrNum).CTFTSourceQ(0);
        //    Ci = DataHeatBalance::Construct(ConstrNum).CTFTSourceIn(0);
        //    Cj = DataHeatBalance::Construct(ConstrNum).CTFTSourceOut(0);

        //    Ck = Cg + ((Ci * (Ca + Cb * Cd) + Cj * (Cd + Ce * Ca)) / (1.0 - Ce * Cb));
        //    Cl = Ch + ((Ci * (Cc + Cb * Cf) + Cj * (Cf + Ce * Cc)) / (1.0 - Ce * Cb));
        //}

        this->Tsurfin1 = TH(2, 2, SurfNum);
        this->Tsurfin2 = TH(1, 2, SurfNum);
        this->IceTemperature = TempSurfIn(SurfNum);

        // if (this->resurf.IceTemperature > 0) {
        //    IceRinkFreezing();
        //    this->resurf.IceTemperature = this->IceSetptTemp;
        //    TempSurfIn(SurfNum) = this->resurf.IceTemperature;
        //    Effectiveness = calcEffectiveness(this->coeffs.RefrigTempIn, this->MaxRefrigMassFlow);
        //    CpRefrig = GetSpecificHeatGlycol(
        //        PlantLoop(this->LoopNum).FluidName, this->coeffs.RefrigTempIn, PlantLoop(this->LoopNum).FluidIndex, RoutineName);
        //    QRadSysSource(SurfNum) =
        //        (this->coeffs.RefrigTempIn - Ck) / ((Cl / this->coeffs.Area) + (1 / (Effectiveness * this->MaxRefrigMassFlow * CpRefrig)));
        //    // TSource = Ck + (Cl * QRadSysSource(SurfNum));
        //    this->coeffs.RefrigMassFlow = this->MinRefrigMassFlow;
        //    this->coeffs.PastRefrigMassFlow = this->coeffs.RefrigMassFlow;

        //} else if (this->resurf.IceTemperature <= this->IceSetptTemp) { // this is copied from Low Temp Rad
        //    // No flow or below minimum allowed so there is no heat source/sink
        //    // This is possible with a mismatch between system and plant operation
        //    // or a slight mismatch between zone and system controls.  This is not
        //    // necessarily a "problem" so this exception is necessary in the code. (setting massflow rate to zero and having this if condition in
        //    // the beginning will make the module will keep looping forever)

        //    // this->QSrc = 0.0;
        //    // Real64 ReqMassFlow = 0.0;
        //    // SetComponentFlowRate(ReqMassFlow, this->InNode, this->OutNode, this->LoopNum, this->LoopSide, this->BranchNum, this->CompNum);
        //    this->RefrigMassFlow = 0;
        //    this->coeffs.PastRefrigMassFlow = this->coeffs.RefrigMassFlow;
        //    QRadSysSource(SurfNum) = 0;
        //    // this->resurf.IceTemperature = (this->coeffs.Ca + (this->coeffs.Cb * this->coeffs.Cd) +
        //    //                               QRadSysSource(SurfNum) * (this->coeffs.Cc + (this->coeffs.Cb * this->coeffs.Cf))) /
        //    //                             (1 - (this->coeffs.Cb * this->coeffs.Ce)); // Eqn 4
        //} else {
        //    // Refrigerant mass flow rate is significant
        //    // Determine the heat exchanger "effectiveness"

        this->CpRefrig =
            GetSpecificHeatGlycol(state, PlantLoop(this->LoopNum).FluidName, this->RefrigTempIn, PlantLoop(this->LoopNum).FluidIndex, RoutineName);
        // surface area is used instead of entire pipe area until the circuits value is set up
        /*QRadSysSource(SurfNum) =
            (this->coeffs.RefrigTempIn - Ck) /
            ((Cl / this->coeffs.Area) +
             (1 / (Effectiveness * this->coeffs.PastRefrigMassFlow * CpRefrig))); */// heat flux to the slab - eqn 11 and not eqn 7 in ice rink papre

        // TSource = Ck + (Cl * this->resurf.Qsource); // Eqn 9
        this->Tsrc = TempSource(SurfNum);
        // this->resurf.IceTemperature = (this->coeffs.Ca + (this->coeffs.Cb * this->coeffs.Cd) +
        //                               QRadSysSource(SurfNum) * (this->coeffs.Cc + (this->coeffs.Cb * this->coeffs.Cf))) /
        //                              (1 - (this->coeffs.Cb * this->coeffs.Ce)); // Eqn 4
        //    // Real64 IceTemp;
        // IceTemp = this->resurf.IceTemperature;

        // this->resurf.Qsrcmax = this->coeffs.PastRefrigMassFlow * CpRefrig * (this->coeffs.RefrigTempIn - this->resurf.Tsrc); // Eqn 6

        // this->resurf.Qsrcmax = (this->coeffs.RefrigTempIn - Ck) / ((Cl / PipeArea) + (1 / (Effectiveness * this->MaxRefrigMassFlow * CpRefrig)));

        // q/cp_delta_T

        //    if (ControlStrategy == BrineOutletTempControl) {
        //        // Finding the required mass flow rate so that the outlet temperature
        //        // becomes equal to the user defined refrigerant outlet temperature
        //        // for the given refrigerant inlet temperature.

        //        Real64 ReqMassFlow =
        //            (((Ck - this->coeffs.RefrigTempIn) / (this->RefrigSetptTemp - this->coeffs.RefrigTempIn)) - (1 / Effectiveness)) *
        //            (DataSurfaces::Surface(SurfNum).Area / (CpRefrig * Cl));

        //        if (ReqMassFlow >= this->MaxRefrigMassFlow) ReqMassFlow = MaxRefrigMassFlow;

        //        Real64 TRefigOutCheck = this->coeffs.RefrigTempIn - (QRadSysSource(SurfNum)) / this->coeffs.RefrigMassFlow * CpRefrig;

        //        if (TRefigOutCheck <= (this->RefrigSetptTemp)) {
        //            this->coeffs.PastRefrigMassFlow = this->MinRefrigMassFlow;
        //            SetComponentFlowRate(
        //                this->coeffs.RefrigMassFlow, this->InNode, this->OutNode, this->LoopNum, this->LoopSide, this->BranchNum, this->CompNum);
        //        } else {
        //            if (ReqMassFlow >= this->MaxRefrigMassFlow) {
        //                this->coeffs.PastRefrigMassFlow = this->MaxRefrigMassFlow;
        //                // SetComponentFlowRate(
        //                //     this->coeffs.RefrigMassFlow, this->InNode, this->OutNode, this->LoopNum, this->LoopSide, this->BranchNum,
        //                //     this->CompNum);
        //            } else {
        //                this->coeffs.PastRefrigMassFlow = ReqMassFlow;
        //                // SetComponentFlowRate(
        //                //    this->coeffs.RefrigMassFlow, this->InNode, this->OutNode, this->LoopNum, this->LoopSide, this->BranchNum,
        //                //    this->CompNum);
        //            }
        //        }
        //   } else if (ControlStrategy == SurfaceTempControl) {
        // Finding heat thant needs to be extraced so that the
        // ice surface reaches the set point temperature.

        // this->resurf.Qsetpoint =
        //    ((((1 - (this->coeffs.Cb * this->coeffs.Ce)) * this->IceSetptTemp) - this->coeffs.Ca - (this->coeffs.Cb * this->coeffs.Cd)) /
        //     (this->coeffs.Cc + (this->coeffs.Cb * this->coeffs.Cf))); // this is for q/ft^2?
        Real64 Volume = this->LengthRink * this->WidthRink * this->IceThickness;
        this->RhoWater = FluidProperties::GetDensityGlycol(state, "WATER", this->IceTemperature, this->WaterIndex, RoutineName);
        this->CpWater = FluidProperties::GetSpecificHeatGlycol(state, "WATER", this->IceTemperature, this->WaterIndex, RoutineName);
        Real64 CpIce(2040.00);
        Real64 RhoIce(917);
        Real64 QFusion(333550.00);

        /*this->resurf.ReqMassFlow = abs(this->resurf.Qsetpoint / ((Effectiveness * this->resurf.CpRefrig) *
                           (this->coeffs.RefrigTempIn -
                            this->resurf.Tsrc))); */// if qsetpoint is negative then this means refrigeration therefore mreq must be absolute
        // if (ReqMassFlow >= this->MaxRefrigMassFlow) ReqMassFlow = MaxRefrigMassFlow;

        if (this->operation == 1) {

            if (this->Tsurfin2 <= this->IceSetPointTemp) { // in the paper, the IceTemp should be of the previous
                                                           // timestep. Here it is calculated in the same iteration.
                QRadSysSource(SurfNum) = 0.0;
                this->Q = 0;
                this->RefrigMassFlow = 0.0;
                this->ReqMassFlow = 0;
                this->PastRefrigMassFlow = this->RefrigMassFlow;

                // SetComponentFlowRate(
                // this->coeffs.RefrigMassFlow, this->InNode, this->OutNode, this->LoopNum, this->LoopSide, this->BranchNum, this->CompNum);
            } else {

                // if (this->resurf.Qsrcmax <= this->resurf.Qsetpoint) {
                if (this->Tsurfin2 >= 0) {
                    // this->coeffs.RefrigMassFlow = this->MaxRefrigMassFlow; // shouldn't the min and max RefrigMassFlow be based on the plant?

                    this->Qsetpoint = (this->RhoWater * Volume * ((this->CpWater * this->Tsurfin2) + (QFusion) - (CpIce * this->IceSetPointTemp))) /
                                      (this->Increments * this->SecInHour);
                    // if (this->resurf.Qsource > absQmax) {
                    //    this->resurf.Qsource = this->resurf.Qsrcmax;
                    //}

                    this->ReqMassFlow = abs(this->Qsetpoint / (this->CpRefrig * (this->Tsurfin2 - this->RefrigTempIn)));

                    if (this->ReqMassFlow > this->maxmdot) {
                        this->ReqMassFlow = this->maxmdot;
                    }

                    this->RefrigMassFlow = this->ReqMassFlow;
                    this->PastRefrigMassFlow = this->RefrigMassFlow;
                    this->Effectiveness = calcEffectiveness(state, this->RefrigTempIn, this->RefrigMassFlow);

                    this->Q = -(this->Effectiveness * this->Qsetpoint);

                    if (this->Q < (this->Effectiveness * this->Qsrcmax)) {
                        this->Q = this->Effectiveness * this->Qsrcmax;
                    }
                    QRadSysSource(SurfNum) = (this->Q);

                    // 6 number of hours to refreeze
                    //(this->coeffs.RefrigTempIn - Ck) / ((Cl / PipeArea) + (1 / (Effectiveness * this->coeffs.RefrigMassFlow * CpRefrig)));
                    // QRadSysSource(SurfNum) = (this->resurf.Qsrcmax); //timestep * hours to freeze (hours to freeze suggested by Dr. Krarti in an
                    // email)

                    // SetComponentFlowRate(
                    // this->coeffs.RefrigMassFlow, this->InNode, this->OutNode, this->LoopNum, this->LoopSide, this->BranchNum, this->CompNum);
                } else {

                    this->Qsetpoint = (RhoIce * Volume * (CpIce * (this->Tsurfin2 - this->IceSetPointTemp))) / (this->Increments * this->SecInHour);

                    // if (this->resurf.Qsetpoint > absQmax) {
                    //    this->resurf.Qsetpoint = this->resurf.Qsrcmax;
                    //}

                    this->ReqMassFlow = abs(this->Qsetpoint / (this->CpRefrig * (this->Tsurfin2 - this->RefrigTempIn)));

                    if (this->ReqMassFlow > this->maxmdot) {
                        this->ReqMassFlow = this->maxmdot;
                    }
                    this->RefrigMassFlow = this->ReqMassFlow;
                    this->PastRefrigMassFlow = this->RefrigMassFlow;
                    this->Effectiveness = calcEffectiveness(state, this->RefrigTempIn, this->RefrigMassFlow);

                    this->Q = -(this->Effectiveness * this->Qsetpoint);

                    if (this->Q < (this->Effectiveness * this->Qsrcmax)) {
                        this->Q = this->Effectiveness * this->Qsrcmax;
                    }

                    QRadSysSource(SurfNum) = (this->Q);

                    // SetComponentFlowRate(
                    //  this->coeffs.RefrigMassFlow, this->InNode, this->OutNode, this->LoopNum, this->LoopSide, this->BranchNum,
                    //  this->CompNum);
                }
            }
        } else {
            QRadSysSource(SurfNum) = 0.0;
            this->Q = 0;
            this->RefrigMassFlow = 0.0;
            this->ReqMassFlow = 0;
            this->PastRefrigMassFlow = this->RefrigMassFlow;
        }

        if (this->RefrigMassFlow > 0)
            this->TRefigOutCheck = this->RefrigTempIn - ((QRadSysSource(SurfNum)) / (this->RefrigMassFlow * this->CpRefrig));

        // Is this the correct location of the following commands.
        HeatBalanceSurfaceManager::CalcHeatBalanceOutsideSurf(
            state, ZoneNum); // This subroutine performs a heat balance on the outside face of each surface in the building
        HeatBalanceSurfaceManager::CalcHeatBalanceInsideSurf(state, ZoneNum); // ?
        this->Qsource2 = QRadSysSource(SurfNum);

        // LoadMet = Freezing + ResurfaceLoad + HeatFlux;
        LoadMet = QRadSysSource(SurfNum);

        this->LoadMet = LoadMet;

        // printf("Ca %f Cb %f Cc %f Cd %f Ce %f Cf %f Cg %f Ch %f Ci %f Cj %f Ck %f \n", Ca, Cb, Cc, Cd, Ce, Cf, Cg, Ch, Ci, Cj, Ck);
    }

    void IceRinkData::update()
    {
        // Using/Aliasing
        //using DataGlobals::TimeStepZone;
        using DataHVACGlobals::SysTimeElapsed;
        using DataHVACGlobals::TimeStepSys;
        using DataLoopNode::Node;
        using DataPlant::PlantLoop;
        using FluidProperties::GetSpecificHeatGlycol;
        using PlantUtilities::SafeCopyPlantNode;

        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("Update");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        Real64 CpFluid; // Specific heat of working fluid
        int LoopNum;
        int LoopSideNum;
        int SurfNum = this->SurfacePtr;

        // this->QSrc = HeatFlux;

        LoopNum = this->LoopNum;
        LoopSideNum = this->LoopSide;

        // if (PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock > 0) {
        //    if (LastSysTimeElapsed(SurfNum) == SysTimeElapsed) {
        //        // Still iterating or reducing system time step, so subtract old values which were
        //        // not valid
        //        QRadSysSrcAvg(SurfNum) -= LastQRadSysSrc(SurfNum) * LastTimeStepSys(SurfNum) / TimeStepZone;
        //    }

        //    // Update the running average and the "last" values with the current values of the appropriate variables
        //    QRadSysSrcAvg(SurfNum) += QRadSysSource(SurfNum) * TimeStepSys / TimeStepZone;

        //    LastQRadSysSrc(SurfNum) = QRadSysSource(SurfNum);
        //    LastSysTimeElapsed(SurfNum) = SysTimeElapsed;
        //    LastTimeStepSys(SurfNum) = TimeStepSys;

        //    CpFluid = GetSpecificHeatGlycol(PlantLoop(this->LoopNum).FluidName, RefrigInletTemp, PlantLoop(this->LoopNum).FluidIndex, RoutineName);

        //    SafeCopyPlantNode(this->InNode, this->OutNode);
        //    // check for flow
        //    if ((CpFluid > 0.0) && (FlowRate > 0.0)) {
        //        Node(this->OutNode).Temp =
        //            RefrigInletTemp -
        //            this->TotalSurfaceArea * HeatFlux /
        //                (this->RefrigMassFlow * CpFluid); // changed flowrate to coeffs.RefrigMassFlow because FLowrate resets to zero -
        //                                                  // Node(this->OutNode).Enthalpy = Node(this->OutNode).Temp * CpFluid;
        //    }
        //}
    }

    void UpdateIceRinkSourceValAvg(bool &IceRinkOn) // .TRUE. if the radiant system has run this zone time step
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Rick Strand
        //       DATE WRITTEN   November 2000

        // PURPOSE OF THIS SUBROUTINE:
        // To transfer the average value of the heat source/sink over the entire
        // zone time step back to the heat balance routines so that the heat
        // balance algorithms can simulate one last time with the average source
        // to maintain some reasonable amount of continuity and energy balance
        // in the temperature and flux histories.

        // METHODOLOGY EMPLOYED:
        // All of the record keeping for the average term is done in the Update
        // routine so the only other thing that this subroutine does is check to
        // see if the system was even on.  If any average term is non-zero, then
        // one or more of the radiant systems was running.

        // SUBROUTINE PARAMETER DEFINITIONS:
        Real64 const CloseEnough(0.01); // Some arbitrarily small value to avoid zeros and numbers that are almost the same

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:

        IceRinkOn = false;

        // If this was never allocated, then there are no radiant systems in this input file (just RETURN)
        if (!allocated(QRadSysSrcAvg)) return;

        // If it was allocated, then we have to check to see if this was running at all...
        // for (SurfNum = 1; SurfNum <= TotSurfaces; ++SurfNum) {
        //    if (QRadSysSrcAvg(SurfNum) != 0.0) {
        //        LowTempRadSysOn = true;
        //        break; // DO loop
        //    }
        //}

        // QRadSysSource = QRadSysSrcAvg;

        QRadSysSource = 0;
    }

    void IceRinkData::report(bool RunFlag)
    {
        // Using/Aliasing
        
        using DataHVACGlobals::TimeStepSys;
        using DataLoopNode::Node;

        this->CoolPower = abs(this->LoadMet) / this->COP;

        this->RefrigInletTemp = Node(this->InNode).Temp;
        this->RefrigOutletTemp = Node(this->OutNode).Temp;
        // this->RefrigMassFlow = Node(this->InNode).MassFlowRate;
        this->CoolEnergy = this->CoolPower * TimeStepSys * SecInHour;
    }
} // namespace IceRink
} // namespace EnergyPlus
