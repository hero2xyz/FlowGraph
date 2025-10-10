// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "UnrealExtensions/FlowNodeBlueprintCompilerContext.h"
#include "Nodes/FlowNodeBlueprint.h"
#include "DetailCustomizations/FlowNodeBlueprint_VariableDetailsCustomization.h"
#include "KismetCompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetReinstanceUtilities.h"

FFlowNodeBlueprintCompilerContext::FFlowNodeBlueprintCompilerContext(UBlueprint* InBlueprint, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions)
    : FKismetCompilerContext(InBlueprint, InMessageLog, InCompilerOptions)
{
}

void FFlowNodeBlueprintCompilerContext::OnNewClassSet(UBlueprintGeneratedClass* ClassToUse)
{
	FKismetCompilerContext::OnNewClassSet(ClassToUse);
	if (UFlowNodeBaseBlueprintGeneratedClass* FlowClass = Cast<UFlowNodeBaseBlueprintGeneratedClass>(ClassToUse))
	{
		ProcessBlueprintVariables(FlowClass);
	}
}

void FFlowNodeBlueprintCompilerContext::SpawnNewClass(const FString& NewClassName)
{
	UFlowNodeBaseBlueprintGeneratedClass* NewBlueprintClass = FindObject<UFlowNodeBaseBlueprintGeneratedClass>(Blueprint->GetOutermost(), *NewClassName);
	if (NewBlueprintClass == nullptr)
	{
		NewBlueprintClass = NewObject<UFlowNodeBaseBlueprintGeneratedClass>(Blueprint->GetOutermost(), FName(*NewClassName), RF_Public | RF_Transactional);
	}
	else
	{
		// Already existed, but wasn't linked for some reason
		FBlueprintCompileReinstancer::Create(NewBlueprintClass);
		NewBlueprintClass->FlowVarSettings.Empty();
	}

	NewBlueprintClass->ClassGeneratedBy = Blueprint;
	NewBlueprintClass->ClassFlags |= (CLASS_CompiledFromBlueprint);
	NewClass = NewBlueprintClass;
}

void FFlowNodeBlueprintCompilerContext::EnsureProperGeneratedClass(UClass*& InTargetClass)
{
	if (InTargetClass && !static_cast<UObject*>(InTargetClass)->IsA(UFlowNodeBaseBlueprintGeneratedClass::StaticClass()))
	{
		FKismetCompilerUtilities::ConsignToOblivion(InTargetClass, Blueprint->bIsRegeneratingOnLoad);
		InTargetClass = nullptr;
	}
}

bool FFlowNodeBlueprintCompilerContext::ValidateGeneratedClass(UBlueprintGeneratedClass* Class)
{
	return FKismetCompilerContext::ValidateGeneratedClass(Class);
}

void FFlowNodeBlueprintCompilerContext::PreCompile()
{
	FKismetCompilerContext::PreCompile();
}

void FFlowNodeBlueprintCompilerContext::ProcessBlueprintVariables(UFlowNodeBaseBlueprintGeneratedClass* FlowClass) const
{
	FlowClass->FlowVarSettings.Empty();
	for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
	{
		FFlowVarConfig VarConfig;

		// Check behavior metadata (None/Input/Output)
		FString BehaviorValue;
		FBlueprintEditorUtils::GetBlueprintVariableMetaData(Blueprint, VarDesc.VarName, nullptr, FFlowNodeBlueprint_VariableDetailsCustomization::MetadataKeyEnum, BehaviorValue);
		if (BehaviorValue.Equals(TEXT("Input"), ESearchCase::IgnoreCase))
		{
			VarConfig.bIsDataPin = true;
			VarConfig.bIsInput = true;
		}
		else if (BehaviorValue.Equals(TEXT("Output"), ESearchCase::IgnoreCase))
		{
			VarConfig.bIsDataPin = true;
			VarConfig.bIsInput = false;
		}

		// Add to class settings if it has any Flow configuration
		if (VarConfig.bIsDataPin)
		{
			FlowClass->FlowVarSettings.Add(VarDesc.VarName, VarConfig);
		}
	}
}

void RegisterFlowNodeBlueprintCompiler()
{
	auto CreateCompilerContext = [](UBlueprint* BP, FCompilerResultsLog& InResultLog, const FKismetCompilerOptions& InCompileOptions) {
		return MakeShared<FFlowNodeBlueprintCompilerContext>(BP, InResultLog, InCompileOptions);
	};

	// @note We're specifically focusing on UFlowNode rather than UFlowNodeBase
	// to ensure that derived classes like UFlowNodeAddOn, which don't fully support
	// this functionality, are excluded from being treated as data sources.
	// FKismetCompilerContext::RegisterCompilerForBP(UFlowNodeBaseBlueprint::StaticClass(), CreateCompilerContext);
	// FKismetCompilerContext::RegisterCompilerForBP(UFlowNodeAddOnBlueprint::StaticClass(), CreateCompilerContext);
	FKismetCompilerContext::RegisterCompilerForBP(UFlowNodeBlueprint::StaticClass(), CreateCompilerContext);
}
