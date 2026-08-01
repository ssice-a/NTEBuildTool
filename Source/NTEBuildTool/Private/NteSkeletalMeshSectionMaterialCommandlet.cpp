// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteSkeletalMeshSectionMaterialCommandlet.h"

#include "NTEBuildTool.h"
#include "NteEditorAssetUtils.h"

#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Rendering/SkeletalMeshModel.h"
#include "SkeletalMeshTypes.h"
#include "UObject/SavePackage.h"

namespace
{
bool SaveAssetPackage(UObject& Asset)
{
	UPackage* Package = Asset.GetPackage();
	if (!Package)
	{
		return false;
	}

	Package->MarkPackageDirty();
	Asset.MarkPackageDirty();
	const FString Filename = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(Package, &Asset, *Filename, SaveArgs);
}
}

UNteSkeletalMeshSectionMaterialCommandlet::UNteSkeletalMeshSectionMaterialCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
	UseCommandletResultAsExitCode = true;
	HelpDescription = TEXT("Assigns SkeletalMesh LOD section material slots without changing the mesh material array.");
	HelpUsage = TEXT("UnrealEditor-Cmd.exe <Project>.uproject -run=NteSkeletalMeshSectionMaterial -Mesh=/Game/Mesh -LOD=0 -Slots=0,1,2");
}

int32 UNteSkeletalMeshSectionMaterialCommandlet::Main(const FString& Params)
{
	FString MeshPath;
	FString SlotText;
	int32 LODIndex = 0;
	FParse::Value(*Params, TEXT("Mesh="), MeshPath);
	FParse::Value(*Params, TEXT("Slots="), SlotText, false);
	FParse::Value(*Params, TEXT("LOD="), LODIndex);
	const bool bVerifyOnly = FParse::Param(*Params, TEXT("VerifyOnly"));
	MeshPath = NTEBuildTool::Editor::NormalizeAssetPathForText(MeshPath);

	USkeletalMesh* Mesh = NTEBuildTool::Editor::LoadAssetByPath<USkeletalMesh>(MeshPath);
	if (!Mesh || SlotText.IsEmpty())
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Mesh and Slots are required."));
		return 1;
	}

	TArray<FString> SlotParts;
	SlotText.ParseIntoArray(SlotParts, TEXT(","), true);
	TArray<int32> TargetSlots;
	for (const FString& SlotPart : SlotParts)
	{
		TargetSlots.Add(FCString::Atoi(*SlotPart));
	}

	FSkeletalMeshModel* ImportedModel = Mesh->GetImportedModel();
	FSkeletalMeshLODInfo* LODInfo = Mesh->GetLODInfo(LODIndex);
	if (!ImportedModel || !ImportedModel->LODModels.IsValidIndex(LODIndex) || !LODInfo)
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Mesh %s has no editable LOD %d."), *MeshPath, LODIndex);
		return 2;
	}

	const TArray<FSkelMeshSection>& Sections = ImportedModel->LODModels[LODIndex].Sections;
	if (TargetSlots.Num() != Sections.Num())
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Slots count %d does not match LOD %d section count %d."), TargetSlots.Num(), LODIndex, Sections.Num());
		return 3;
	}
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		if (!Mesh->GetMaterials().IsValidIndex(TargetSlots[SectionIndex]))
		{
			UE_LOG(LogNTEBuildTool, Error, TEXT("Material slot %d is invalid for section %d."), TargetSlots[SectionIndex], SectionIndex);
			return 4;
		}
	}

	if (!bVerifyOnly)
	{
		Mesh->Modify();
		{
			FScopedSkeletalMeshPostEditChange ScopedPostEditChange(Mesh);
			LODInfo->LODMaterialMap.Reset();
			LODInfo->LODMaterialMap.Reserve(Sections.Num());
			for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
			{
				const int32 SourceSlot = Sections[SectionIndex].MaterialIndex;
				const int32 TargetSlot = TargetSlots[SectionIndex];
				LODInfo->LODMaterialMap.Add(SourceSlot == TargetSlot ? INDEX_NONE : TargetSlot);
			}
		}

		if (!SaveAssetPackage(*Mesh))
		{
			UE_LOG(LogNTEBuildTool, Error, TEXT("Failed to save %s."), *MeshPath);
			return 5;
		}
	}

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		const int32 OverrideSlot = LODInfo->LODMaterialMap.IsValidIndex(SectionIndex)
			? LODInfo->LODMaterialMap[SectionIndex]
			: INDEX_NONE;
		const int32 ResolvedSlot = OverrideSlot == INDEX_NONE
			? Sections[SectionIndex].MaterialIndex
			: OverrideSlot;
		UE_LOG(LogNTEBuildTool, Display, TEXT("NTE_SECTION_SLOT|section=%d|slot=%d"), SectionIndex, ResolvedSlot);
		if (ResolvedSlot != TargetSlots[SectionIndex])
		{
			UE_LOG(LogNTEBuildTool, Error, TEXT("Section %d resolved to %d instead of %d."), SectionIndex, ResolvedSlot, TargetSlots[SectionIndex]);
			return 6;
		}
	}

	UE_LOG(LogNTEBuildTool, Display, TEXT("NTE_SECTION_SLOT_REPAIR=%s"), bVerifyOnly ? TEXT("VERIFIED") : TEXT("PASS"));
	return 0;
}
