#include "SWTypes.h"
#include "SWTypes/SonarPulse.h"
#include "SWTypes/UnitDelivery.h"
#include "SWTypes/GenericWarhead.h"
#include "SWTypes/Firewall.h"
#include "SWTypes/Protect.h"
#include "SWTypes/Reveal.h"
#include "SWTypes/ParaDrop.h"
#include "SWTypes/SpyPlane.h"
#include "SWTypes/ChronoSphere.h"
#include "SWTypes/ChronoWarp.h"
#include "SWTypes/GeneticMutator.h"
#include "SWTypes/Dominator.h"
#include "SWTypes/LightningStorm.h"
#include "SWTypes/Nuke.h"
#include "SWTypes/HunterSeeker.h"
#include "SWTypes/DropPod.h"
#include "SWTypes/EMPulse.h"

#include "../Ext/Building/Body.h"
#include "../Ext/TechnoType/Body.h"

#include <BuildingClass.h>
#include <HouseClass.h>

#include "SavegameDef.h"

#include <algorithm>

#pragma region TargetingData definitions

TargetingData::TargetingData(SWTypeExt::ExtData* pTypeExt, HouseClass* pOwner)
	: TypeExt(pTypeExt), Owner(pOwner)
{ }

TargetingData::~TargetingData() = default;

#pragma endregion

std::vector<std::unique_ptr<NewSWType>> NewSWType::Array;
std::vector<std::unique_ptr<SWStateMachine>> SWStateMachine::Array;

void NewSWType::Init()
{
	if(!Array.empty()) {
		return;
	}

	Register(std::make_unique<SW_SonarPulse>());
	Register(std::make_unique<SW_UnitDelivery>());
	Register(std::make_unique<SW_GenericWarhead>());
	Register(std::make_unique<SW_Firewall>());
	Register(std::make_unique<SW_Protect>());
	Register(std::make_unique<SW_Reveal>());
	Register(std::make_unique<SW_ParaDrop>());
	Register(std::make_unique<SW_SpyPlane>());
	Register(std::make_unique<SW_ChronoSphere>());
	Register(std::make_unique<SW_ChronoWarp>());
	Register(std::make_unique<SW_GeneticMutator>());
	Register(std::make_unique<SW_PsychicDominator>());
	Register(std::make_unique<SW_LightningStorm>());
	Register(std::make_unique<SW_NuclearMissile>());
	Register(std::make_unique<SW_HunterSeeker>());
	Register(std::make_unique<SW_DropPod>());
	Register(std::make_unique<SW_EMPulse>());
}

std::unique_ptr<const TargetingData> NewSWType::GetTargetingData(
	SWTypeExt::ExtData* const pSWType, HouseClass* const pOwner) const
{
	return std::make_unique<const TargetingData>(pSWType, pOwner);
}

bool NewSWType::CanFireAt(
	SWTypeExt::ExtData* const pSWType, HouseClass* const pOwner,
	const CellStruct& cell, bool manual) const
{
	auto const data = this->GetTargetingData(pSWType, pOwner);
	return this->CanFireAt(*data, cell, manual);
}

bool NewSWType::CanFireAt(TargetingData const& data, const CellStruct& cell, bool manual) const {
	return data.TypeExt->CanFireAt(data.Owner, cell, manual)
		&& HasLaunchSite(data.TypeExt, data.Owner, cell)
		&& HasDesignator(data.TypeExt, data.Owner, cell)
		&& HasInhibitor(data.TypeExt, data.Owner, cell) == false;
}

bool NewSWType::IsLaunchSite(SWTypeExt::ExtData* pSWType, BuildingClass* pBuilding) const
{
	if(pBuilding->IsAlive && pBuilding->Health && !pBuilding->InLimbo && pBuilding->IsPowerOnline()) {
		auto const pExt = BuildingExt::ExtMap.Find(pBuilding);
		return pExt->HasSuperWeapon(pSWType->OwnerObject()->ArrayIndex, true);
	}

	return false;
}

std::pair<double, double> NewSWType::GetLaunchSiteRange(SWTypeExt::ExtData* pSWType, BuildingClass* pBuilding) const
{
	return std::make_pair(pSWType->SW_RangeMinimum, pSWType->SW_RangeMaximum);
}

bool NewSWType::HasLaunchSite(SWTypeExt::ExtData* pSWType, HouseClass* pOwner, const CellStruct &Coords) const
{
	// the quick way out: no range restriction at all
	auto range = GetLaunchSiteRange(pSWType);

	if(range.first < 0.0 && range.second < 0.0) {
		return true;
	}

	return std::any_of(pOwner->Buildings.begin(), pOwner->Buildings.end(), [&](BuildingClass* pBld) {
		return IsLaunchSiteEligible(pSWType, Coords, pBld, false);
	});
}

bool NewSWType::IsLaunchSiteEligible(SWTypeExt::ExtData* pSWType, const CellStruct &Coords, BuildingClass* pBuilding, bool ignoreRange) const
{
	if(!IsLaunchSite(pSWType, pBuilding)) {
		return false;
	}

	if(ignoreRange) {
		return true;
	}

	// get the range for this building
	auto range = GetLaunchSiteRange(pSWType, pBuilding);
	const auto& minRange = range.first;
	const auto& maxRange = range.second;

	const auto center = CellClass::Coord2Cell(BuildingExt::GetCenterCoords(pBuilding));
	const auto distance = Coords.DistanceFrom(center);

	// negative range values just pass the test
	return (minRange < 0.0 || distance >= minRange)
		&& (maxRange < 0.0 || distance <= maxRange);
}

bool NewSWType::IsDesignator(SWTypeExt::ExtData* pSWType, HouseClass* pOwner, TechnoClass* pTechno) const
{
	if(pTechno->IsAlive && pTechno->Health && !pTechno->InLimbo && !pTechno->Deactivated) {
		if(pTechno->GetOwningHouse() == pOwner) {
			return pSWType->SW_AnyDesignator
				|| pSWType->SW_Designators.Contains(pTechno->GetTechnoType());
		}
	}

	return false;
}

bool NewSWType::HasDesignator(SWTypeExt::ExtData* pSWType, HouseClass* pOwner, const CellStruct &Coords) const
{
	// does not require designators
	if(pSWType->SW_Designators.empty() && !pSWType->SW_AnyDesignator) {
		return true;
	}

	// a single designator in range suffices
	return std::any_of(TechnoClass::Array->begin(), TechnoClass::Array->end(), [&](TechnoClass* pTechno) {
		return IsDesignatorEligible(pSWType, pOwner, Coords, pTechno);
	});
}

bool NewSWType::IsDesignatorEligible(SWTypeExt::ExtData* pSWType, HouseClass* pOwner, const CellStruct &Coords, TechnoClass* pTechno) const {
	if(IsDesignator(pSWType, pOwner, pTechno)) {
		const auto pType = pTechno->GetTechnoType();
		const auto pExt = TechnoTypeExt::ExtMap.Find(pType);

		// get the designator's center
		auto center = pTechno->GetCoords();
		if(auto pBuilding = abstract_cast<BuildingClass*>(pTechno)) {
			center = BuildingExt::GetCenterCoords(pBuilding);
		}

		// has to be closer than the designator range (which defaults to Sight)
		auto distance = Coords.DistanceFrom(CellClass::Coord2Cell(center));
		return distance <= pExt->DesignatorRange.Get(pType->Sight);
	}

	return false;
}

bool NewSWType::IsInhibitor(SWTypeExt::ExtData* pSWType, HouseClass* pOwner, TechnoClass* pTechno) const
{
	if(pTechno->IsAlive && pTechno->Health && !pTechno->InLimbo && !pTechno->Deactivated) {
		if(!pOwner->IsAlliedWith(pTechno)) {
			if(auto pBld = abstract_cast<BuildingClass*>(pTechno)) {
				if(!pBld->IsPowerOnline()) {
					return false;
				}
			}

			return pSWType->SW_AnyInhibitor
				|| pSWType->SW_Inhibitors.Contains(pTechno->GetTechnoType());
		}
	}

	return false;
}

bool NewSWType::HasInhibitor(SWTypeExt::ExtData* pSWType, HouseClass* pOwner, const CellStruct &Coords) const
{
	// does not allow inhibitors
	if(pSWType->SW_Inhibitors.empty() && !pSWType->SW_AnyInhibitor) {
		return false;
	}

	// a single inhibitor in range suffices
	return std::any_of(TechnoClass::Array->begin(), TechnoClass::Array->end(), [&](TechnoClass* pTechno) {
		return IsInhibitorEligible(pSWType, pOwner, Coords, pTechno);
	});
}

bool NewSWType::IsInhibitorEligible(SWTypeExt::ExtData* pSWType, HouseClass* pOwner, const CellStruct &Coords, TechnoClass* pTechno) const {
	if(IsInhibitor(pSWType, pOwner, pTechno)) {
		const auto pType = pTechno->GetTechnoType();
		const auto pExt = TechnoTypeExt::ExtMap.Find(pType);

		// get the inhibitor's center
		auto center = pTechno->GetCoords();
		if(auto pBuilding = abstract_cast<BuildingClass*>(pTechno)) {
			center = BuildingExt::GetCenterCoords(pBuilding);
		}

		// has to be closer than the inhibitor range (which defaults to Sight)
		auto distance = Coords.DistanceFrom(CellClass::Coord2Cell(center));
		return distance <= pExt->InhibitorRange.Get(pType->Sight);
	}

	return false;
}

SuperWeaponType NewSWType::FindIndex(const char* pType) {
	auto it = std::find_if(Array.begin(), Array.end(), [pType](const std::unique_ptr<NewSWType> &item) {
		const char* pID = item->GetTypeString();
		return pID && !strcmp(pID, pType);
	});

	if(it != Array.end()) {
		return static_cast<SuperWeaponType>(FIRST_SW_TYPE + std::distance(Array.begin(), it));
	}

	return SuperWeaponType::Invalid;
}

SuperWeaponType NewSWType::FindHandler(SuperWeaponType Type) {
	auto it = std::find_if(Array.begin(), Array.end(), [Type](const std::unique_ptr<NewSWType> &item) {
		return item->HandlesType(Type);
	});

	if(it != Array.end()) {
		return static_cast<SuperWeaponType>(FIRST_SW_TYPE + std::distance(Array.begin(), it));
	}

	return SuperWeaponType::Invalid;
}

bool NewSWType::LoadGlobals(AresStreamReader& Stm)
{
	// load the old type pointers, and map by position in vector
	// this enables swizzling NewSWType*
	for(const auto& ptr : Array) {
		Stm.RegisterChange(ptr.get());
	}

	return Stm.Success();
}

bool NewSWType::SaveGlobals(AresStreamWriter& Stm)
{
	// remember all current type pointers
	for(const auto& ptr : Array) {
		Stm.Save(ptr.get());
	}

	return Stm.Success();
}

void SWStateMachine::UpdateAll()
{
	for(auto& Machine : SWStateMachine::Array) {
		Machine->Update();
	}

	Array.erase(std::remove_if(Array.begin(), Array.end(), [](const std::unique_ptr<SWStateMachine> &ptr) {
		return ptr->Finished();
	}), Array.end());
}

void SWStateMachine::PointerGotInvalid(void *ptr, bool remove)
{
	for(auto& Machine : SWStateMachine::Array) {
		Machine->InvalidatePointer(ptr, remove);
	}
}

void SWStateMachine::Clear()
{
	// hard-reset any super weapon related globals
	SW_LightningStorm::CurrentLightningStorm = nullptr;
	SW_NuclearMissile::CurrentNukeType = nullptr;
	SW_PsychicDominator::CurrentPsyDom = nullptr;

	SWStateMachine::Array.clear();
}

bool SWStateMachine::Load(AresStreamReader &Stm, bool RegisterForChange) {
	return Stm
		.Process(this->Clock)
		.Process(this->Super, RegisterForChange)
		.Process(this->Type, RegisterForChange)
		.Process(this->Coords)
		.Success();
}

bool SWStateMachine::Save(AresStreamWriter &Stm) const {
	// used to instantiate in ObjectFactory
	Stm.Save(this->GetIdentifier());

	return Stm
		.Process(this->Clock)
		.Process(this->Super)
		.Process(this->Type)
		.Process(this->Coords)
		.Success();
}

bool SWStateMachine::LoadGlobals(AresStreamReader& Stm)
{
	return Stm
		.Process(Array)
		.Process(SW_LightningStorm::CurrentLightningStorm)
		.Process(SW_NuclearMissile::CurrentNukeType)
		.Process(SW_PsychicDominator::CurrentPsyDom)
		.Success();
}

bool SWStateMachine::SaveGlobals(AresStreamWriter& Stm)
{
	return Stm
		.Process(Array)
		.Process(SW_LightningStorm::CurrentLightningStorm)
		.Process(SW_NuclearMissile::CurrentNukeType)
		.Process(SW_PsychicDominator::CurrentPsyDom)
		.Success();
}

DEFINE_HOOK(55AFB3, LogicClass_Update, 6)
{
	SWStateMachine::UpdateAll();
	Ares::UpdateStability();
	return 0;
}
