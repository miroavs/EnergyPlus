// EnergyPlus, Copyright (c) 1996-2020, The Board of Trustees of the University of Illinois,
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

// ObjexxFCL Headers
#include <ObjexxFCL/Array.functions.hh>
#include <ObjexxFCL/Fmath.hh>

// EnergyPlus Headers
#include <EnergyPlus/BranchNodeConnections.hh>
#include <EnergyPlus/ChillerGasAbsorption.hh>
#include <EnergyPlus/CurveManager.hh>
#include <EnergyPlus/DataBranchAirLoopPlant.hh>
#include <EnergyPlus/DataEnvironment.hh>
#include <EnergyPlus/DataHVACGlobals.hh>
#include <EnergyPlus/DataIPShortCuts.hh>
#include <EnergyPlus/DataLoopNode.hh>
#include <EnergyPlus/DataPlant.hh>
#include <EnergyPlus/DataPrecisionGlobals.hh>
#include <EnergyPlus/DataSizing.hh>
#include <EnergyPlus/EMSManager.hh>
#include <EnergyPlus/FluidProperties.hh>
#include <EnergyPlus/General.hh>
#include <EnergyPlus/GlobalNames.hh>
#include <EnergyPlus/InputProcessing/InputProcessor.hh>
#include <EnergyPlus/NodeInputManager.hh>
#include <EnergyPlus/OutAirNodeManager.hh>
#include <EnergyPlus/OutputProcessor.hh>
#include <EnergyPlus/OutputReportPredefined.hh>
#include <EnergyPlus/PlantUtilities.hh>
#include <EnergyPlus/Psychrometrics.hh>
#include <EnergyPlus/ReportSizingManager.hh>
#include <EnergyPlus/UtilityRoutines.hh>

namespace EnergyPlus {

namespace ChillerGasAbsorption {

    // MODULE INFORMATION:
    //    AUTHOR         Jason Glazer of GARD Analytics, Inc.
    //                   for Gas Research Institute
    //    DATE WRITTEN   March 2001
    //    MODIFIED       Brent Griffith, Nov 2010 plant upgrades, generalize fluid properties
    //    RE-ENGINEERED  na
    // PURPOSE OF THIS MODULE:
    //    This module simulates the performance of the direct fired
    //    absorption chiller.
    // METHODOLOGY EMPLOYED:
    //    Once the PlantLoopManager determines that the absorber chiller
    //    is available to meet a loop cooling demand, it calls SimGasAbsorption
    //    which in turn calls the appropriate Absorption Chiller model.
    // REFERENCES:
    //    DOE-2.1e Supplement
    //    PG&E CoolTools GasMod
    // OTHER NOTES:
    //    The curves on this model follow the DOE-2 approach of using
    //    electric and heat input ratios.  In addition, the temperature
    //    correction curve has two independent variables for the
    //    chilled water temperature and either the entering or leaving
    //    condenser water temperature.
    //    The code was originally adopted from the ChillerAbsorption
    //    routine but has been extensively modified.
    //    Development of this module was funded by the Gas Research Institute.
    //    (Please see copyright and disclaimer information at end of module)

    int NumGasAbsorbers(0); // number of Absorption Chillers specified in input

    Array1D_bool CheckEquipName;

    Array1D<GasAbsorberSpecs> GasAbsorber; // dimension to number of machines
    Array1D<ReportVars> GasAbsorberReport;

    namespace {
        // These were static variables within different functions. They were pulled out into the namespace
        // to facilitate easier unit testing of those functions.
        // These are purposefully not in the header file as an extern variable. No one outside of this should
        // use these. They are cleared by clear_state() for use by unit tests, but normal simulations should be unaffected.
        // This is purposefully in an anonymous namespace so nothing outside this implementation file can use it.
        Real64 Sim_HeatCap(0.0); // W - nominal heating capacity
        bool Sim_GetInput(true); // then TRUE, calls subroutine to read input file.
        bool Get_ErrorsFound(false);
        bool Init_MyOneTimeFlag(true);
        Array1D_bool Init_MyEnvrnFlag;
        Array1D_bool Init_MyPlantScanFlag;
        Real64 Calc_oldCondSupplyTemp(0.0); // save the last iteration value of leaving condenser water temperature
    }

    void SimGasAbsorber(std::string const &EP_UNUSED(AbsorberType), // type of Absorber
                        std::string const &AbsorberName,            // user specified name of Absorber
                        int const EP_UNUSED(EquipFlowCtrl),         // Flow control mode for the equipment
                        int &CompIndex,                             // Absorber number counter
                        bool const RunFlag,                         // simulate Absorber when TRUE
                        bool const FirstIteration,                  // initialize variables when TRUE
                        bool &InitLoopEquip,                        // If not false, calculate the max load for operating conditions
                        Real64 &MyLoad,                             // loop demand component will meet
                        int const BranchInletNodeNum,               // node number of inlet to calling branch,
                        Real64 &MaxCap,                             // W - maximum operating capacity of Absorber
                        Real64 &MinCap,                             // W - minimum operating capacity of Absorber
                        Real64 &OptCap,                             // W - optimal operating capacity of Absorber
                        bool const GetSizingFactor,                 // TRUE when just the sizing factor is requested
                        Real64 &SizingFactor,                       // sizing factor
                        Real64 &TempCondInDesign,
                        Real64 &TempEvapOutDesign)
    {
        //       AUTHOR         Jason Glazer
        //       DATE WRITTEN   March 2001

        // PURPOSE OF THIS SUBROUTINE: This is the Absorption Chiller model driver.  It
        // gets the input for the models, initializes simulation variables, call
        // the appropriate model and sets up reporting variables.

        int ChillNum; // Absorber number counter

        // Get Absorber data from input file
        if (Sim_GetInput) {
            GetGasAbsorberInput();
            Sim_GetInput = false;
        }

        // Find the correct Equipment
        if (CompIndex == 0) {
            ChillNum = UtilityRoutines::FindItemInList(AbsorberName, GasAbsorber);
            if (ChillNum == 0) {
                ShowFatalError("SimGasAbsorber: Unit not found=" + AbsorberName);
            }
            CompIndex = ChillNum;
        } else {
            ChillNum = CompIndex;
            if (ChillNum > NumGasAbsorbers || ChillNum < 1) {
                ShowFatalError("SimGasAbsorber:  Invalid CompIndex passed=" + General::TrimSigDigits(ChillNum) +
                               ", Number of Units=" + General::TrimSigDigits(NumGasAbsorbers) + ", Entered Unit name=" + AbsorberName);
            }
            if (CheckEquipName(ChillNum)) {
                if (AbsorberName != GasAbsorber(ChillNum).Name) {
                    ShowFatalError("SimGasAbsorber: Invalid CompIndex passed=" + General::TrimSigDigits(ChillNum) + ", Unit name=" + AbsorberName +
                                   ", stored Unit Name for that index=" + GasAbsorber(ChillNum).Name);
                }
                CheckEquipName(ChillNum) = false;
            }
        }

        // Check that this is a valid call
        if (InitLoopEquip) {
            TempEvapOutDesign = GasAbsorber(ChillNum).TempDesCHWSupply;
            TempCondInDesign = GasAbsorber(ChillNum).TempDesCondReturn;
            InitGasAbsorber(ChillNum, RunFlag);

            // Match inlet node name of calling branch to determine if this call is for heating or cooling
            if (BranchInletNodeNum == GasAbsorber(ChillNum).ChillReturnNodeNum) { // Operate as chiller
                SizeGasAbsorber(ChillNum);                                        // only call from chilled water loop
                MinCap = GasAbsorber(ChillNum).NomCoolingCap * GasAbsorber(ChillNum).MinPartLoadRat;
                MaxCap = GasAbsorber(ChillNum).NomCoolingCap * GasAbsorber(ChillNum).MaxPartLoadRat;
                OptCap = GasAbsorber(ChillNum).NomCoolingCap * GasAbsorber(ChillNum).OptPartLoadRat;
            } else if (BranchInletNodeNum == GasAbsorber(ChillNum).HeatReturnNodeNum) { // Operate as heater
                Sim_HeatCap = GasAbsorber(ChillNum).NomCoolingCap * GasAbsorber(ChillNum).NomHeatCoolRatio;
                MinCap = Sim_HeatCap * GasAbsorber(ChillNum).MinPartLoadRat;
                MaxCap = Sim_HeatCap * GasAbsorber(ChillNum).MaxPartLoadRat;
                OptCap = Sim_HeatCap * GasAbsorber(ChillNum).OptPartLoadRat;
            } else if (BranchInletNodeNum == GasAbsorber(ChillNum).CondReturnNodeNum) { // called from condenser loop
                Sim_HeatCap = 0.0;
                MinCap = 0.0;
                MaxCap = 0.0;
                OptCap = 0.0;
            } else { // Error, nodes do not match
                ShowSevereError("SimGasAbsorber: Invalid call to Gas Absorbtion Chiller-Heater " + AbsorberName);
                ShowContinueError("Node connections in branch are not consistent with object nodes.");
                ShowFatalError("Preceding conditions cause termination.");
            } // Operate as Chiller or Heater
            if (GetSizingFactor) {
                SizingFactor = GasAbsorber(ChillNum).SizFac;
            }
            return;
        }

        // Match inlet node name of calling branch to determine if this call is for heating or cooling
        if (BranchInletNodeNum == GasAbsorber(ChillNum).ChillReturnNodeNum) { // Operate as chiller
            // Calculate Node Values
            // Calculate Equipment and Update Variables
            GasAbsorber(ChillNum).InCoolingMode = RunFlag != 0;
            InitGasAbsorber(ChillNum, RunFlag);
            CalcGasAbsorberChillerModel(ChillNum, MyLoad, RunFlag);
            UpdateGasAbsorberCoolRecords(MyLoad, RunFlag, ChillNum);
        } else if (BranchInletNodeNum == GasAbsorber(ChillNum).HeatReturnNodeNum) { // Operate as heater
            // Calculate Node Values
            // Calculate Equipment and Update Variables
            GasAbsorber(ChillNum).InHeatingMode = RunFlag != 0;
            InitGasAbsorber(ChillNum, RunFlag);
            CalcGasAbsorberHeaterModel(ChillNum, MyLoad, RunFlag);
            UpdateGasAbsorberHeatRecords(MyLoad, RunFlag, ChillNum);
        } else if (BranchInletNodeNum == GasAbsorber(ChillNum).CondReturnNodeNum) { // called from condenser loop
            if (GasAbsorber(ChillNum).CDLoopNum > 0) {
                PlantUtilities::UpdateChillerComponentCondenserSide(GasAbsorber(ChillNum).CDLoopNum,
                                                    GasAbsorber(ChillNum).CDLoopSideNum,
                                                    DataPlant::TypeOf_Chiller_DFAbsorption,
                                                    GasAbsorber(ChillNum).CondReturnNodeNum,
                                                    GasAbsorber(ChillNum).CondSupplyNodeNum,
                                                    GasAbsorberReport(ChillNum).TowerLoad,
                                                    GasAbsorberReport(ChillNum).CondReturnTemp,
                                                    GasAbsorberReport(ChillNum).CondSupplyTemp,
                                                    GasAbsorberReport(ChillNum).CondWaterFlowRate,
                                                    FirstIteration);
            }
        } else { // Error, nodes do not match
            ShowSevereError("Invalid call to Gas Absorber Chiller " + AbsorberName);
            ShowContinueError("Node connections in branch are not consistent with object nodes.");
            ShowFatalError("Preceding conditions cause termination.");
        }
    }

    void GetGasAbsorberInput()
    {
        //       AUTHOR:          Jason Glazer
        //       DATE WRITTEN:    March 2001
        // This routine will get the input
        // required by the Direct Fired Absorption chiller model in the object ChillerHeater:Absorption:DirectFired

        using namespace DataIPShortCuts; // Data for field names, blank numerics
        using BranchNodeConnections::TestCompSet;
        using CurveManager::GetCurveCheck;
        using DataSizing::AutoSize;
        using GlobalNames::VerifyUniqueChillerName;
        using NodeInputManager::GetOnlySingleNode;
        using OutAirNodeManager::CheckAndAddAirNodeNumber;

        int AbsorberNum; // Absorber counter
        int NumAlphas;   // Number of elements in the alpha array
        int NumNums;     // Number of elements in the numeric array
        int IOStat;      // IO Status when calling get input subroutine
        std::string ChillerName;
        bool Okay;

        // FLOW
        cCurrentModuleObject = "ChillerHeater:Absorption:DirectFired";
        NumGasAbsorbers = inputProcessor->getNumObjectsFound(cCurrentModuleObject);

        if (NumGasAbsorbers <= 0) {
            ShowSevereError("No " + cCurrentModuleObject + " equipment found in input file");
            Get_ErrorsFound = true;
        }

        if (allocated(GasAbsorber)) return;

        // ALLOCATE ARRAYS
        GasAbsorber.allocate(NumGasAbsorbers);

        GasAbsorberReport.allocate(NumGasAbsorbers);
        CheckEquipName.dimension(NumGasAbsorbers, true);

        // LOAD ARRAYS

        for (AbsorberNum = 1; AbsorberNum <= NumGasAbsorbers; ++AbsorberNum) {
            inputProcessor->getObjectItem(cCurrentModuleObject,
                                          AbsorberNum,
                                          cAlphaArgs,
                                          NumAlphas,
                                          rNumericArgs,
                                          NumNums,
                                          IOStat,
                                          _,
                                          lAlphaFieldBlanks,
                                          cAlphaFieldNames,
                                          cNumericFieldNames);
            UtilityRoutines::IsNameEmpty(cAlphaArgs(1), cCurrentModuleObject, Get_ErrorsFound);

            // Get_ErrorsFound will be set to True if problem was found, left untouched otherwise
            VerifyUniqueChillerName(cCurrentModuleObject, cAlphaArgs(1), Get_ErrorsFound, cCurrentModuleObject + " Name");

            GasAbsorber(AbsorberNum).Name = cAlphaArgs(1);
            ChillerName = cCurrentModuleObject + " Named " + GasAbsorber(AbsorberNum).Name;

            // Assign capacities
            GasAbsorber(AbsorberNum).NomCoolingCap = rNumericArgs(1);
            if (GasAbsorber(AbsorberNum).NomCoolingCap == AutoSize) {
                GasAbsorber(AbsorberNum).NomCoolingCapWasAutoSized = true;
            }
            GasAbsorber(AbsorberNum).NomHeatCoolRatio = rNumericArgs(2);
            // Assign efficiencies
            GasAbsorber(AbsorberNum).FuelCoolRatio = rNumericArgs(3);
            GasAbsorber(AbsorberNum).FuelHeatRatio = rNumericArgs(4);
            GasAbsorber(AbsorberNum).ElecCoolRatio = rNumericArgs(5);
            GasAbsorber(AbsorberNum).ElecHeatRatio = rNumericArgs(6);

            // Assign Node Numbers to specified nodes
            GasAbsorber(AbsorberNum).ChillReturnNodeNum = GetOnlySingleNode(
                cAlphaArgs(2), Get_ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), DataLoopNode::NodeType_Water, DataLoopNode::NodeConnectionType_Inlet, 1, DataLoopNode::ObjectIsNotParent);
            GasAbsorber(AbsorberNum).ChillSupplyNodeNum = GetOnlySingleNode(
                cAlphaArgs(3), Get_ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), DataLoopNode::NodeType_Water, DataLoopNode::NodeConnectionType_Outlet, 1, DataLoopNode::ObjectIsNotParent);
            TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(2), cAlphaArgs(3), "Chilled Water Nodes");
            // Condenser node processing depends on condenser type, see below
            GasAbsorber(AbsorberNum).HeatReturnNodeNum = GetOnlySingleNode(
                cAlphaArgs(6), Get_ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), DataLoopNode::NodeType_Water, DataLoopNode::NodeConnectionType_Inlet, 3, DataLoopNode::ObjectIsNotParent);
            GasAbsorber(AbsorberNum).HeatSupplyNodeNum = GetOnlySingleNode(
                cAlphaArgs(7), Get_ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), DataLoopNode::NodeType_Water, DataLoopNode::NodeConnectionType_Outlet, 3, DataLoopNode::ObjectIsNotParent);
            TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(6), cAlphaArgs(7), "Hot Water Nodes");
            if (Get_ErrorsFound) {
                ShowFatalError("Errors found in processing node input for " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                Get_ErrorsFound = false;
            }

            // Assign Part Load Ratios
            GasAbsorber(AbsorberNum).MinPartLoadRat = rNumericArgs(7);
            GasAbsorber(AbsorberNum).MaxPartLoadRat = rNumericArgs(8);
            GasAbsorber(AbsorberNum).OptPartLoadRat = rNumericArgs(9);
            // Assign Design Conditions
            GasAbsorber(AbsorberNum).TempDesCondReturn = rNumericArgs(10);
            GasAbsorber(AbsorberNum).TempDesCHWSupply = rNumericArgs(11);
            GasAbsorber(AbsorberNum).EvapVolFlowRate = rNumericArgs(12);
            if (GasAbsorber(AbsorberNum).EvapVolFlowRate == AutoSize) {
                GasAbsorber(AbsorberNum).EvapVolFlowRateWasAutoSized = true;
            }
            if (UtilityRoutines::SameString(cAlphaArgs(16), "AirCooled")) {
                GasAbsorber(AbsorberNum).CondVolFlowRate = 0.0011; // Condenser flow rate not used for this cond type
            } else {
                GasAbsorber(AbsorberNum).CondVolFlowRate = rNumericArgs(13);
                if (GasAbsorber(AbsorberNum).CondVolFlowRate == AutoSize) {
                    GasAbsorber(AbsorberNum).CondVolFlowRateWasAutoSized = true;
                }
            }
            GasAbsorber(AbsorberNum).HeatVolFlowRate = rNumericArgs(14);
            if (GasAbsorber(AbsorberNum).HeatVolFlowRate == AutoSize) {
                GasAbsorber(AbsorberNum).HeatVolFlowRateWasAutoSized = true;
            }
            // Assign Curve Numbers
            GasAbsorber(AbsorberNum).CoolCapFTCurve = GetCurveCheck(cAlphaArgs(8), Get_ErrorsFound, ChillerName);
            GasAbsorber(AbsorberNum).FuelCoolFTCurve = GetCurveCheck(cAlphaArgs(9), Get_ErrorsFound, ChillerName);
            GasAbsorber(AbsorberNum).FuelCoolFPLRCurve = GetCurveCheck(cAlphaArgs(10), Get_ErrorsFound, ChillerName);
            GasAbsorber(AbsorberNum).ElecCoolFTCurve = GetCurveCheck(cAlphaArgs(11), Get_ErrorsFound, ChillerName);
            GasAbsorber(AbsorberNum).ElecCoolFPLRCurve = GetCurveCheck(cAlphaArgs(12), Get_ErrorsFound, ChillerName);
            GasAbsorber(AbsorberNum).HeatCapFCoolCurve = GetCurveCheck(cAlphaArgs(13), Get_ErrorsFound, ChillerName);
            GasAbsorber(AbsorberNum).FuelHeatFHPLRCurve = GetCurveCheck(cAlphaArgs(14), Get_ErrorsFound, ChillerName);
            if (Get_ErrorsFound) {
                ShowFatalError("Errors found in processing curve input for " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                Get_ErrorsFound = false;
            }
            if (UtilityRoutines::SameString(cAlphaArgs(15), "LeavingCondenser")) {
                GasAbsorber(AbsorberNum).isEnterCondensTemp = false;
            } else if (UtilityRoutines::SameString(cAlphaArgs(15), "EnteringCondenser")) {
                GasAbsorber(AbsorberNum).isEnterCondensTemp = true;
            } else {
                GasAbsorber(AbsorberNum).isEnterCondensTemp = true;
                ShowWarningError(cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\", invalid value");
                ShowContinueError("Invalid " + cAlphaFieldNames(15) + "=\"" + cAlphaArgs(15) + "\"");
                ShowContinueError("resetting to EnteringCondenser, simulation continues");
            }
            // Assign Other Parameters
            if (UtilityRoutines::SameString(cAlphaArgs(16), "AirCooled")) {
                GasAbsorber(AbsorberNum).isWaterCooled = false;
            } else if (UtilityRoutines::SameString(cAlphaArgs(16), "WaterCooled")) {
                GasAbsorber(AbsorberNum).isWaterCooled = true;
            } else {
                GasAbsorber(AbsorberNum).isWaterCooled = true;
                ShowWarningError(cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\", invalid value");
                ShowContinueError("Invalid " + cAlphaFieldNames(16) + '=' + cAlphaArgs(16));
                ShowContinueError("resetting to WaterCooled, simulation continues");
            }
            if (!GasAbsorber(AbsorberNum).isEnterCondensTemp && !GasAbsorber(AbsorberNum).isWaterCooled) {
                GasAbsorber(AbsorberNum).isEnterCondensTemp = true;
                ShowWarningError(cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\", invalid value");
                ShowContinueError("Invalid to have both LeavingCondenser and AirCooled.");
                ShowContinueError("resetting to EnteringCondenser, simulation continues");
            }
            if (GasAbsorber(AbsorberNum).isWaterCooled) {
                if (lAlphaFieldBlanks(5)) {
                    ShowSevereError(cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\", invalid value");
                    ShowContinueError("For WaterCooled chiller the condenser outlet node is required.");
                    Get_ErrorsFound = true;
                }
                GasAbsorber(AbsorberNum).CondReturnNodeNum = GetOnlySingleNode(cAlphaArgs(4),
                                                                               Get_ErrorsFound,
                                                                               cCurrentModuleObject,
                                                                               cAlphaArgs(1),
                                                                               DataLoopNode::NodeType_Water,
                                                                               DataLoopNode::NodeConnectionType_Inlet,
                                                                               2,
                                                                               DataLoopNode::ObjectIsNotParent);
                GasAbsorber(AbsorberNum).CondSupplyNodeNum = GetOnlySingleNode(cAlphaArgs(5),
                                                                               Get_ErrorsFound,
                                                                               cCurrentModuleObject,
                                                                               cAlphaArgs(1),
                                                                               DataLoopNode::NodeType_Water,
                                                                               DataLoopNode::NodeConnectionType_Outlet,
                                                                               2,
                                                                               DataLoopNode::ObjectIsNotParent);
                TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(4), cAlphaArgs(5), "Condenser Water Nodes");
            } else {
                GasAbsorber(AbsorberNum).CondReturnNodeNum = GetOnlySingleNode(cAlphaArgs(4),
                                                                               Get_ErrorsFound,
                                                                               cCurrentModuleObject,
                                                                               cAlphaArgs(1),
                                                                               DataLoopNode::NodeType_Air,
                                                                               DataLoopNode::NodeConnectionType_OutsideAirReference,
                                                                               2,
                                                                               DataLoopNode::ObjectIsNotParent);
                // Condenser outlet node not used for air or evap cooled condenser so ingore cAlphaArgs( 5 )
                // Connection not required for air or evap cooled condenser so no call to TestCompSet here
                CheckAndAddAirNodeNumber(GasAbsorber(AbsorberNum).CondReturnNodeNum, Okay);
                if (!Okay) {
                    ShowWarningError(cCurrentModuleObject + ", Adding OutdoorAir:Node=" + cAlphaArgs(4));
                }
            }
            GasAbsorber(AbsorberNum).CHWLowLimitTemp = rNumericArgs(15);
            GasAbsorber(AbsorberNum).FuelHeatingValue = rNumericArgs(16);
            GasAbsorber(AbsorberNum).SizFac = rNumericArgs(17);

            // Fuel Type Case Statement
            {
                auto const SELECT_CASE_var(cAlphaArgs(17));
                if (SELECT_CASE_var == "NATURALGAS") {
                    GasAbsorber(AbsorberNum).FuelType = "Gas";

                } else if (SELECT_CASE_var == "DIESEL") {
                    GasAbsorber(AbsorberNum).FuelType = "Diesel";

                } else if (SELECT_CASE_var == "GASOLINE") {
                    GasAbsorber(AbsorberNum).FuelType = "Gasoline";

                } else if (SELECT_CASE_var == "FUELOILNO1") {
                    GasAbsorber(AbsorberNum).FuelType = "FuelOil#1";

                } else if (SELECT_CASE_var == "FUELOILNO2") {
                    GasAbsorber(AbsorberNum).FuelType = "FuelOil#2";

                } else if (SELECT_CASE_var == "PROPANE") {
                    GasAbsorber(AbsorberNum).FuelType = "Propane";

                } else if (SELECT_CASE_var == "OTHERFUEL1") {
                    GasAbsorber(AbsorberNum).FuelType = "OtherFuel1";

                } else if (SELECT_CASE_var == "OTHERFUEL2") {
                    GasAbsorber(AbsorberNum).FuelType = "OtherFuel2";

                } else {
                    ShowSevereError(cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\", invalid value");
                    ShowContinueError("Invalid " + cAlphaFieldNames(17) + '=' + cAlphaArgs(17));
                    ShowContinueError(
                        "Valid choices are Electricity, NaturalGas, Propane, Diesel, Gasoline, FuelOilNo1, FuelOilNo2,OtherFuel1 or OtherFuel2");
                    Get_ErrorsFound = true;
                }
            }
        }

        if (Get_ErrorsFound) {
            ShowFatalError("Errors found in processing input for " + cCurrentModuleObject);
        }

        for (AbsorberNum = 1; AbsorberNum <= NumGasAbsorbers; ++AbsorberNum) {
            ChillerName = GasAbsorber(AbsorberNum).Name;

            SetupOutputVariable("Chiller Heater Evaporator Cooling Rate",
                                OutputProcessor::Unit::W,
                                GasAbsorberReport(AbsorberNum).CoolingLoad,
                                "System",
                                "Average",
                                ChillerName);
            SetupOutputVariable("Chiller Heater Evaporator Cooling Energy",
                                OutputProcessor::Unit::J,
                                GasAbsorberReport(AbsorberNum).CoolingEnergy,
                                "System",
                                "Sum",
                                ChillerName,
                                _,
                                "ENERGYTRANSFER",
                                "CHILLERS",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller Heater Heating Rate",
                                OutputProcessor::Unit::W,
                                GasAbsorberReport(AbsorberNum).HeatingLoad,
                                "System",
                                "Average",
                                ChillerName);
            SetupOutputVariable("Chiller Heater Heating Energy",
                                OutputProcessor::Unit::J,
                                GasAbsorberReport(AbsorberNum).HeatingEnergy,
                                "System",
                                "Sum",
                                ChillerName,
                                _,
                                "ENERGYTRANSFER",
                                "BOILERS",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller Heater Condenser Heat Transfer Rate",
                                OutputProcessor::Unit::W,
                                GasAbsorberReport(AbsorberNum).TowerLoad,
                                "System",
                                "Average",
                                ChillerName);
            SetupOutputVariable("Chiller Heater Condenser Heat Transfer Energy",
                                OutputProcessor::Unit::J,
                                GasAbsorberReport(AbsorberNum).TowerEnergy,
                                "System",
                                "Sum",
                                ChillerName,
                                _,
                                "ENERGYTRANSFER",
                                "HEATREJECTION",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller Heater " + GasAbsorber(AbsorberNum).FuelType + " Rate",
                                OutputProcessor::Unit::W,
                                GasAbsorberReport(AbsorberNum).FuelUseRate,
                                "System",
                                "Average",
                                ChillerName);
            // Do not include this on meters, this would duplicate the cool fuel and heat fuel
            SetupOutputVariable("Chiller Heater " + GasAbsorber(AbsorberNum).FuelType + " Energy",
                                OutputProcessor::Unit::J,
                                GasAbsorberReport(AbsorberNum).FuelEnergy,
                                "System",
                                "Sum",
                                ChillerName);

            SetupOutputVariable("Chiller Heater Cooling " + GasAbsorber(AbsorberNum).FuelType + " Rate",
                                OutputProcessor::Unit::W,
                                GasAbsorberReport(AbsorberNum).CoolFuelUseRate,
                                "System",
                                "Average",
                                ChillerName);
            SetupOutputVariable("Chiller Heater Cooling " + GasAbsorber(AbsorberNum).FuelType + " Energy",
                                OutputProcessor::Unit::J,
                                GasAbsorberReport(AbsorberNum).CoolFuelEnergy,
                                "System",
                                "Sum",
                                ChillerName,
                                _,
                                GasAbsorber(AbsorberNum).FuelType,
                                "Cooling",
                                _,
                                "Plant");

            SetupOutputVariable(
                "Chiller Heater Cooling COP", OutputProcessor::Unit::W_W, GasAbsorberReport(AbsorberNum).FuelCOP, "System", "Average", ChillerName);

            SetupOutputVariable("Chiller Heater Heating " + GasAbsorber(AbsorberNum).FuelType + " Rate",
                                OutputProcessor::Unit::W,
                                GasAbsorberReport(AbsorberNum).HeatFuelUseRate,
                                "System",
                                "Average",
                                ChillerName);
            SetupOutputVariable("Chiller Heater Heating " + GasAbsorber(AbsorberNum).FuelType + " Energy",
                                OutputProcessor::Unit::J,
                                GasAbsorberReport(AbsorberNum).HeatFuelEnergy,
                                "System",
                                "Sum",
                                ChillerName,
                                _,
                                GasAbsorber(AbsorberNum).FuelType,
                                "Heating",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller Heater Electric Power",
                                OutputProcessor::Unit::W,
                                GasAbsorberReport(AbsorberNum).ElectricPower,
                                "System",
                                "Average",
                                ChillerName);
            // Do not include this on meters, this would duplicate the cool electric and heat electric
            SetupOutputVariable("Chiller Heater Electric Energy",
                                OutputProcessor::Unit::J,
                                GasAbsorberReport(AbsorberNum).ElectricEnergy,
                                "System",
                                "Sum",
                                ChillerName);

            SetupOutputVariable("Chiller Heater Cooling Electric Power",
                                OutputProcessor::Unit::W,
                                GasAbsorberReport(AbsorberNum).CoolElectricPower,
                                "System",
                                "Average",
                                ChillerName);
            SetupOutputVariable("Chiller Heater Cooling Electric Energy",
                                OutputProcessor::Unit::J,
                                GasAbsorberReport(AbsorberNum).CoolElectricEnergy,
                                "System",
                                "Sum",
                                ChillerName,
                                _,
                                "Electricity",
                                "Cooling",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller Heater Heating Electric Power",
                                OutputProcessor::Unit::W,
                                GasAbsorberReport(AbsorberNum).HeatElectricPower,
                                "System",
                                "Average",
                                ChillerName);
            SetupOutputVariable("Chiller Heater Heating Electric Energy",
                                OutputProcessor::Unit::J,
                                GasAbsorberReport(AbsorberNum).HeatElectricEnergy,
                                "System",
                                "Sum",
                                ChillerName,
                                _,
                                "Electricity",
                                "Heating",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller Heater Evaporator Inlet Temperature",
                                OutputProcessor::Unit::C,
                                GasAbsorberReport(AbsorberNum).ChillReturnTemp,
                                "System",
                                "Average",
                                ChillerName);
            SetupOutputVariable("Chiller Heater Evaporator Outlet Temperature",
                                OutputProcessor::Unit::C,
                                GasAbsorberReport(AbsorberNum).ChillSupplyTemp,
                                "System",
                                "Average",
                                ChillerName);
            SetupOutputVariable("Chiller Heater Evaporator Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                GasAbsorberReport(AbsorberNum).ChillWaterFlowRate,
                                "System",
                                "Average",
                                ChillerName);

            if (GasAbsorber(AbsorberNum).isWaterCooled) {
                SetupOutputVariable("Chiller Heater Condenser Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    GasAbsorberReport(AbsorberNum).CondReturnTemp,
                                    "System",
                                    "Average",
                                    ChillerName);
                SetupOutputVariable("Chiller Heater Condenser Outlet Temperature",
                                    OutputProcessor::Unit::C,
                                    GasAbsorberReport(AbsorberNum).CondSupplyTemp,
                                    "System",
                                    "Average",
                                    ChillerName);
                SetupOutputVariable("Chiller Heater Condenser Mass Flow Rate",
                                    OutputProcessor::Unit::kg_s,
                                    GasAbsorberReport(AbsorberNum).CondWaterFlowRate,
                                    "System",
                                    "Average",
                                    ChillerName);
            } else {
                SetupOutputVariable("Chiller Heater Condenser Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    GasAbsorberReport(AbsorberNum).CondReturnTemp,
                                    "System",
                                    "Average",
                                    ChillerName);
            }

            SetupOutputVariable("Chiller Heater Heating Inlet Temperature",
                                OutputProcessor::Unit::C,
                                GasAbsorberReport(AbsorberNum).HotWaterReturnTemp,
                                "System",
                                "Average",
                                ChillerName);
            SetupOutputVariable("Chiller Heater Heating Outlet Temperature",
                                OutputProcessor::Unit::C,
                                GasAbsorberReport(AbsorberNum).HotWaterSupplyTemp,
                                "System",
                                "Average",
                                ChillerName);
            SetupOutputVariable("Chiller Heater Heating Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                GasAbsorberReport(AbsorberNum).HotWaterFlowRate,
                                "System",
                                "Average",
                                ChillerName);

            SetupOutputVariable("Chiller Heater Cooling Part Load Ratio",
                                OutputProcessor::Unit::None,
                                GasAbsorberReport(AbsorberNum).CoolPartLoadRatio,
                                "System",
                                "Average",
                                ChillerName);
            SetupOutputVariable("Chiller Heater Maximum Cooling Rate",
                                OutputProcessor::Unit::W,
                                GasAbsorberReport(AbsorberNum).CoolingCapacity,
                                "System",
                                "Average",
                                ChillerName);
            SetupOutputVariable("Chiller Heater Heating Part Load Ratio",
                                OutputProcessor::Unit::None,
                                GasAbsorberReport(AbsorberNum).HeatPartLoadRatio,
                                "System",
                                "Average",
                                ChillerName);
            SetupOutputVariable("Chiller Heater Maximum Heating Rate",
                                OutputProcessor::Unit::W,
                                GasAbsorberReport(AbsorberNum).HeatingCapacity,
                                "System",
                                "Average",
                                ChillerName);

            SetupOutputVariable("Chiller Heater Runtime Fraction",
                                OutputProcessor::Unit::None,
                                GasAbsorberReport(AbsorberNum).FractionOfPeriodRunning,
                                "System",
                                "Average",
                                ChillerName);
        }
    }

    void InitGasAbsorber(int const ChillNum,           // number of the current engine driven chiller being simulated
                         bool const EP_UNUSED(RunFlag) // TRUE when chiller operating
    )
    {
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   June 2003

        // This subroutine is for initializations of direct fired absorption chiller
        // components.

        // Uses the status flags to trigger initializations.

        std::string const RoutineName("InitGasAbsorber");

        int CondInletNode;  // node number of water inlet node to the condenser
        int CondOutletNode; // node number of water outlet node from the condenser
        int HeatInletNode;  // node number of hot water inlet node
        int HeatOutletNode; // node number of hot water outlet node
        bool errFlag;
        Real64 rho;  // local fluid density
        Real64 mdot; // lcoal fluid mass flow rate

        // Do the one time initializations
        if (Init_MyOneTimeFlag) {
            Init_MyPlantScanFlag.allocate(NumGasAbsorbers);
            Init_MyEnvrnFlag.dimension(NumGasAbsorbers, true);
            Init_MyOneTimeFlag = false;
            Init_MyPlantScanFlag = true;
        }

        // Init more variables
        if (Init_MyPlantScanFlag(ChillNum)) {
            // Locate the chillers on the plant loops for later usage
            errFlag = false;
            PlantUtilities::ScanPlantLoopsForObject(GasAbsorber(ChillNum).Name,
                                    DataPlant::TypeOf_Chiller_DFAbsorption,
                                    GasAbsorber(ChillNum).CWLoopNum,
                                    GasAbsorber(ChillNum).CWLoopSideNum,
                                    GasAbsorber(ChillNum).CWBranchNum,
                                    GasAbsorber(ChillNum).CWCompNum,
                                    errFlag,
                                    GasAbsorber(ChillNum).CHWLowLimitTemp,
                                    _,
                                    _,
                                    GasAbsorber(ChillNum).ChillReturnNodeNum,
                                    _);
            if (errFlag) {
                ShowFatalError("InitGasAbsorber: Program terminated due to previous condition(s).");
            }

            PlantUtilities::ScanPlantLoopsForObject(GasAbsorber(ChillNum).Name,
                                    DataPlant::TypeOf_Chiller_DFAbsorption,
                                    GasAbsorber(ChillNum).HWLoopNum,
                                    GasAbsorber(ChillNum).HWLoopSideNum,
                                    GasAbsorber(ChillNum).HWBranchNum,
                                    GasAbsorber(ChillNum).HWCompNum,
                                    errFlag,
                                    _,
                                    _,
                                    _,
                                    GasAbsorber(ChillNum).HeatReturnNodeNum,
                                    _);
            if (errFlag) {
                ShowFatalError("InitGasAbsorber: Program terminated due to previous condition(s).");
            }

            if (GasAbsorber(ChillNum).isWaterCooled) {
                PlantUtilities::ScanPlantLoopsForObject(GasAbsorber(ChillNum).Name,
                                        DataPlant::TypeOf_Chiller_DFAbsorption,
                                        GasAbsorber(ChillNum).CDLoopNum,
                                        GasAbsorber(ChillNum).CDLoopSideNum,
                                        GasAbsorber(ChillNum).CDBranchNum,
                                        GasAbsorber(ChillNum).CDCompNum,
                                        errFlag,
                                        _,
                                        _,
                                        _,
                                        GasAbsorber(ChillNum).CondReturnNodeNum,
                                        _);
                if (errFlag) {
                    ShowFatalError("InitGasAbsorber: Program terminated due to previous condition(s).");
                }
                PlantUtilities::InterConnectTwoPlantLoopSides(GasAbsorber(ChillNum).CWLoopNum,
                                              GasAbsorber(ChillNum).CWLoopSideNum,
                                              GasAbsorber(ChillNum).CDLoopNum,
                                              GasAbsorber(ChillNum).CDLoopSideNum,
                                              DataPlant::TypeOf_Chiller_DFAbsorption,
                                              true);
                PlantUtilities::InterConnectTwoPlantLoopSides(GasAbsorber(ChillNum).HWLoopNum,
                                              GasAbsorber(ChillNum).HWLoopSideNum,
                                              GasAbsorber(ChillNum).CDLoopNum,
                                              GasAbsorber(ChillNum).CDLoopSideNum,
                                              DataPlant::TypeOf_Chiller_DFAbsorption,
                                              true);
            }

            PlantUtilities::InterConnectTwoPlantLoopSides(GasAbsorber(ChillNum).CWLoopNum,
                                          GasAbsorber(ChillNum).CWLoopSideNum,
                                          GasAbsorber(ChillNum).HWLoopNum,
                                          GasAbsorber(ChillNum).HWLoopSideNum,
                                          DataPlant::TypeOf_Chiller_DFAbsorption,
                                          true);

            // check if outlet node of chilled water side has a setpoint.
            if ((DataLoopNode::Node(GasAbsorber(ChillNum).ChillSupplyNodeNum).TempSetPoint == DataLoopNode::SensedNodeFlagValue) &&
                (DataLoopNode::Node(GasAbsorber(ChillNum).ChillSupplyNodeNum).TempSetPointHi == DataLoopNode::SensedNodeFlagValue)) {
                if (!DataGlobals::AnyEnergyManagementSystemInModel) {
                    if (!GasAbsorber(ChillNum).ChillSetPointErrDone) {
                        ShowWarningError("Missing temperature setpoint on cool side for chiller heater named " + GasAbsorber(ChillNum).Name);
                        ShowContinueError("  A temperature setpoint is needed at the outlet node of this chiller, use a SetpointManager");
                        ShowContinueError("  The overall loop setpoint will be assumed for chiller. The simulation continues ... ");
                        GasAbsorber(ChillNum).ChillSetPointErrDone = true;
                    }
                } else {
                    // need call to EMS to check node
                    errFlag = false; // but not really fatal yet, but should be.
                    EMSManager::CheckIfNodeSetPointManagedByEMS(GasAbsorber(ChillNum).ChillSupplyNodeNum, EMSManager::iTemperatureSetPoint, errFlag);
                    if (errFlag) {
                        if (!GasAbsorber(ChillNum).ChillSetPointErrDone) {
                            ShowWarningError("Missing temperature setpoint on cool side for chiller heater named " + GasAbsorber(ChillNum).Name);
                            ShowContinueError("  A temperature setpoint is needed at the outlet node of this chiller evaporator ");
                            ShowContinueError("  use a Setpoint Manager to establish a setpoint at the chiller evaporator outlet node ");
                            ShowContinueError("  or use an EMS actuator to establish a setpoint at the outlet node ");
                            ShowContinueError("  The overall loop setpoint will be assumed for chiller. The simulation continues ... ");
                            GasAbsorber(ChillNum).ChillSetPointErrDone = true;
                        }
                    }
                }
                GasAbsorber(ChillNum).ChillSetPointSetToLoop = true;
                DataLoopNode::Node(GasAbsorber(ChillNum).ChillSupplyNodeNum).TempSetPoint =
                        DataLoopNode::Node(DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).TempSetPointNodeNum).TempSetPoint;
                DataLoopNode::Node(GasAbsorber(ChillNum).ChillSupplyNodeNum).TempSetPointHi =
                        DataLoopNode::Node(DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).TempSetPointNodeNum).TempSetPointHi;
            }
            // check if outlet node of hot water side has a setpoint.
            if ((DataLoopNode::Node(GasAbsorber(ChillNum).HeatSupplyNodeNum).TempSetPoint == DataLoopNode::SensedNodeFlagValue) &&
                (DataLoopNode::Node(GasAbsorber(ChillNum).HeatSupplyNodeNum).TempSetPointLo == DataLoopNode::SensedNodeFlagValue)) {
                if (!DataGlobals::AnyEnergyManagementSystemInModel) {
                    if (!GasAbsorber(ChillNum).HeatSetPointErrDone) {
                        ShowWarningError("Missing temperature setpoint on heat side for chiller heater named " + GasAbsorber(ChillNum).Name);
                        ShowContinueError("  A temperature setpoint is needed at the outlet node of this chiller, use a SetpointManager");
                        ShowContinueError("  The overall loop setpoint will be assumed for chiller. The simulation continues ... ");
                        GasAbsorber(ChillNum).HeatSetPointErrDone = true;
                    }
                } else {
                    // need call to EMS to check node
                    errFlag = false; // but not really fatal yet, but should be.
                    EMSManager::CheckIfNodeSetPointManagedByEMS(GasAbsorber(ChillNum).HeatSupplyNodeNum, EMSManager::iTemperatureSetPoint, errFlag);
                    if (errFlag) {
                        if (!GasAbsorber(ChillNum).HeatSetPointErrDone) {
                            ShowWarningError("Missing temperature setpoint on heat side for chiller heater named " + GasAbsorber(ChillNum).Name);
                            ShowContinueError("  A temperature setpoint is needed at the outlet node of this chiller heater ");
                            ShowContinueError("  use a Setpoint Manager to establish a setpoint at the heater side outlet node ");
                            ShowContinueError("  or use an EMS actuator to establish a setpoint at the outlet node ");
                            ShowContinueError("  The overall loop setpoint will be assumed for heater side. The simulation continues ... ");
                            GasAbsorber(ChillNum).HeatSetPointErrDone = true;
                        }
                    }
                }
                GasAbsorber(ChillNum).HeatSetPointSetToLoop = true;
                DataLoopNode::Node(GasAbsorber(ChillNum).HeatSupplyNodeNum).TempSetPoint =
                        DataLoopNode::Node(DataPlant::PlantLoop(GasAbsorber(ChillNum).HWLoopNum).TempSetPointNodeNum).TempSetPoint;
                DataLoopNode::Node(GasAbsorber(ChillNum).HeatSupplyNodeNum).TempSetPointLo =
                        DataLoopNode::Node(DataPlant::PlantLoop(GasAbsorber(ChillNum).HWLoopNum).TempSetPointNodeNum).TempSetPointLo;
            }
            Init_MyPlantScanFlag(ChillNum) = false;
        }

        CondInletNode = GasAbsorber(ChillNum).CondReturnNodeNum;
        CondOutletNode = GasAbsorber(ChillNum).CondSupplyNodeNum;
        HeatInletNode = GasAbsorber(ChillNum).HeatReturnNodeNum;
        HeatOutletNode = GasAbsorber(ChillNum).HeatSupplyNodeNum;

        if (Init_MyEnvrnFlag(ChillNum) && DataGlobals::BeginEnvrnFlag && (DataPlant::PlantFirstSizesOkayToFinalize)) {

            if (GasAbsorber(ChillNum).isWaterCooled) {
                // init max available condenser water flow rate
                if (GasAbsorber(ChillNum).CDLoopNum > 0) {
                    rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(GasAbsorber(ChillNum).CDLoopNum).FluidName,
                                           DataGlobals::CWInitConvTemp,
                                           DataPlant::PlantLoop(GasAbsorber(ChillNum).CDLoopNum).FluidIndex,
                                           RoutineName);
                } else {
                    rho = Psychrometrics::RhoH2O(DataGlobals::InitConvTemp);
                }

                GasAbsorber(ChillNum).DesCondMassFlowRate = rho * GasAbsorber(ChillNum).CondVolFlowRate;
                PlantUtilities::InitComponentNodes(0.0,
                                   GasAbsorber(ChillNum).DesCondMassFlowRate,
                                   CondInletNode,
                                   CondOutletNode,
                                   GasAbsorber(ChillNum).CDLoopNum,
                                   GasAbsorber(ChillNum).CDLoopSideNum,
                                   GasAbsorber(ChillNum).CDBranchNum,
                                   GasAbsorber(ChillNum).CDCompNum);
            }

            if (GasAbsorber(ChillNum).HWLoopNum > 0) {
                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(GasAbsorber(ChillNum).HWLoopNum).FluidName,
                                       DataGlobals::HWInitConvTemp,
                                       DataPlant::PlantLoop(GasAbsorber(ChillNum).HWLoopNum).FluidIndex,
                                       RoutineName);
            } else {
                rho = Psychrometrics::RhoH2O(DataGlobals::InitConvTemp);
            }
            GasAbsorber(ChillNum).DesHeatMassFlowRate = rho * GasAbsorber(ChillNum).HeatVolFlowRate;
            // init available hot water flow rate
            PlantUtilities::InitComponentNodes(0.0,
                               GasAbsorber(ChillNum).DesHeatMassFlowRate,
                               HeatInletNode,
                               HeatOutletNode,
                               GasAbsorber(ChillNum).HWLoopNum,
                               GasAbsorber(ChillNum).HWLoopSideNum,
                               GasAbsorber(ChillNum).HWBranchNum,
                               GasAbsorber(ChillNum).HWCompNum);

            if (GasAbsorber(ChillNum).CWLoopNum > 0) {
                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).FluidName,
                                       DataGlobals::CWInitConvTemp,
                                       DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).FluidIndex,
                                       RoutineName);
            } else {
                rho = Psychrometrics::RhoH2O(DataGlobals::InitConvTemp);
            }
            GasAbsorber(ChillNum).DesEvapMassFlowRate = rho * GasAbsorber(ChillNum).EvapVolFlowRate;
            // init available hot water flow rate
            PlantUtilities::InitComponentNodes(0.0,
                               GasAbsorber(ChillNum).DesEvapMassFlowRate,
                               GasAbsorber(ChillNum).ChillReturnNodeNum,
                               GasAbsorber(ChillNum).ChillSupplyNodeNum,
                               GasAbsorber(ChillNum).CWLoopNum,
                               GasAbsorber(ChillNum).CWLoopSideNum,
                               GasAbsorber(ChillNum).CWBranchNum,
                               GasAbsorber(ChillNum).CWCompNum);

            Init_MyEnvrnFlag(ChillNum) = false;
        }

        if (!DataGlobals::BeginEnvrnFlag) {
            Init_MyEnvrnFlag(ChillNum) = true;
        }

        // this component model works off setpoints on the leaving node
        // fill from plant if needed
        if (GasAbsorber(ChillNum).ChillSetPointSetToLoop) {
            DataLoopNode::Node(GasAbsorber(ChillNum).ChillSupplyNodeNum).TempSetPoint =
                    DataLoopNode::Node(DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).TempSetPointNodeNum).TempSetPoint;
            DataLoopNode::Node(GasAbsorber(ChillNum).ChillSupplyNodeNum).TempSetPointHi =
                    DataLoopNode::Node(DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).TempSetPointNodeNum).TempSetPointHi;
        }

        if (GasAbsorber(ChillNum).HeatSetPointSetToLoop) {
            DataLoopNode::Node(GasAbsorber(ChillNum).HeatSupplyNodeNum).TempSetPoint =
                    DataLoopNode::Node(DataPlant::PlantLoop(GasAbsorber(ChillNum).HWLoopNum).TempSetPointNodeNum).TempSetPoint;
            DataLoopNode::Node(GasAbsorber(ChillNum).HeatSupplyNodeNum).TempSetPointLo =
                    DataLoopNode::Node(DataPlant::PlantLoop(GasAbsorber(ChillNum).HWLoopNum).TempSetPointNodeNum).TempSetPointLo;
        }

        if ((GasAbsorber(ChillNum).isWaterCooled) && ((GasAbsorber(ChillNum).InHeatingMode) || (GasAbsorber(ChillNum).InCoolingMode)) &&
            (!Init_MyPlantScanFlag(ChillNum))) {
            mdot = GasAbsorber(ChillNum).DesCondMassFlowRate;
            // DSU removed, this has to have been wrong (?)  Node(CondInletNode)%Temp  = GasAbsorber(ChillNum)%TempDesCondReturn

            PlantUtilities::SetComponentFlowRate(mdot,
                                 GasAbsorber(ChillNum).CondReturnNodeNum,
                                 GasAbsorber(ChillNum).CondSupplyNodeNum,
                                 GasAbsorber(ChillNum).CDLoopNum,
                                 GasAbsorber(ChillNum).CDLoopSideNum,
                                 GasAbsorber(ChillNum).CDBranchNum,
                                 GasAbsorber(ChillNum).CDCompNum);

        } else {
            mdot = 0.0;
            if (GasAbsorber(ChillNum).CDLoopNum > 0 && GasAbsorber(ChillNum).isWaterCooled) {
                PlantUtilities::SetComponentFlowRate(mdot,
                                     GasAbsorber(ChillNum).CondReturnNodeNum,
                                     GasAbsorber(ChillNum).CondSupplyNodeNum,
                                     GasAbsorber(ChillNum).CDLoopNum,
                                     GasAbsorber(ChillNum).CDLoopSideNum,
                                     GasAbsorber(ChillNum).CDBranchNum,
                                     GasAbsorber(ChillNum).CDCompNum);
            }
        }
    }

    void SizeGasAbsorber(int const ChillNum)
    {
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   June 2003
        //       MODIFIED       November 2013 Daeho Kang, add component sizing table entries

        // This subroutine is for sizing direct fired gas absorption chiller components for which
        // capacities and flow rates have not been specified in the input.

        // METHODOLOGY EMPLOYED:
        // Obtains evaporator flow rate from the plant sizing array. Calculates nominal capacity from
        // the evaporator flow rate and the chilled water loop design delta T. The condenser flow rate
        // is calculated from the nominal capacity, the COP, and the condenser loop design delta T.

        std::string const RoutineName("SizeGasAbsorber");

        int PltSizCoolNum; // Plant Sizing index for cooling loop
        int PltSizHeatNum; // Plant Sizing index for heating loop
        int PltSizCondNum; // Plant Sizing index for condenser loop

        bool ErrorsFound; // If errors detected in input
        std::string equipName;
        Real64 Cp;                     // local fluid specific heat
        Real64 rho;                    // local fluid density
        Real64 tmpNomCap;              // local nominal capacity cooling power
        Real64 tmpEvapVolFlowRate;     // local evaporator design volume flow rate
        Real64 tmpCondVolFlowRate;     // local condenser design volume flow rate
        Real64 tmpHeatRecVolFlowRate;  // local heat recovery design volume flow rate
        Real64 NomCapUser;             // Hardsized nominal capacity for reporting
        Real64 EvapVolFlowRateUser;    // Hardsized evaporator volume flow rate for reporting
        Real64 CondVolFlowRateUser;    // Hardsized condenser flow rate for reporting
        Real64 HeatRecVolFlowRateUser; // Hardsized generator flow rate for reporting

        PltSizCoolNum = 0;
        PltSizCondNum = 0;
        PltSizHeatNum = 0;
        ErrorsFound = false;
        tmpNomCap = GasAbsorber(ChillNum).NomCoolingCap;
        tmpEvapVolFlowRate = GasAbsorber(ChillNum).EvapVolFlowRate;
        tmpCondVolFlowRate = GasAbsorber(ChillNum).CondVolFlowRate;
        tmpHeatRecVolFlowRate = GasAbsorber(ChillNum).HeatVolFlowRate;
        NomCapUser = 0.0;
        EvapVolFlowRateUser = 0.0;
        CondVolFlowRateUser = 0.0;
        HeatRecVolFlowRateUser = 0.0;

        if (GasAbsorber(ChillNum).isWaterCooled) PltSizCondNum = DataPlant::PlantLoop(GasAbsorber(ChillNum).CDLoopNum).PlantSizNum;
        PltSizHeatNum = DataPlant::PlantLoop(GasAbsorber(ChillNum).HWLoopNum).PlantSizNum;
        PltSizCoolNum = DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).PlantSizNum;

        if (PltSizCoolNum > 0) {
            if (DataSizing::PlantSizData(PltSizCoolNum).DesVolFlowRate >= DataHVACGlobals::SmallWaterVolFlow) {
                Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).FluidName,
                                           DataGlobals::CWInitConvTemp,
                                           DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).FluidIndex,
                                           RoutineName);
                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).FluidName,
                                       DataGlobals::CWInitConvTemp,
                                       DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).FluidIndex,
                                       RoutineName);
                tmpNomCap = Cp * rho * DataSizing::PlantSizData(PltSizCoolNum).DeltaT * DataSizing::PlantSizData(PltSizCoolNum).DesVolFlowRate * GasAbsorber(ChillNum).SizFac;
                if (!GasAbsorber(ChillNum).NomCoolingCapWasAutoSized) tmpNomCap = GasAbsorber(ChillNum).NomCoolingCap;
            } else {
                if (GasAbsorber(ChillNum).NomCoolingCapWasAutoSized) tmpNomCap = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (GasAbsorber(ChillNum).NomCoolingCapWasAutoSized) {
                    GasAbsorber(ChillNum).NomCoolingCap = tmpNomCap;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                           GasAbsorber(ChillNum).Name,
                                           "Design Size Nominal Cooling Capacity [W]",
                                           tmpNomCap);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                           GasAbsorber(ChillNum).Name,
                                           "Initial Design Size Nominal Cooling Capacity [W]",
                                           tmpNomCap);
                    }
                } else {
                    if (GasAbsorber(ChillNum).NomCoolingCap > 0.0 && tmpNomCap > 0.0) {
                        NomCapUser = GasAbsorber(ChillNum).NomCoolingCap;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                               GasAbsorber(ChillNum).Name,
                                               "Design Size Nominal Cooling Capacity [W]",
                                               tmpNomCap,
                                               "User-Specified Nominal Cooling Capacity [W]",
                                               NomCapUser);
                            if (DataGlobals::DisplayExtraWarnings) {
                                if ((std::abs(tmpNomCap - NomCapUser) / NomCapUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeChillerHeaterAbsorptionDirectFired: Potential issue with equipment sizing for " +
                                                GasAbsorber(ChillNum).Name);
                                    ShowContinueError("User-Specified Nominal Capacity of " + General::RoundSigDigits(NomCapUser, 2) + " [W]");
                                    ShowContinueError("differs from Design Size Nominal Capacity of " + General::RoundSigDigits(tmpNomCap, 2) + " [W]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpNomCap = NomCapUser;
                    }
                }
            }
        } else {
            if (GasAbsorber(ChillNum).NomCoolingCapWasAutoSized) {
                if (DataPlant::PlantFirstSizesOkayToFinalize) {
                    ShowSevereError("SizeGasAbsorber: ChillerHeater:Absorption:DirectFired=\"" + GasAbsorber(ChillNum).Name + "\", autosize error.");
                    ShowContinueError("Autosizing of Direct Fired Absorption Chiller nominal cooling capacity requires");
                    ShowContinueError("a cooling loop Sizing:Plant object.");
                    ErrorsFound = true;
                }
            } else {
                if (DataPlant::PlantFinalSizesOkayToReport) {
                    if (GasAbsorber(ChillNum).NomCoolingCap > 0.0) {
                        ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                           GasAbsorber(ChillNum).Name,
                                           "User-Specified Nominal Capacity [W]",
                                           GasAbsorber(ChillNum).NomCoolingCap);
                    }
                }
            }
        }

        if (PltSizCoolNum > 0) {
            if (DataSizing::PlantSizData(PltSizCoolNum).DesVolFlowRate >= DataHVACGlobals::SmallWaterVolFlow) {
                tmpEvapVolFlowRate = DataSizing::PlantSizData(PltSizCoolNum).DesVolFlowRate * GasAbsorber(ChillNum).SizFac;
                if (!GasAbsorber(ChillNum).EvapVolFlowRateWasAutoSized) tmpEvapVolFlowRate = GasAbsorber(ChillNum).EvapVolFlowRate;
            } else {
                if (GasAbsorber(ChillNum).EvapVolFlowRateWasAutoSized) tmpEvapVolFlowRate = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (GasAbsorber(ChillNum).EvapVolFlowRateWasAutoSized) {
                    GasAbsorber(ChillNum).EvapVolFlowRate = tmpEvapVolFlowRate;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                           GasAbsorber(ChillNum).Name,
                                           "Design Size Design Chilled Water Flow Rate [m3/s]",
                                           tmpEvapVolFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                           GasAbsorber(ChillNum).Name,
                                           "Initial Design Size Design Chilled Water Flow Rate [m3/s]",
                                           tmpEvapVolFlowRate);
                    }
                } else {
                    if (GasAbsorber(ChillNum).EvapVolFlowRate > 0.0 && tmpEvapVolFlowRate > 0.0) {
                        EvapVolFlowRateUser = GasAbsorber(ChillNum).EvapVolFlowRate;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                               GasAbsorber(ChillNum).Name,
                                               "Design Size Design Chilled Water Flow Rate [m3/s]",
                                               tmpEvapVolFlowRate,
                                               "User-Specified Design Chilled Water Flow Rate [m3/s]",
                                               EvapVolFlowRateUser);
                            if (DataGlobals::DisplayExtraWarnings) {
                                if ((std::abs(tmpEvapVolFlowRate - EvapVolFlowRateUser) / EvapVolFlowRateUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeChillerAbsorptionDirectFired: Potential issue with equipment sizing for " +
                                                GasAbsorber(ChillNum).Name);
                                    ShowContinueError("User-Specified Design Chilled Water Flow Rate of " + General::RoundSigDigits(EvapVolFlowRateUser, 5) +
                                                      " [m3/s]");
                                    ShowContinueError("differs from Design Size Design Chilled Water Flow Rate of " +
                                                              General::RoundSigDigits(tmpEvapVolFlowRate, 5) + " [m3/s]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpEvapVolFlowRate = EvapVolFlowRateUser;
                    }
                }
            }
        } else {
            if (GasAbsorber(ChillNum).EvapVolFlowRateWasAutoSized) {
                if (DataPlant::PlantFirstSizesOkayToFinalize) {
                    ShowSevereError("SizeGasAbsorber: ChillerHeater:Absorption:DirectFired=\"" + GasAbsorber(ChillNum).Name + "\", autosize error.");
                    ShowContinueError("Autosizing of Direct Fired Absorption Chiller evap flow rate requires");
                    ShowContinueError("a cooling loop Sizing:Plant object.");
                    ErrorsFound = true;
                }
            } else {
                if (DataPlant::PlantFinalSizesOkayToReport) {
                    if (GasAbsorber(ChillNum).EvapVolFlowRate > 0.0) {
                        ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                           GasAbsorber(ChillNum).Name,
                                           "User-Specified Design Chilled Water Flow Rate [m3/s]",
                                           GasAbsorber(ChillNum).EvapVolFlowRate);
                    }
                }
            }
        }

        PlantUtilities::RegisterPlantCompDesignFlow(GasAbsorber(ChillNum).ChillReturnNodeNum, tmpEvapVolFlowRate);

        if (PltSizHeatNum > 0) {
            if (DataSizing::PlantSizData(PltSizHeatNum).DesVolFlowRate >= DataHVACGlobals::SmallWaterVolFlow) {
                tmpHeatRecVolFlowRate = DataSizing::PlantSizData(PltSizHeatNum).DesVolFlowRate * GasAbsorber(ChillNum).SizFac;
                if (!GasAbsorber(ChillNum).HeatVolFlowRateWasAutoSized) tmpHeatRecVolFlowRate = GasAbsorber(ChillNum).HeatVolFlowRate;

            } else {
                if (GasAbsorber(ChillNum).HeatVolFlowRateWasAutoSized) tmpHeatRecVolFlowRate = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (GasAbsorber(ChillNum).HeatVolFlowRateWasAutoSized) {
                    GasAbsorber(ChillNum).HeatVolFlowRate = tmpHeatRecVolFlowRate;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                           GasAbsorber(ChillNum).Name,
                                           "Design Size Design Hot Water Flow Rate [m3/s]",
                                           tmpHeatRecVolFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                           GasAbsorber(ChillNum).Name,
                                           "Initial Design Size Design Hot Water Flow Rate [m3/s]",
                                           tmpHeatRecVolFlowRate);
                    }
                } else {
                    if (GasAbsorber(ChillNum).HeatVolFlowRate > 0.0 && tmpHeatRecVolFlowRate > 0.0) {
                        HeatRecVolFlowRateUser = GasAbsorber(ChillNum).HeatVolFlowRate;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                               GasAbsorber(ChillNum).Name,
                                               "Design Size Design Hot Water Flow Rate [m3/s]",
                                               tmpHeatRecVolFlowRate,
                                               "User-Specified Design Hot Water Flow Rate [m3/s]",
                                               HeatRecVolFlowRateUser);
                            if (DataGlobals::DisplayExtraWarnings) {
                                if ((std::abs(tmpHeatRecVolFlowRate - HeatRecVolFlowRateUser) / HeatRecVolFlowRateUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeChillerHeaterAbsorptionDirectFired: Potential issue with equipment sizing for " +
                                                GasAbsorber(ChillNum).Name);
                                    ShowContinueError("User-Specified Design Hot Water Flow Rate of " + General::RoundSigDigits(HeatRecVolFlowRateUser, 5) +
                                                      " [m3/s]");
                                    ShowContinueError("differs from Design Size Design Hot Water Flow Rate of " +
                                                              General::RoundSigDigits(tmpHeatRecVolFlowRate, 5) + " [m3/s]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpHeatRecVolFlowRate = HeatRecVolFlowRateUser;
                    }
                }
            }
        } else {
            if (GasAbsorber(ChillNum).HeatVolFlowRateWasAutoSized) {
                if (DataPlant::PlantFirstSizesOkayToFinalize) {
                    ShowSevereError("SizeGasAbsorber: ChillerHeater:Absorption:DirectFired=\"" + GasAbsorber(ChillNum).Name + "\", autosize error.");
                    ShowContinueError("Autosizing of Direct Fired Absorption Chiller hot water flow rate requires");
                    ShowContinueError("a heating loop Sizing:Plant object.");
                    ErrorsFound = true;
                }
            } else {
                if (DataPlant::PlantFinalSizesOkayToReport) {
                    if (GasAbsorber(ChillNum).HeatVolFlowRate > 0.0) {
                        ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                           GasAbsorber(ChillNum).Name,
                                           "User-Specified Design Hot Water Flow Rate [m3/s]",
                                           GasAbsorber(ChillNum).HeatVolFlowRate);
                    }
                }
            }
        }

        PlantUtilities::RegisterPlantCompDesignFlow(GasAbsorber(ChillNum).HeatReturnNodeNum, tmpHeatRecVolFlowRate);

        if (PltSizCondNum > 0 && PltSizCoolNum > 0) {
            if (DataSizing::PlantSizData(PltSizCoolNum).DesVolFlowRate >= DataHVACGlobals::SmallWaterVolFlow && tmpNomCap > 0.0) {

                Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(GasAbsorber(ChillNum).CDLoopNum).FluidName,
                                           GasAbsorber(ChillNum).TempDesCondReturn,
                                           DataPlant::PlantLoop(GasAbsorber(ChillNum).CDLoopNum).FluidIndex,
                                           RoutineName);
                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(GasAbsorber(ChillNum).CDLoopNum).FluidName,
                                       GasAbsorber(ChillNum).TempDesCondReturn,
                                       DataPlant::PlantLoop(GasAbsorber(ChillNum).CDLoopNum).FluidIndex,
                                       RoutineName);
                tmpCondVolFlowRate = tmpNomCap * (1.0 + GasAbsorber(ChillNum).FuelCoolRatio) / (DataSizing::PlantSizData(PltSizCondNum).DeltaT * Cp * rho);
                if (!GasAbsorber(ChillNum).CondVolFlowRateWasAutoSized) tmpCondVolFlowRate = GasAbsorber(ChillNum).CondVolFlowRate;
                // IF (PlantFirstSizesOkayToFinalize) GasAbsorber(ChillNum)%CondVolFlowRate = tmpCondVolFlowRate
            } else {
                if (GasAbsorber(ChillNum).CondVolFlowRateWasAutoSized) tmpCondVolFlowRate = 0.0;
                // IF (PlantFirstSizesOkayToFinalize) GasAbsorber(ChillNum)%CondVolFlowRate = tmpCondVolFlowRate
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (GasAbsorber(ChillNum).CondVolFlowRateWasAutoSized) {
                    GasAbsorber(ChillNum).CondVolFlowRate = tmpCondVolFlowRate;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                           GasAbsorber(ChillNum).Name,
                                           "Design Size Design Condenser Water Flow Rate [m3/s]",
                                           tmpCondVolFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                           GasAbsorber(ChillNum).Name,
                                           "Initial Design Size Design Condenser Water Flow Rate [m3/s]",
                                           tmpCondVolFlowRate);
                    }
                } else {
                    if (GasAbsorber(ChillNum).CondVolFlowRate > 0.0 && tmpCondVolFlowRate > 0.0) {
                        CondVolFlowRateUser = GasAbsorber(ChillNum).CondVolFlowRate;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                               GasAbsorber(ChillNum).Name,
                                               "Design Size Design Condenser Water Flow Rate [m3/s]",
                                               tmpCondVolFlowRate,
                                               "User-Specified Design Condenser Water Flow Rate [m3/s]",
                                               CondVolFlowRateUser);
                            if (DataGlobals::DisplayExtraWarnings) {
                                if ((std::abs(tmpCondVolFlowRate - CondVolFlowRateUser) / CondVolFlowRateUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeChillerAbsorptionDirectFired: Potential issue with equipment sizing for " +
                                                GasAbsorber(ChillNum).Name);
                                    ShowContinueError("User-Specified Design Condenser Water Flow Rate of " + General::RoundSigDigits(CondVolFlowRateUser, 5) +
                                                      " [m3/s]");
                                    ShowContinueError("differs from Design Size Design Condenser Water Flow Rate of " +
                                                              General::RoundSigDigits(tmpCondVolFlowRate, 5) + " [m3/s]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpCondVolFlowRate = CondVolFlowRateUser;
                    }
                }
            }
        } else {
            if (GasAbsorber(ChillNum).CondVolFlowRateWasAutoSized) {
                if (DataPlant::PlantFirstSizesOkayToFinalize) {
                    ShowSevereError("SizeGasAbsorber: ChillerHeater:Absorption:DirectFired=\"" + GasAbsorber(ChillNum).Name + "\", autosize error.");
                    ShowContinueError("Autosizing of Direct Fired Absorption Chiller condenser flow rate requires a condenser");
                    ShowContinueError("loop Sizing:Plant object.");
                    ErrorsFound = true;
                }
            } else {
                if (DataPlant::PlantFinalSizesOkayToReport) {
                    if (GasAbsorber(ChillNum).CondVolFlowRate > 0.0) {
                        ReportSizingManager::ReportSizingOutput("ChillerHeater:Absorption:DirectFired",
                                           GasAbsorber(ChillNum).Name,
                                           "User-Specified Design Condenser Water Flow Rate [m3/s]",
                                           GasAbsorber(ChillNum).CondVolFlowRate);
                    }
                }
            }
        }

        // save the design condenser water volumetric flow rate for use by the condenser water loop sizing algorithms
        if (GasAbsorber(ChillNum).isWaterCooled) PlantUtilities::RegisterPlantCompDesignFlow(GasAbsorber(ChillNum).CondReturnNodeNum, tmpCondVolFlowRate);

        if (ErrorsFound) {
            ShowFatalError("Preceding sizing errors cause program termination");
        }

        if (DataPlant::PlantFinalSizesOkayToReport) {
            // create predefined report
            equipName = GasAbsorber(ChillNum).Name;
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechType, equipName, "ChillerHeater:Absorption:DirectFired");
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechNomEff, equipName, GasAbsorber(ChillNum).FuelCoolRatio);
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechNomCap, equipName, GasAbsorber(ChillNum).NomCoolingCap);
        }
    }

    void CalcGasAbsorberChillerModel(int &ChillNum,                // Absorber number
                                     Real64 &MyLoad,               // operating load
                                     bool const EP_UNUSED(RunFlag) // TRUE when Absorber operating
    )
    {
        //       AUTHOR         Jason Glazer
        //       DATE WRITTEN   March 2001

        // Simulate a direct fired (gas consuming) absorption chiller using
        // curves and inputs similar to DOE-2.1e

        // METHODOLOGY EMPLOYED:
        // Curve fit of performance data

        // REFERENCES:
        // 1.  DOE-2.1e Supplement and source code
        // 2.  CoolTools GasMod work

        // FlowLock = 0  if mass flow rates may be changed by loop components
        // FlowLock = 1  if mass flow rates may not be changed by loop components

        std::string const RoutineName("CalcGasAbsorberChillerModel");

        // Local copies of GasAbsorberSpecs Type
        // all variables that are local copies of data structure
        // variables are prefaced with an "l" for local.
        Real64 lNomCoolingCap;     // W - design nominal capacity of Absorber
        Real64 lFuelCoolRatio;     // ratio of fuel input to cooling output
        Real64 lFuelHeatRatio;     // ratio of fuel input to heating output
        Real64 lElecCoolRatio;     // ratio of electricity input to cooling output
        int lChillReturnNodeNum;   // Node number on the inlet side of the plant
        int lChillSupplyNodeNum;   // Node number on the outlet side of the plant
        int lCondReturnNodeNum;    // Node number on the inlet side of the condenser
        int lCondSupplyNodeNum;    // Node number on the outlet side of the condenser
        Real64 lMinPartLoadRat;    // min allowed operating frac full load
        Real64 lMaxPartLoadRat;    // max allowed operating frac full load
        Real64 lOptPartLoadRat;    // optimal operating frac full load
        Real64 lTempDesCondReturn; // design secondary loop fluid temperature at the Absorber condenser side inlet
        Real64 lTempDesCHWSupply;  // design chilled water supply temperature
        Real64 lCondVolFlowRate;   // m**3/s - design nominal water volumetric flow rate through the condenser
        int lCoolCapFTCurve;       // cooling capacity as a function of temperature curve
        int lFuelCoolFTCurve;      // Fuel-Input-to cooling output Ratio Function of Temperature Curve
        int lFuelCoolFPLRCurve;    // Fuel-Input-to cooling output Ratio Function of Part Load Ratio Curve
        int lElecCoolFTCurve;      // Electric-Input-to cooling output Ratio Function of Temperature Curve
        int lElecCoolFPLRCurve;    // Electric-Input-to cooling output Ratio Function of Part Load Ratio Curve
        bool lIsEnterCondensTemp;  // if using entering conderser water temperature is TRUE, exiting is FALSE
        bool lIsWaterCooled;       // if water cooled it is TRUE
        Real64 lCHWLowLimitTemp;   // Chilled Water Lower Limit Temperature
        Real64 lFuelHeatingValue;
        // Local copies of GasAbsorberReportVars Type
        Real64 lCoolingLoad(0.0); // cooling load on the chiller (previously called QEvap)
        // Real64 lCoolingEnergy( 0.0 ); // variable to track total cooling load for period (was EvapEnergy)
        Real64 lTowerLoad(0.0); // load on the cooling tower/condenser (previously called QCond)
        // Real64 lTowerEnergy( 0.0 ); // variable to track total tower load for a period (was CondEnergy)
        // Real64 lFuelUseRate( 0.0 ); // instantaneous use of gas for period
        // Real64 lFuelEnergy( 0.0 ); // variable to track total fuel used for a period
        Real64 lCoolFuelUseRate(0.0); // instantaneous use of gas for period for cooling
        // Real64 lCoolFuelEnergy( 0.0 ); // variable to track total fuel used for a period for cooling
        Real64 lHeatFuelUseRate(0.0); // instantaneous use of gas for period for heating
        // Real64 lElectricPower( 0.0 ); // parasitic electric power used (was PumpingPower)
        // Real64 lElectricEnergy( 0.0 ); // track the total electricity used for a period (was PumpingEnergy)
        Real64 lCoolElectricPower(0.0); // parasitic electric power used  for cooling
        // Real64 lCoolElectricEnergy( 0.0 ); // track the total electricity used for a period for cooling
        Real64 lHeatElectricPower(0.0);        // parasitic electric power used  for heating
        Real64 lChillReturnTemp(0.0);          // reporting: evaporator inlet temperature (was EvapInletTemp)
        Real64 lChillSupplyTemp(0.0);          // reporting: evaporator outlet temperature (was EvapOutletTemp)
        Real64 lChillWaterMassFlowRate(0.0);   // reporting: evaporator mass flow rate (was Evapmdot)
        Real64 lCondReturnTemp(0.0);           // reporting: condenser inlet temperature (was CondInletTemp)
        Real64 lCondSupplyTemp(0.0);           // reporting: condenser outlet temperature (was CondOutletTemp)
        Real64 lCondWaterMassFlowRate(0.0);    // reporting: condenser mass flow rate (was Condmdot)
        Real64 lCoolPartLoadRatio(0.0);        // operating part load ratio (load/capacity for cooling)
        Real64 lHeatPartLoadRatio(0.0);        // operating part load ratio (load/capacity for heating)
        Real64 lAvailableCoolingCapacity(0.0); // current capacity after temperature adjustment
        Real64 lFractionOfPeriodRunning(0.0);
        Real64 PartLoadRat(0.0);           // actual operating part load ratio of unit (ranges from minplr to 1)
        Real64 lChillWaterMassflowratemax; // Maximum flow rate through the evaporator

        // other local variables
        Real64 ChillDeltaTemp; // chilled water temperature difference
        Real64 ChillSupplySetPointTemp(0.0);

        Real64 calcCondTemp; // the condenser temperature used for curve calculation
        // either return or supply depending on user input
        Real64 revisedEstimateAvailCap; // final estimate of available capacity if using leaving
        // condenser water temperature
        Real64 errorAvailCap; // error fraction on final estimate of AvailableCoolingCapacity
        int LoopNum;
        int LoopSideNum;
        Real64 rhoCW; // local fluid density for chilled water
        Real64 Cp_CW; // local fluid specific heat for chilled water
        Real64 rhoCD; // local fluid density for condenser water
        Real64 Cp_CD; // local fluid specific heat for condenser water

        // set node values to data structure values for nodes

        lChillReturnNodeNum = GasAbsorber(ChillNum).ChillReturnNodeNum;
        lChillSupplyNodeNum = GasAbsorber(ChillNum).ChillSupplyNodeNum;
        lCondReturnNodeNum = GasAbsorber(ChillNum).CondReturnNodeNum;
        lCondSupplyNodeNum = GasAbsorber(ChillNum).CondSupplyNodeNum;

        // set local copies of data from rest of input structure

        lNomCoolingCap = GasAbsorber(ChillNum).NomCoolingCap;
        lFuelCoolRatio = GasAbsorber(ChillNum).FuelCoolRatio;
        lFuelHeatRatio = GasAbsorber(ChillNum).FuelHeatRatio;
        lElecCoolRatio = GasAbsorber(ChillNum).ElecCoolRatio;
        lMinPartLoadRat = GasAbsorber(ChillNum).MinPartLoadRat;
        lMaxPartLoadRat = GasAbsorber(ChillNum).MaxPartLoadRat;
        lOptPartLoadRat = GasAbsorber(ChillNum).OptPartLoadRat;
        lTempDesCondReturn = GasAbsorber(ChillNum).TempDesCondReturn;
        lTempDesCHWSupply = GasAbsorber(ChillNum).TempDesCHWSupply;
        lCondVolFlowRate = GasAbsorber(ChillNum).CondVolFlowRate;
        lCoolCapFTCurve = GasAbsorber(ChillNum).CoolCapFTCurve;
        lFuelCoolFTCurve = GasAbsorber(ChillNum).FuelCoolFTCurve;
        lFuelCoolFPLRCurve = GasAbsorber(ChillNum).FuelCoolFPLRCurve;
        lElecCoolFTCurve = GasAbsorber(ChillNum).ElecCoolFTCurve;
        lElecCoolFPLRCurve = GasAbsorber(ChillNum).ElecCoolFPLRCurve;
        lIsEnterCondensTemp = GasAbsorber(ChillNum).isEnterCondensTemp;
        lIsWaterCooled = GasAbsorber(ChillNum).isWaterCooled;
        lCHWLowLimitTemp = GasAbsorber(ChillNum).CHWLowLimitTemp;
        lFuelHeatingValue = GasAbsorber(ChillNum).FuelHeatingValue;

        lHeatElectricPower = GasAbsorberReport(ChillNum).HeatElectricPower;
        lHeatFuelUseRate = GasAbsorberReport(ChillNum).HeatFuelUseRate;
        lHeatPartLoadRatio = GasAbsorberReport(ChillNum).HeatPartLoadRatio;

        // initialize entering conditions
        lChillReturnTemp = DataLoopNode::Node(lChillReturnNodeNum).Temp;
        lChillWaterMassFlowRate = DataLoopNode::Node(lChillReturnNodeNum).MassFlowRate;
        lCondReturnTemp = DataLoopNode::Node(lCondReturnNodeNum).Temp;
        lCondWaterMassFlowRate = DataLoopNode::Node(lCondReturnNodeNum).MassFlowRate;
        {
            auto const SELECT_CASE_var(DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).LoopDemandCalcScheme);
            if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                ChillSupplySetPointTemp = DataLoopNode::Node(lChillSupplyNodeNum).TempSetPoint;
            } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                ChillSupplySetPointTemp = DataLoopNode::Node(lChillSupplyNodeNum).TempSetPointHi;
            } else {
                assert(false);
            }
        }
        ChillDeltaTemp = std::abs(lChillReturnTemp - ChillSupplySetPointTemp);

        rhoCW = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).FluidName,
                                 lChillReturnTemp,
                                 DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).FluidIndex,
                                 RoutineName);
        Cp_CW = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).FluidName,
                                      lChillReturnTemp,
                                      DataPlant::PlantLoop(GasAbsorber(ChillNum).CWLoopNum).FluidIndex,
                                      RoutineName);
        if (GasAbsorber(ChillNum).CDLoopNum > 0) {
            rhoCD = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(GasAbsorber(ChillNum).CDLoopNum).FluidName,
                                     lChillReturnTemp,
                                     DataPlant::PlantLoop(GasAbsorber(ChillNum).CDLoopNum).FluidIndex,
                                     RoutineName);
            Cp_CD = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(GasAbsorber(ChillNum).CDLoopNum).FluidName,
                                          lChillReturnTemp,
                                          DataPlant::PlantLoop(GasAbsorber(ChillNum).CDLoopNum).FluidIndex,
                                          RoutineName);
        }

        // If no loop demand or Absorber OFF, return
        // will need to modify when absorber can act as a boiler
        if (MyLoad >= 0 || !((GasAbsorber(ChillNum).InHeatingMode) || (GasAbsorber(ChillNum).InCoolingMode))) {
            // set node temperatures
            lChillSupplyTemp = lChillReturnTemp;
            lCondSupplyTemp = lCondReturnTemp;
            lCondWaterMassFlowRate = 0.0;
            if (lIsWaterCooled) {
                PlantUtilities::SetComponentFlowRate(lCondWaterMassFlowRate,
                                     GasAbsorber(ChillNum).CondReturnNodeNum,
                                     GasAbsorber(ChillNum).CondSupplyNodeNum,
                                     GasAbsorber(ChillNum).CDLoopNum,
                                     GasAbsorber(ChillNum).CDLoopSideNum,
                                     GasAbsorber(ChillNum).CDBranchNum,
                                     GasAbsorber(ChillNum).CDCompNum);
            }
            ChillDeltaTemp = 0.0;
            lFractionOfPeriodRunning = min(1.0, max(lHeatPartLoadRatio, lCoolPartLoadRatio) / lMinPartLoadRat);
        } else {

            // if water cooled use the input node otherwise just use outside air temperature
            if (lIsWaterCooled) {
                // most manufacturers rate have tables of entering condenser water temperature
                // but a few use leaving condenser water temperature so we have a flag
                // when leaving is used it uses the previous iterations value of the value
                lCondReturnTemp = DataLoopNode::Node(lCondReturnNodeNum).Temp;
                if (lIsEnterCondensTemp) {
                    calcCondTemp = lCondReturnTemp;
                } else {
                    if (Calc_oldCondSupplyTemp == 0) {
                        Calc_oldCondSupplyTemp = lCondReturnTemp + 8.0; // if not previously estimated assume 8C greater than return
                    }
                    calcCondTemp = Calc_oldCondSupplyTemp;
                }
                // Set mass flow rates
                lCondWaterMassFlowRate = GasAbsorber(ChillNum).DesCondMassFlowRate;
                PlantUtilities::SetComponentFlowRate(lCondWaterMassFlowRate,
                                     GasAbsorber(ChillNum).CondReturnNodeNum,
                                     GasAbsorber(ChillNum).CondSupplyNodeNum,
                                     GasAbsorber(ChillNum).CDLoopNum,
                                     GasAbsorber(ChillNum).CDLoopSideNum,
                                     GasAbsorber(ChillNum).CDBranchNum,
                                     GasAbsorber(ChillNum).CDCompNum);
            } else {
                // air cooled
                DataLoopNode::Node(lCondReturnNodeNum).Temp = DataLoopNode::Node(lCondReturnNodeNum).OutAirDryBulb;
                calcCondTemp = DataLoopNode::Node(lCondReturnNodeNum).OutAirDryBulb;
                lCondReturnTemp = DataLoopNode::Node(lCondReturnNodeNum).Temp;
                lCondWaterMassFlowRate = 0.0;
                if (GasAbsorber(ChillNum).CDLoopNum > 0) {
                    PlantUtilities::SetComponentFlowRate(lCondWaterMassFlowRate,
                                         GasAbsorber(ChillNum).CondReturnNodeNum,
                                         GasAbsorber(ChillNum).CondSupplyNodeNum,
                                         GasAbsorber(ChillNum).CDLoopNum,
                                         GasAbsorber(ChillNum).CDLoopSideNum,
                                         GasAbsorber(ChillNum).CDBranchNum,
                                         GasAbsorber(ChillNum).CDCompNum);
                }
            }

            // Determine available cooling capacity using the setpoint temperature
            lAvailableCoolingCapacity = lNomCoolingCap * CurveManager::CurveValue(lCoolCapFTCurve, ChillSupplySetPointTemp, calcCondTemp);

            // Calculate current load for cooling
            MyLoad = sign(max(std::abs(MyLoad), lAvailableCoolingCapacity * lMinPartLoadRat), MyLoad);
            MyLoad = sign(min(std::abs(MyLoad), lAvailableCoolingCapacity * lMaxPartLoadRat), MyLoad);

            // Determine the following variables depending on if the flow has been set in
            // the nodes (flowlock=1 to 2) or if the amount of load is still be determined (flowlock=0)
            //    chilled water flow,
            //    cooling load taken by the chiller, and
            //    supply temperature
            lChillWaterMassflowratemax = GasAbsorber(ChillNum).DesEvapMassFlowRate;

            LoopNum = GasAbsorber(ChillNum).CWLoopNum;
            LoopSideNum = GasAbsorber(ChillNum).CWLoopSideNum;
            {
                auto const SELECT_CASE_var(DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock);
                if (SELECT_CASE_var == 0) { // mass flow rates may be changed by loop components
                    GasAbsorber(ChillNum).PossibleSubcooling = false;
                    lCoolingLoad = std::abs(MyLoad);
                    if (ChillDeltaTemp != 0.0) {
                        lChillWaterMassFlowRate = std::abs(lCoolingLoad / (Cp_CW * ChillDeltaTemp));
                        if (lChillWaterMassFlowRate - lChillWaterMassflowratemax > DataBranchAirLoopPlant::MassFlowTolerance) GasAbsorber(ChillNum).PossibleSubcooling = true;

                        PlantUtilities::SetComponentFlowRate(lChillWaterMassFlowRate,
                                             GasAbsorber(ChillNum).ChillReturnNodeNum,
                                             GasAbsorber(ChillNum).ChillSupplyNodeNum,
                                             GasAbsorber(ChillNum).CWLoopNum,
                                             GasAbsorber(ChillNum).CWLoopSideNum,
                                             GasAbsorber(ChillNum).CWBranchNum,
                                             GasAbsorber(ChillNum).CWCompNum);
                        lChillSupplyTemp = ChillSupplySetPointTemp;
                    } else {
                        lChillWaterMassFlowRate = 0.0;
                        ShowRecurringWarningErrorAtEnd("GasAbsorberChillerModel:Cooling\"" + GasAbsorber(ChillNum).Name +
                                                           "\", DeltaTemp = 0 in mass flow calculation",
                                                       GasAbsorber(ChillNum).DeltaTempCoolErrCount);
                    }
                    lChillSupplyTemp = ChillSupplySetPointTemp;
                } else if (SELECT_CASE_var == 1) { // mass flow rates may not be changed by loop components
                    lChillWaterMassFlowRate = DataLoopNode::Node(lChillReturnNodeNum).MassFlowRate;
                    if (GasAbsorber(ChillNum).PossibleSubcooling) {
                        lCoolingLoad = std::abs(MyLoad);

                        ChillDeltaTemp = lCoolingLoad / lChillWaterMassFlowRate / Cp_CW;
                        lChillSupplyTemp = DataLoopNode::Node(lChillReturnNodeNum).Temp - ChillDeltaTemp;
                    } else {
                        ChillDeltaTemp = DataLoopNode::Node(lChillReturnNodeNum).Temp - ChillSupplySetPointTemp;
                        lCoolingLoad = std::abs(lChillWaterMassFlowRate * Cp_CW * ChillDeltaTemp);
                        lChillSupplyTemp = ChillSupplySetPointTemp;
                    }
                    // Check that the Chiller Supply outlet temp honors both plant loop temp low limit and also the chiller low limit
                    if (lChillSupplyTemp < lCHWLowLimitTemp) {
                        if ((DataLoopNode::Node(lChillReturnNodeNum).Temp - lCHWLowLimitTemp) > DataPlant::DeltaTempTol) {
                            lChillSupplyTemp = lCHWLowLimitTemp;
                            ChillDeltaTemp = DataLoopNode::Node(lChillReturnNodeNum).Temp - lChillSupplyTemp;
                            lCoolingLoad = lChillWaterMassFlowRate * Cp_CW * ChillDeltaTemp;
                        } else {
                            lChillSupplyTemp = DataLoopNode::Node(lChillReturnNodeNum).Temp;
                            ChillDeltaTemp = DataLoopNode::Node(lChillReturnNodeNum).Temp - lChillSupplyTemp;
                            lCoolingLoad = lChillWaterMassFlowRate * Cp_CW * ChillDeltaTemp;
                        }
                    }
                    if (lChillSupplyTemp < DataLoopNode::Node(lChillSupplyNodeNum).TempMin) {
                        if ((DataLoopNode::Node(lChillReturnNodeNum).Temp - DataLoopNode::Node(lChillSupplyNodeNum).TempMin) > DataPlant::DeltaTempTol) {
                            lChillSupplyTemp = DataLoopNode::Node(lChillSupplyNodeNum).TempMin;
                            ChillDeltaTemp = DataLoopNode::Node(lChillReturnNodeNum).Temp - lChillSupplyTemp;
                            lCoolingLoad = lChillWaterMassFlowRate * Cp_CW * ChillDeltaTemp;
                        } else {
                            lChillSupplyTemp = DataLoopNode::Node(lChillReturnNodeNum).Temp;
                            ChillDeltaTemp = DataLoopNode::Node(lChillReturnNodeNum).Temp - lChillSupplyTemp;
                            lCoolingLoad = lChillWaterMassFlowRate * Cp_CW * ChillDeltaTemp;
                        }
                    }

                    // Checks Coolingload on the basis of the machine limits.
                    if (lCoolingLoad > std::abs(MyLoad)) {
                        if (lChillWaterMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                            lCoolingLoad = std::abs(MyLoad);
                            ChillDeltaTemp = lCoolingLoad / lChillWaterMassFlowRate / Cp_CW;
                            lChillSupplyTemp = DataLoopNode::Node(lChillReturnNodeNum).Temp - ChillDeltaTemp;
                        } else {
                            lChillSupplyTemp = DataLoopNode::Node(lChillReturnNodeNum).Temp;
                            ChillDeltaTemp = DataLoopNode::Node(lChillReturnNodeNum).Temp - lChillSupplyTemp;
                            lCoolingLoad = lChillWaterMassFlowRate * Cp_CW * ChillDeltaTemp;
                        }
                    }
                }
            }

            // Calculate operating part load ratio for cooling
            PartLoadRat = min(std::abs(MyLoad) / lAvailableCoolingCapacity, lMaxPartLoadRat);
            PartLoadRat = max(lMinPartLoadRat, PartLoadRat);

            if (lAvailableCoolingCapacity > 0.0) {
                if (std::abs(MyLoad) / lAvailableCoolingCapacity < lMinPartLoadRat) {
                    lCoolPartLoadRatio = MyLoad / lAvailableCoolingCapacity;
                } else {
                    lCoolPartLoadRatio = PartLoadRat;
                }
            } else { // Else if AvailableCoolingCapacity < 0.0
                lCoolPartLoadRatio = 0.0;
            }

            // calculate the fraction of the time period that the chiller would be running
            // use maximum from heating and cooling sides
            if (lCoolPartLoadRatio < lMinPartLoadRat || lHeatPartLoadRatio < lMinPartLoadRat) {
                lFractionOfPeriodRunning = min(1.0, max(lHeatPartLoadRatio, lCoolPartLoadRatio) / lMinPartLoadRat);
            } else {
                lFractionOfPeriodRunning = 1.0;
            }

            // Calculate fuel consumption for cooling
            // fuel used for cooling availCap * HIR * HIR-FT * HIR-FPLR
            lCoolFuelUseRate = lAvailableCoolingCapacity * lFuelCoolRatio * CurveManager::CurveValue(lFuelCoolFTCurve, lChillSupplyTemp, calcCondTemp) *
                    CurveManager::CurveValue(lFuelCoolFPLRCurve, lCoolPartLoadRatio) * lFractionOfPeriodRunning;

            // Calculate electric parasitics used
            // based on nominal capacity, not available capacity,
            // electric used for cooling nomCap * %OP * EIR * EIR-FT * EIR-FPLR
            lCoolElectricPower = lNomCoolingCap * lElecCoolRatio * lFractionOfPeriodRunning *
                    CurveManager::CurveValue(lElecCoolFTCurve, lChillSupplyTemp, calcCondTemp) * CurveManager::CurveValue(lElecCoolFPLRCurve, lCoolPartLoadRatio);

            // determine conderser load which is cooling load plus the
            // fuel used for cooling times the burner efficiency plus
            // the electricity used
            lTowerLoad = lCoolingLoad + lCoolFuelUseRate / lFuelHeatRatio + lCoolElectricPower;

            // for water cooled condenser make sure enough flow rate
            // for air cooled condenser just set supply to return temperature
            if (lIsWaterCooled) {
                if (lCondWaterMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                    lCondSupplyTemp = lCondReturnTemp + lTowerLoad / (lCondWaterMassFlowRate * Cp_CD);
                } else {
                    ShowSevereError("CalcGasAbsorberChillerModel: Condenser flow = 0, for Gas Absorber Chiller=" + GasAbsorber(ChillNum).Name);
                    ShowContinueErrorTimeStamp("");
                    ShowFatalError("Program Terminates due to previous error condition.");
                }
            } else {
                lCondSupplyTemp = lCondReturnTemp; // if air cooled condenser just set supply and return to same temperature
            }

            // save the condenser water supply temperature for next iteration if that is used in lookup
            // and if capacity is large enough error than report problem
            Calc_oldCondSupplyTemp = lCondSupplyTemp;
            if (!lIsEnterCondensTemp) {
                // calculate the fraction of the estimated error between the capacity based on the previous
                // iteration's value of condenser supply temperature and the actual calculated condenser supply
                // temperature.  If this becomes too common then may need to iterate a solution instead of
                // relying on previous iteration method.
                revisedEstimateAvailCap = lNomCoolingCap * CurveManager::CurveValue(lCoolCapFTCurve, ChillSupplySetPointTemp, lCondSupplyTemp);
                if (revisedEstimateAvailCap > 0.0) {
                    errorAvailCap = std::abs((revisedEstimateAvailCap - lAvailableCoolingCapacity) / revisedEstimateAvailCap);
                    if (errorAvailCap > 0.05) { // if more than 5% error in estimate
                        ShowRecurringWarningErrorAtEnd("GasAbsorberChillerModel:\"" + GasAbsorber(ChillNum).Name +
                                                           "\", poor Condenser Supply Estimate",
                                                       GasAbsorber(ChillNum).CondErrCount,
                                                       errorAvailCap,
                                                       errorAvailCap);
                    }
                }
            }
        } // IF(MyLoad>=0 .OR. .NOT. RunFlag)
        // Write into the Report Variables except for nodes
        GasAbsorberReport(ChillNum).CoolingLoad = lCoolingLoad;
        GasAbsorberReport(ChillNum).TowerLoad = lTowerLoad;
        GasAbsorberReport(ChillNum).CoolFuelUseRate = lCoolFuelUseRate;
        GasAbsorberReport(ChillNum).CoolElectricPower = lCoolElectricPower;
        GasAbsorberReport(ChillNum).CondReturnTemp = lCondReturnTemp;
        GasAbsorberReport(ChillNum).ChillReturnTemp = lChillReturnTemp;
        GasAbsorberReport(ChillNum).CondSupplyTemp = lCondSupplyTemp;
        GasAbsorberReport(ChillNum).ChillSupplyTemp = lChillSupplyTemp;
        GasAbsorberReport(ChillNum).ChillWaterFlowRate = lChillWaterMassFlowRate;
        GasAbsorberReport(ChillNum).CondWaterFlowRate = lCondWaterMassFlowRate;
        GasAbsorberReport(ChillNum).CoolPartLoadRatio = lCoolPartLoadRatio;
        GasAbsorberReport(ChillNum).CoolingCapacity = lAvailableCoolingCapacity;
        GasAbsorberReport(ChillNum).FractionOfPeriodRunning = lFractionOfPeriodRunning;

        // write the combined heating and cooling fuel used and electric used
        GasAbsorberReport(ChillNum).FuelUseRate = lCoolFuelUseRate + lHeatFuelUseRate;
        GasAbsorberReport(ChillNum).ElectricPower = lCoolElectricPower + lHeatElectricPower;
    }

    void CalcGasAbsorberHeaterModel(int &ChillNum,     // Absorber number
                                    Real64 &MyLoad,    // operating load
                                    bool const RunFlag // TRUE when Absorber operating
    )
    {
        //       AUTHOR         Jason Glazer and Michael J. Witte
        //       DATE WRITTEN   March 2001
        // Simulate a direct fired (gas consuming) absorption chiller using
        // curves and inputs similar to DOE-2.1e

        // METHODOLOGY EMPLOYED:
        // Curve fit of performance data

        // REFERENCES:
        // 1.  DOE-2.1e Supplement and source code
        // 2.  CoolTools GasMod work

        // FlowLock = 0  if mass flow rates may be changed by loop components
        // FlowLock = 1  if mass flow rates may not be changed by loop components
        // FlowLock = 2  if overloaded and mass flow rates has changed to a small amount and Tout drops
        //                 below Setpoint

        // SUBROUTINE PARAMETER DEFINITIONS:
        std::string const RoutineName("CalcGasAbsorberHeaterModel");

        // Local copies of GasAbsorberSpecs Type
        // all variables that are local copies of data structure
        // variables are prefaced with an "l" for local.
        Real64 lNomCoolingCap;    // W - design nominal capacity of Absorber
        Real64 lNomHeatCoolRatio; // ratio of heating to cooling capacity
        Real64 lFuelHeatRatio;    // ratio of fuel input to heating output
        Real64 lElecHeatRatio;    // ratio of electricity input to heating output
        int lHeatReturnNodeNum;   // absorber steam inlet node number, water side
        int lHeatSupplyNodeNum;   // absorber steam outlet node number, water side
        Real64 lMinPartLoadRat;   // min allowed operating frac full load
        Real64 lMaxPartLoadRat;   // max allowed operating frac full load
        Real64 lOptPartLoadRat;   // optimal operating frac full load
        int lHeatCapFCoolCurve;   // Heating Capacity Function of Cooling Capacity Curve
        int lFuelHeatFHPLRCurve;  // Fuel Input to heat output ratio during heating only function
        Real64 lFuelHeatingValue(0.0);
        // Local copies of GasAbsorberReportVars Type
        Real64 lHeatingLoad(0.0); // heating load on the chiller
        // Real64 lHeatingEnergy( 0.0 ); // heating energy
        // Real64 lFuelUseRate( 0.0 ); // instantaneous use of gas for period
        // Real64 lFuelEnergy( 0.0 ); // variable to track total fuel used for a period
        Real64 lCoolFuelUseRate(0.0); // instantaneous use of gas for period for cooling
        Real64 lHeatFuelUseRate(0.0); // instantaneous use of gas for period for heating
        // Real64 lHeatFuelEnergy( 0.0 ); // variable to track total fuel used for a period for heating
        // Real64 lElectricPower( 0.0 ); // parasitic electric power used (was PumpingPower)
        // Real64 lElectricEnergy( 0.0 ); // track the total electricity used for a period (was PumpingEnergy)
        Real64 lCoolElectricPower(0.0); // parasitic electric power used  for cooling
        Real64 lHeatElectricPower(0.0); // parasitic electric power used  for heating
        // Real64 lHeatElectricEnergy( 0.0 ); // track the total electricity used for a period for heating
        Real64 lHotWaterReturnTemp(0.0);       // reporting: hot water return (inlet) temperature
        Real64 lHotWaterSupplyTemp(0.0);       // reporting: hot water supply (outlet) temperature
        Real64 lHotWaterMassFlowRate(0.0);     // reporting: hot water mass flow rate
        Real64 lCoolPartLoadRatio(0.0);        // operating part load ratio (load/capacity for cooling)
        Real64 lHeatPartLoadRatio(0.0);        // operating part load ratio (load/capacity for heating)
        Real64 lAvailableHeatingCapacity(0.0); // current heating capacity
        Real64 lFractionOfPeriodRunning(0.0);
        Real64 lHotWaterMassFlowRateMax(0.0); // Maximum flow rate through the evaporator
        // other local variables
        Real64 HeatDeltaTemp(0.0); // hot water temperature difference
        Real64 HeatSupplySetPointTemp(0.0);
        int LoopNum;
        int LoopSideNum;
        Real64 Cp_HW; // local fluid specific heat for hot water
        Real64 rhoHW; // local fluid density for hot water

        // set node values to data structure values for nodes

        lHeatReturnNodeNum = GasAbsorber(ChillNum).HeatReturnNodeNum;
        lHeatSupplyNodeNum = GasAbsorber(ChillNum).HeatSupplyNodeNum;

        // set local copies of data from rest of input structure

        lNomCoolingCap = GasAbsorber(ChillNum).NomCoolingCap;
        lNomHeatCoolRatio = GasAbsorber(ChillNum).NomHeatCoolRatio;
        lFuelHeatRatio = GasAbsorber(ChillNum).FuelHeatRatio;
        lElecHeatRatio = GasAbsorber(ChillNum).ElecHeatRatio;
        lMinPartLoadRat = GasAbsorber(ChillNum).MinPartLoadRat;
        lMaxPartLoadRat = GasAbsorber(ChillNum).MaxPartLoadRat;
        lOptPartLoadRat = GasAbsorber(ChillNum).OptPartLoadRat;
        lHeatCapFCoolCurve = GasAbsorber(ChillNum).HeatCapFCoolCurve;
        lFuelHeatFHPLRCurve = GasAbsorber(ChillNum).FuelHeatFHPLRCurve;
        lFuelHeatingValue = GasAbsorber(ChillNum).FuelHeatingValue;
        lHotWaterMassFlowRateMax = GasAbsorber(ChillNum).DesHeatMassFlowRate;
        LoopNum = GasAbsorber(ChillNum).HWLoopNum;
        LoopSideNum = GasAbsorber(ChillNum).HWLoopSideNum;

        Cp_HW = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(LoopNum).FluidName, lHotWaterReturnTemp, DataPlant::PlantLoop(LoopNum).FluidIndex, RoutineName);
        rhoHW = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(LoopNum).FluidName, lHotWaterReturnTemp, DataPlant::PlantLoop(LoopNum).FluidIndex, RoutineName);

        lCoolElectricPower = GasAbsorberReport(ChillNum).CoolElectricPower;
        lCoolFuelUseRate = GasAbsorberReport(ChillNum).CoolFuelUseRate;
        lCoolPartLoadRatio = GasAbsorberReport(ChillNum).CoolPartLoadRatio;

        // initialize entering conditions
        lHotWaterReturnTemp = DataLoopNode::Node(lHeatReturnNodeNum).Temp;
        lHotWaterMassFlowRate = DataLoopNode::Node(lHeatReturnNodeNum).MassFlowRate;
        {
            auto const SELECT_CASE_var(DataPlant::PlantLoop(LoopNum).LoopDemandCalcScheme);
            if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                HeatSupplySetPointTemp = DataLoopNode::Node(lHeatSupplyNodeNum).TempSetPoint;
            } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                HeatSupplySetPointTemp = DataLoopNode::Node(lHeatSupplyNodeNum).TempSetPointLo;
            } else {
                assert(false);
            }
        }
        HeatDeltaTemp = std::abs(lHotWaterReturnTemp - HeatSupplySetPointTemp);

        // If no loop demand or Absorber OFF, return
        // will need to modify when absorber can act as a boiler
        if (MyLoad <= 0 || !RunFlag) {
            // set node temperatures
            lHotWaterSupplyTemp = lHotWaterReturnTemp;
            HeatDeltaTemp = 0.0;
            lFractionOfPeriodRunning = min(1.0, max(lHeatPartLoadRatio, lCoolPartLoadRatio) / lMinPartLoadRat);
        } else {

            // Determine available heating capacity using the current cooling load
            lAvailableHeatingCapacity =
                GasAbsorber(ChillNum).NomHeatCoolRatio * GasAbsorber(ChillNum).NomCoolingCap *
                        CurveManager::CurveValue(lHeatCapFCoolCurve, (GasAbsorberReport(ChillNum).CoolingLoad / GasAbsorber(ChillNum).NomCoolingCap));

            // Calculate current load for heating
            MyLoad = sign(max(std::abs(MyLoad), GasAbsorberReport(ChillNum).HeatingCapacity * lMinPartLoadRat), MyLoad);
            MyLoad = sign(min(std::abs(MyLoad), GasAbsorberReport(ChillNum).HeatingCapacity * lMaxPartLoadRat), MyLoad);

            // Determine the following variables depending on if the flow has been set in
            // the nodes (flowlock=1 to 2) or if the amount of load is still be determined (flowlock=0)
            //    chilled water flow,
            //    cooling load taken by the chiller, and
            //    supply temperature
            {
                auto const SELECT_CASE_var(DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock);
                if (SELECT_CASE_var == 0) { // mass flow rates may be changed by loop components
                    lHeatingLoad = std::abs(MyLoad);
                    if (HeatDeltaTemp != 0) {
                        lHotWaterMassFlowRate = std::abs(lHeatingLoad / (Cp_HW * HeatDeltaTemp));

                        PlantUtilities::SetComponentFlowRate(lHotWaterMassFlowRate,
                                             GasAbsorber(ChillNum).HeatReturnNodeNum,
                                             GasAbsorber(ChillNum).HeatSupplyNodeNum,
                                             GasAbsorber(ChillNum).HWLoopNum,
                                             GasAbsorber(ChillNum).HWLoopSideNum,
                                             GasAbsorber(ChillNum).HWBranchNum,
                                             GasAbsorber(ChillNum).HWCompNum);

                    } else {
                        lHotWaterMassFlowRate = 0.0;
                        ShowRecurringWarningErrorAtEnd("GasAbsorberChillerModel:Heating\"" + GasAbsorber(ChillNum).Name +
                                                           "\", DeltaTemp = 0 in mass flow calculation",
                                                       GasAbsorber(ChillNum).DeltaTempHeatErrCount);
                    }
                    lHotWaterSupplyTemp = HeatSupplySetPointTemp;
                } else if (SELECT_CASE_var == 1) { // mass flow rates may not be changed by loop components
                    lHotWaterSupplyTemp = HeatSupplySetPointTemp;
                    lHeatingLoad = std::abs(lHotWaterMassFlowRate * Cp_HW * HeatDeltaTemp);

                    // DSU this "2" is not a real state for flowLock
                } else if (SELECT_CASE_var ==
                           2) { // chiller is underloaded and mass flow rates has changed to a small amount and Tout drops below Setpoint

                    // DSU? this component model needs a lot of work, does not honor limits, incomplete ...

                    // MJW Not sure what to do with this now
                    // Must make adjustment to supply temperature since load is greater than available capacity
                    // this also affects the available capacity itself since it is a function of supply temperature
                    // Since these curves are generally fairly flat just use an estimate (done above) and correction
                    // approach instead of iterating to a solution.
                    // MJW 07MAR01 Logic seems wrong here, because of misunderstanding of what "overload" means
                    //  "overload" means the chiller is overcooling the branch.  See SUBROUTINE DistributeLoad
                    //      IF (lChillWaterMassFlowRate > MassFlowTol) THEN
                    //        ChillDeltaTemp = MyLoad / (CPCW(lChillReturnTemp) * lChillWaterMassFlowRate)
                    //        lChillSupplyTemp = lChillReturnTemp - ChillDeltaTemp
                    //        lAvailableCoolingCapacity = lNomCoolingCap * CurveValue(lCoolCapFTCurve,lChillSupplyTemp,calcCondTemp)
                    //      ELSE
                    //        ErrCount = ErrCount + 1
                    //        IF (ErrCount < 10) THEN
                    //          CALL ShowWarningError('GasAbsorberModel:lChillWaterMassFlowRate near 0 in available capacity calculation')
                    //        END IF
                    //      END IF

                    // MJW 07MAR01 Borrow logic from steam absorption module
                    // The following conditional statements are made to avoid extremely small EvapMdot
                    // & unreasonable EvapOutletTemp due to overloading.
                    // Avoid 'divide by zero' due to small EvapMdot
                    if (lHotWaterMassFlowRate < DataBranchAirLoopPlant::MassFlowTolerance) {
                        HeatDeltaTemp = 0.0;
                    } else {
                        HeatDeltaTemp = std::abs(MyLoad) / (Cp_HW * lHotWaterMassFlowRate);
                    }
                    lHotWaterSupplyTemp = lHotWaterReturnTemp + HeatDeltaTemp;

                    lHeatingLoad = std::abs(lHotWaterMassFlowRate * Cp_HW * HeatDeltaTemp);
                }
            }

            // Calculate operating part load ratio for cooling
            lHeatPartLoadRatio = lHeatingLoad / lAvailableHeatingCapacity;

            // Calculate fuel consumption for cooling
            // fuel used for cooling availCap * HIR * HIR-FT * HIR-FPLR

            lHeatFuelUseRate = lAvailableHeatingCapacity * lFuelHeatRatio * CurveManager::CurveValue(lFuelHeatFHPLRCurve, lHeatPartLoadRatio);

            // calculate the fraction of the time period that the chiller would be running
            // use maximum from heating and cooling sides
            lFractionOfPeriodRunning = min(1.0, max(lHeatPartLoadRatio, lCoolPartLoadRatio) / lMinPartLoadRat);

            // Calculate electric parasitics used
            // for heating based on nominal capacity not available capacity
            lHeatElectricPower = lNomCoolingCap * lNomHeatCoolRatio * lElecHeatRatio * lFractionOfPeriodRunning;
            // Coodinate electric parasitics for heating and cooling to avoid double counting
            // Total electric is the max of heating electric or cooling electric
            // If heating electric is greater, leave cooling electric and subtract if off of heating elec
            // If cooling electric is greater, set heating electric to zero
            if (lHeatElectricPower <= lCoolElectricPower) {
                lHeatElectricPower = 0.0;
            } else {
                lHeatElectricPower -= lCoolElectricPower;
            }

        } // IF(MyLoad==0 .OR. .NOT. RunFlag)
        // Write into the Report Variables except for nodes
        GasAbsorberReport(ChillNum).HeatingLoad = lHeatingLoad;
        GasAbsorberReport(ChillNum).HeatFuelUseRate = lHeatFuelUseRate;
        GasAbsorberReport(ChillNum).HeatElectricPower = lHeatElectricPower;
        GasAbsorberReport(ChillNum).HotWaterReturnTemp = lHotWaterReturnTemp;
        GasAbsorberReport(ChillNum).HotWaterSupplyTemp = lHotWaterSupplyTemp;
        GasAbsorberReport(ChillNum).HotWaterFlowRate = lHotWaterMassFlowRate;
        GasAbsorberReport(ChillNum).HeatPartLoadRatio = lHeatPartLoadRatio;
        GasAbsorberReport(ChillNum).HeatingCapacity = lAvailableHeatingCapacity;
        GasAbsorberReport(ChillNum).FractionOfPeriodRunning = lFractionOfPeriodRunning;

        // write the combined heating and cooling fuel used and electric used
        GasAbsorberReport(ChillNum).FuelUseRate = lCoolFuelUseRate + lHeatFuelUseRate;
        GasAbsorberReport(ChillNum).ElectricPower = lCoolElectricPower + lHeatElectricPower;
    }

    void UpdateGasAbsorberCoolRecords(Real64 const MyLoad, // current load
                                      bool const RunFlag,  // TRUE if Absorber operating
                                      int const ChillNum   // Absorber number
    )
    {
        //       AUTHOR         Jason Glazer
        //       DATE WRITTEN   March 2001

        int lChillReturnNodeNum = GasAbsorber(ChillNum).ChillReturnNodeNum;
        int lChillSupplyNodeNum = GasAbsorber(ChillNum).ChillSupplyNodeNum;
        int lCondReturnNodeNum = GasAbsorber(ChillNum).CondReturnNodeNum;
        int lCondSupplyNodeNum = GasAbsorber(ChillNum).CondSupplyNodeNum;

        if (MyLoad == 0 || !RunFlag) {
            DataLoopNode::Node(lChillSupplyNodeNum).Temp = DataLoopNode::Node(lChillReturnNodeNum).Temp;
            if (GasAbsorber(ChillNum).isWaterCooled) {
                DataLoopNode::Node(lCondSupplyNodeNum).Temp = DataLoopNode::Node(lCondReturnNodeNum).Temp;
            }
        } else {
            DataLoopNode::Node(lChillSupplyNodeNum).Temp = GasAbsorberReport(ChillNum).ChillSupplyTemp;
            if (GasAbsorber(ChillNum).isWaterCooled) {
                DataLoopNode::Node(lCondSupplyNodeNum).Temp = GasAbsorberReport(ChillNum).CondSupplyTemp;
            }
        }

        // convert power to energy and instantaneous use to use over the time step
        GasAbsorberReport(ChillNum).CoolingEnergy = GasAbsorberReport(ChillNum).CoolingLoad * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        GasAbsorberReport(ChillNum).TowerEnergy = GasAbsorberReport(ChillNum).TowerLoad * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        GasAbsorberReport(ChillNum).FuelEnergy = GasAbsorberReport(ChillNum).FuelUseRate * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        GasAbsorberReport(ChillNum).CoolFuelEnergy = GasAbsorberReport(ChillNum).CoolFuelUseRate * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        GasAbsorberReport(ChillNum).ElectricEnergy = GasAbsorberReport(ChillNum).ElectricPower * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        GasAbsorberReport(ChillNum).CoolElectricEnergy = GasAbsorberReport(ChillNum).CoolElectricPower * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        if (GasAbsorberReport(ChillNum).CoolFuelUseRate != 0.0) {
            GasAbsorberReport(ChillNum).FuelCOP = GasAbsorberReport(ChillNum).CoolingLoad / GasAbsorberReport(ChillNum).CoolFuelUseRate;
        } else {
            GasAbsorberReport(ChillNum).FuelCOP = 0.0;
        }
    }

    void UpdateGasAbsorberHeatRecords(Real64 const MyLoad, // current load
                                      bool const RunFlag,  // TRUE if Absorber operating
                                      int const ChillNum   // Absorber number
    )
    {
        //       AUTHOR         Jason Glazer
        //       DATE WRITTEN   March 2001

        int lHeatReturnNodeNum = GasAbsorber(ChillNum).HeatReturnNodeNum;
        int lHeatSupplyNodeNum = GasAbsorber(ChillNum).HeatSupplyNodeNum;

        if (MyLoad == 0 || !RunFlag) {
            DataLoopNode::Node(lHeatSupplyNodeNum).Temp = DataLoopNode::Node(lHeatReturnNodeNum).Temp;
        } else {
            DataLoopNode::Node(lHeatSupplyNodeNum).Temp = GasAbsorberReport(ChillNum).HotWaterSupplyTemp;
        }

        // convert power to energy and instantaneous use to use over the time step
        GasAbsorberReport(ChillNum).HeatingEnergy = GasAbsorberReport(ChillNum).HeatingLoad * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        GasAbsorberReport(ChillNum).FuelEnergy = GasAbsorberReport(ChillNum).FuelUseRate * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        GasAbsorberReport(ChillNum).HeatFuelEnergy = GasAbsorberReport(ChillNum).HeatFuelUseRate * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        GasAbsorberReport(ChillNum).ElectricEnergy = GasAbsorberReport(ChillNum).ElectricPower * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        GasAbsorberReport(ChillNum).HeatElectricEnergy = GasAbsorberReport(ChillNum).HeatElectricPower * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
    }

    void clear_state()
    {
        NumGasAbsorbers = 0;
        CheckEquipName.deallocate();
        GasAbsorber.deallocate();
        GasAbsorberReport.deallocate();
        Sim_HeatCap = 0.0;
        Sim_GetInput = true;
        Get_ErrorsFound = false;
        Init_MyOneTimeFlag = true;
        Init_MyEnvrnFlag.deallocate();
        Init_MyPlantScanFlag.deallocate();
        Calc_oldCondSupplyTemp = 0.0;
    }

} // namespace ChillerGasAbsorption

} // namespace EnergyPlus
