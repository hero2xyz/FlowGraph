// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Nodes/Graph/FlowNode_SubGraph.h"

#include "FlowAsset.h"
#include "FlowLogChannels.h"
#include "FlowSettings.h"
#include "FlowSubsystem.h"
#include "Nodes/Graph/FlowNode_DefineProperties.h"
#include "Nodes/Graph/FlowNode_Finish.h"
#include "Nodes/Graph/FlowNode_Start.h"
#include "Nodes/Graph/FlowNode_CustomInput.h"
#include "Nodes/Graph/FlowNode_CustomOutput.h"
#include "Types/FlowPropertyUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlowNode_SubGraph)

#define LOCTEXT_NAMESPACE "FlowNode_SubGraph"

FFlowPin UFlowNode_SubGraph::StartPin(TEXT("Start"));
FFlowPin UFlowNode_SubGraph::FinishPin(TEXT("Finish"));

UFlowNode_SubGraph::UFlowNode_SubGraph(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer), bCanInstanceIdenticalAsset(false)
{
#if WITH_EDITOR
	Category = TEXT("Graph");
	NodeDisplayStyle = FlowNodeStyle::SubGraph;

	AllowedAssignedAssetClasses = {UFlowAsset::StaticClass()};
#endif

	InputPins = {StartPin};
	OutputPins = {FinishPin};
}

UFlowNode_SubGraph::~UFlowNode_SubGraph()
{
#if WITH_EDITOR
	if (Asset)
	{
		Asset->OnSubGraphReconstructionRequested.Remove(OnSubGraphReconstructionRequestedHandle);
	}
#endif
}

bool UFlowNode_SubGraph::CanBeAssetInstanced() const
{
	const UFlowAsset* OwningTemplate = GetFlowAsset() ? GetFlowAsset()->GetTemplateAsset() : nullptr;
	return !Asset.IsNull() && (bCanInstanceIdenticalAsset || !OwningTemplate || Asset.ToString() != GetPathNameSafe(OwningTemplate));
}

void UFlowNode_SubGraph::CachePinProperties()
{
	InputPropertyCache = PropertyBagUtils::GetCachedPropertiesFromBag(InputDataBag);
	OutputPropertyCache = PropertyBagUtils::GetCachedPropertiesFromBag(OutputDataBag);
}

void UFlowNode_SubGraph::PreloadContent()
{
	if (CanBeAssetInstanced())
	{
		if (UFlowSubsystem* FlowSys = GetFlowSubsystem())
		{
			// Dummy flag: bNeedsLoadingOutput is not used during preload
			bool bDummyNeedsLoading = false;
			if (UFlowAsset* Instance = FlowSys->GetOrCreateSubFlowInstance(this, FString(), /*out*/ bDummyNeedsLoading))
			{
				Instance->PreloadNodes();
			}
			else
			{
				LogError(FString::Printf(TEXT("PreloadContent: Failed to get or create instance for SubGraph %s."), *GetNameSafe(this)));
			}
		}
	}
}

void UFlowNode_SubGraph::FlushContent()
{
	if (CanBeAssetInstanced() && GetFlowSubsystem())
	{
		GetFlowSubsystem()->RemoveSubFlow(this, EFlowFinishPolicy::Abort);
	}
}
void UFlowNode_SubGraph::ExecuteInput(const FName& PinName)
{
    if (!CanBeAssetInstanced())
    {
       LogError(Asset.IsNull() ? TEXT("Missing Flow Asset reference.") : TEXT("Recursive subgraph instancing disallowed or asset invalid."));
       Finish();
       return;
    }

    if (!PrepareInputs())
    {
       LogError(FString::Printf(TEXT("SubGraph %s failed to prepare its inputs from parent graph. Aborting trigger for pin %s."), *GetNameSafe(this), *PinName.ToString()), EFlowOnScreenMessageType::Permanent);
       return; // Don't Finish() - allow retry on future triggers
    }
    
    UFlowSubsystem* FlowSys = GetFlowSubsystem();
    if (!FlowSys)
    {
       LogError(TEXT("Flow Subsystem not available."), EFlowOnScreenMessageType::Permanent);
       Finish();
       return;
    }

    bool bNeedsLoading = false;
    UFlowAsset* SubGraphInstance = FlowSys->GetOrCreateSubFlowInstance(this, SavedAssetInstanceName, /*out*/ bNeedsLoading);
    SavedAssetInstanceName.Empty();
    
    if (!SubGraphInstance)
    {
       LogError(FString::Printf(TEXT("SubGraph %s failed to create or find sub-asset instance '%s'."), *GetNameSafe(this), *Asset.ToString()), EFlowOnScreenMessageType::Permanent);
       Finish();
       return;
    }

    SubGraphInstance->NodeOwningThisAssetInstance = this;
    if (bNeedsLoading)
    {
       // Loaded instances resume from the saved state - no data transfer or activation needed
       FlowSys->LoadFlowInstanceFromSaveData(SubGraphInstance, SubGraphInstance->GetName());
       return;
    }

    // Find the entry point node for this input pin
    UFlowNode_DefineProperties* InternalEntryPointNode;
    const bool bIsStartPin = (PinName == StartPin.PinName);
    
    if (bIsStartPin)
    {
       InternalEntryPointNode = Cast<UFlowNode_DefineProperties>(SubGraphInstance->GetDefaultEntryNode());
       if (!InternalEntryPointNode)
       {
          LogWarning(FString::Printf(TEXT("SubGraph %s: Could not find Start node (or it doesn't derive from DefineProperties) in asset %s."), *GetNameSafe(this), *Asset.ToString()));
          return;
       }
    }
    else
    {
       InternalEntryPointNode = Cast<UFlowNode_DefineProperties>(SubGraphInstance->TryFindCustomInputNodeByEventName(PinName));
       if (!InternalEntryPointNode)
       {
          LogWarning(FString::Printf(TEXT("SubGraph %s: Could not find Custom Input node (or it doesn't derive from DefineProperties) for event %s in asset %s."), *GetNameSafe(this), *PinName.ToString(), *Asset.ToString()));
          return;
       }
    }

    // Transfer input data from parent to subgraph entry node
    const FInstancedPropertyBag& SourceBag = this->InputDataBag;
    FInstancedPropertyBag& TargetBag = InternalEntryPointNode->GetOutputProperties();
    if (SourceBag.IsValid() && TargetBag.IsValid())
    {
       const bool bAllowImplicitConversion = UFlowSettings::Get()->bAllowImplicitConversion;
       PropertyBagUtils::CopyMatchingProperties(SourceBag, TargetBag, bAllowImplicitConversion, /*bCreateMissing=*/false);
    }

    // Trigger subgraph execution
    if (bIsStartPin)
    {
       FlowSys->StartSubFlowInstance(SubGraphInstance);
    }
    else
    {
       FlowSys->TriggerSubFlowCustomInput(SubGraphInstance, PinName);
    }

    // @note: Node completes when the subgraph calls NotifySubGraphOutput, not here
}

void UFlowNode_SubGraph::Cleanup()
{
	if (CanBeAssetInstanced() && GetFlowSubsystem())
	{
		GetFlowSubsystem()->RemoveSubFlow(this, EFlowFinishPolicy::Keep);
	}

	Super::Cleanup();
}

void UFlowNode_SubGraph::ForceFinishNode()
{
	TriggerFirstOutput(true);
}

bool UFlowNode_SubGraph::IsSupportedInputPinName(const FName& PinName) const
{
	if (Super::IsSupportedInputPinName(PinName))
	{
		return true;
	}

	// Validate against custom inputs from the asset template (instances may not be ready yet)
	const UFlowAsset* ReferencedAssetTemplate = Asset.Get();
	if (!ReferencedAssetTemplate)
	{
		ReferencedAssetTemplate = Asset.LoadSynchronous();
	}

	if (ReferencedAssetTemplate)
	{
		const TArray<FName> CustomInputNames = ReferencedAssetTemplate->GatherCustomInputNodeEventNames();
		if (CustomInputNames.Contains(PinName))
		{
			return true;
		}
	}

	return false;
}

void* UFlowNode_SubGraph::GetPropertyContainer(const FProperty* Property) const
{
	if (void* BagContainer = PropertyBagUtils::FindPropertyBagMemoryForProperty(Property, {&InputDataBag, &OutputDataBag}); BagContainer != nullptr)
	{
		return BagContainer;
	}

	return Super::GetPropertyContainer(Property);
}

void UFlowNode_SubGraph::NotifySubGraphOutput(UFlowNode* InternalNode, const FName OutputExecPinName)
{
	const UFlowNode_DefineProperties* InternalInterfaceNode = Cast<UFlowNode_DefineProperties>(InternalNode);
	if (!InternalInterfaceNode)
	{
		LogError(FString::Printf(TEXT("NotifySubGraphOutput: Internal node %s is not a DefineProperties derivative."), *InternalNode->GetName()));
		return;
	}

	const FInstancedPropertyBag& SourceBag = InternalInterfaceNode->GetInputProperties();
	FInstancedPropertyBag& TargetBag = this->OutputDataBag;
	if (SourceBag.IsValid() && TargetBag.IsValid())
	{
		const bool bAllowImplicitConversion = UFlowSettings::Get()->bAllowImplicitConversion;
		PropertyBagUtils::CopyMatchingProperties(SourceBag, TargetBag, bAllowImplicitConversion, /*bCreateMissing=*/false);
	}
	
	const bool bShouldFinishNode = (OutputExecPinName == FinishPin.PinName);
	TriggerOutput(OutputExecPinName, bShouldFinishNode);
}

void UFlowNode_SubGraph::OnLoad_Implementation()
{
	// Actual loading is deferred until the first ExecuteInput call via GetOrCreateSubFlowInstance.
	// SavedAssetInstanceName persists until then to trigger proper instance restoration.
    
	UE_LOG(LogFlow, Log, TEXT("SubGraph %s: OnLoad called. SavedAssetInstanceName = %s. Loading deferred until first execution trigger."), 
		   *GetNameSafe(this), *SavedAssetInstanceName);
}

#if WITH_EDITOR

FText UFlowNode_SubGraph::GetNodeTitle() const
{
	if (UFlowSettings::Get()->bUseAdaptiveNodeTitles && !Asset.IsNull())
	{
		return FText::Format(LOCTEXT("SubGraphTitle", "{0}\n{1}"), {Super::GetNodeTitle(), FText::FromString(Asset.ToSoftObjectPath().GetAssetName())});
	}

	return Super::GetNodeTitle();
}

FString UFlowNode_SubGraph::GetNodeDescription() const
{
	if (!UFlowSettings::Get()->bUseAdaptiveNodeTitles && !Asset.IsNull())
	{
		return Asset.ToSoftObjectPath().GetAssetName();
	}

	return Super::GetNodeDescription();
}

UObject* UFlowNode_SubGraph::GetAssetToEdit()
{
	return Asset.IsNull() ? nullptr : Asset.LoadSynchronous();
}

EDataValidationResult UFlowNode_SubGraph::ValidateNode()
{
	EDataValidationResult Result = Super::ValidateNode();
	bool bSelfConfigurationValid = true;
	
	if (Asset.IsNull())
	{
		ValidationLog.Error<UFlowNode>(*LOCTEXT("SubGraph_AssetNotAssigned", "Flow Asset not assigned.").ToString(), this);
		bSelfConfigurationValid = false;
	}

	const UFlowAsset* OwningTemplate = GetFlowAsset() ? GetFlowAsset()->GetTemplateAsset() : nullptr;
	if (OwningTemplate && !bCanInstanceIdenticalAsset && Asset.ToSoftObjectPath() == OwningTemplate->GetPathName())
	{
		ValidationLog.Error<UFlowNode>(*LOCTEXT("SubGraph_RecursiveIdenticalNotAllowed", "Recursive instancing of the same asset is disallowed by 'bCanInstanceIdenticalAsset' setting.").ToString(), this);
		bSelfConfigurationValid = false;
	}
	
	if (!bSelfConfigurationValid)
	{
		return EDataValidationResult::Invalid;
	}

	if (!Asset.IsNull())
	{
		UFlowAsset* ReferencedAsset = Asset.LoadSynchronous();
		if (!IsValid(ReferencedAsset))
		{
			ValidationLog.Error<UFlowNode>(
			    *FText::Format(LOCTEXT("SubGraph_AssetCannotBeLoaded_Error", "Assigned Flow Asset '{0}' could not be loaded. It might be missing or corrupted."),
			        FText::FromString(Asset.ToSoftObjectPath().ToString()))
			        .ToString(),
			    this);
			return EDataValidationResult::Invalid;
		}
	}

	return Result;
}

void UFlowNode_SubGraph::OnGraphLoaded()
{
	SubscribeToAssetChanges();
}

void UFlowNode_SubGraph::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UFlowNode_SubGraph, Asset))
	{
		if (Asset)
		{
			Asset->OnSubGraphReconstructionRequested.Remove(OnSubGraphReconstructionRequestedHandle);
			OnSubGraphReconstructionRequestedHandle.Reset();
		}
	}
}

void UFlowNode_SubGraph::Clear()
{
	InputDataBag.Reset();
	OutputDataBag.Reset();
}

void UFlowNode_SubGraph::HarvestDataPins()
{
	Modify();
	const UFlowAsset* FlowAssetObj = Asset.IsNull() ? nullptr : Asset.LoadSynchronous();
	
	TArray<FPropertyBagPropertyDesc> RequiredInputDescs;
	TArray<FPropertyBagPropertyDesc> RequiredOutputDescs;
	
	if (IsValid(FlowAssetObj))
	{
		TSet<FName> SeenInputPinNames;
		TSet<FName> SeenOutputPinNames;

		for (const auto& Pair : ObjectPtrDecay(FlowAssetObj->GetNodes()))
		{
			const UFlowNode* Node = Pair.Value;
			if (!IsValid(Node))
			{
				continue;
			}

			const UFlowNode_DefineProperties* DefinePropsNode = Cast<const UFlowNode_DefineProperties>(Node);
			if (!DefinePropsNode)
			{
				continue;
			}

			const bool bIsInputSource = Node->IsA<UFlowNode_Start>() || Node->IsA<UFlowNode_CustomInput>();
			const bool bIsOutputSource = Node->IsA<UFlowNode_Finish>() || Node->IsA<UFlowNode_CustomOutput>();
			if (bIsInputSource)
			{
				// SubGraph inputs come from Start/CustomInput output properties
				const FInstancedPropertyBag& NodeOutputBag = DefinePropsNode->GetOutputProperties();
				if (NodeOutputBag.IsValid() && NodeOutputBag.GetPropertyBagStruct())
				{
					for (const FPropertyBagPropertyDesc& Desc : NodeOutputBag.GetPropertyBagStruct()->GetPropertyDescs())
					{
						if (Desc.ValueType != EPropertyBagPropertyType::None && !Desc.Name.IsNone() && !SeenInputPinNames.Contains(Desc.Name))
						{
							RequiredInputDescs.Add(Desc);
							SeenInputPinNames.Add(Desc.Name);
						}
					}
				}
			}
			else if (bIsOutputSource)
			{
				// SubGraph outputs come from Finish/CustomOutput input properties
				const FInstancedPropertyBag& NodeInputBag = DefinePropsNode->GetInputProperties();
				if (NodeInputBag.IsValid() && NodeInputBag.GetPropertyBagStruct())
				{
					for (const FPropertyBagPropertyDesc& Desc : NodeInputBag.GetPropertyBagStruct()->GetPropertyDescs())
					{
						if (Desc.ValueType != EPropertyBagPropertyType::None && !Desc.Name.IsNone() && !SeenOutputPinNames.Contains(Desc.Name))
						{
							RequiredOutputDescs.Add(Desc);
							SeenOutputPinNames.Add(Desc.Name);
						}
					}
				}
			}
		}
	}
	else
	{
		UE_LOG(LogFlow, Warning, TEXT("SubGraph %s: Invalid asset %s!"), *GetNameSafe(this), *Asset.ToString());
	}

	const bool bInputChanged = PropertyBagUtils::SyncBagStructureWithDescriptors(InputDataBag, RequiredInputDescs);
	const bool bOutputChanged = PropertyBagUtils::SyncBagStructureWithDescriptors(OutputDataBag, RequiredOutputDescs);
	if (bInputChanged || bOutputChanged)
	{
		CachePinProperties();
		OnReconstructionRequested.ExecuteIfBound();
	}
}

TArray<FFlowPin> UFlowNode_SubGraph::GetContextInputs() const
{
	TArray<FFlowPin> ContextInputPins = Super::GetContextInputs();

	if (!Asset.IsNull())
	{
		(void)Asset.LoadSynchronous();
		if (Asset.IsValid())
		{
			for (const FName& PinName : Asset->GetCustomInputs())
			{
				if (!PinName.IsNone())
				{
					ContextInputPins.AddUnique(FFlowPin(PinName));
				}
			}
		}
	}

	return ContextInputPins;
}

TArray<FFlowPin> UFlowNode_SubGraph::GetContextOutputs() const
{
	TArray<FFlowPin> ContextOutputPins = Super::GetContextOutputs();

	if (!Asset.IsNull())
	{
		(void)Asset.LoadSynchronous();
		if (Asset.IsValid())
		{
			for (const FName& PinName : Asset->GetCustomOutputs())
			{
				if (!PinName.IsNone())
				{
					ContextOutputPins.AddUnique(FFlowPin(PinName));
				}
			}
		}
	}

	return ContextOutputPins;
}

void UFlowNode_SubGraph::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.GetPropertyName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UFlowNode_SubGraph, Asset))
	{
		InputDataBag.Reset();
		OutputDataBag.Reset();

		OnReconstructionRequested.ExecuteIfBound();
		SubscribeToAssetChanges();
		HarvestDataPins();
	}
}

void UFlowNode_SubGraph::SubscribeToAssetChanges()
{
	if (Asset)
	{
		OnSubGraphReconstructionRequestedHandle = Asset->OnSubGraphReconstructionRequested.AddUObject(this, &UFlowNode_SubGraph::OnAssetParametersChanged);
	}
}

void UFlowNode_SubGraph::OnAssetParametersChanged()
{
	if (this)
	{
		OnReconstructionRequested.ExecuteIfBound();
		HarvestDataPins();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
