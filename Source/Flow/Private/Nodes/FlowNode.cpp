// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Nodes/FlowNode.h"
#include "AddOns/FlowNodeAddOn.h"

#include "FlowAsset.h"
#include "FlowSettings.h"
#include "Components/ActorComponent.h"
#if WITH_EDITOR
	#include "Editor.h"
#endif

#include "FlowLogChannels.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Misc/App.h"
#include "Nodes/FlowNodeBlueprint.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Types/FlowPropertyUtils.h"

FFlowPin UFlowNode::DefaultInputPin(TEXT("In"));
FFlowPin UFlowNode::DefaultOutputPin(TEXT("Out"));

FString UFlowNode::MissingIdentityTag = TEXT("Missing Identity Tag");
FString UFlowNode::MissingNotifyTag = TEXT("Missing Notify Tag");
FString UFlowNode::MissingClass = TEXT("Missing class");
FString UFlowNode::NoActorsFound = TEXT("No actors found");

UFlowNode::UFlowNode(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer), AllowedSignalModes({EFlowSignalMode::Enabled, EFlowSignalMode::Disabled, EFlowSignalMode::PassThrough}), SignalMode(EFlowSignalMode::Enabled), bPreloaded(false), ActivationState(EFlowNodeState::NeverActivated)
{
#if WITH_EDITOR
	Category = TEXT("Uncategorized");
	NodeDisplayStyle = FlowNodeStyle::Default;
#endif

	InputPins = {DefaultInputPin};
	OutputPins = {DefaultOutputPin};
}

#if WITH_EDITOR

void UFlowNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	OnPropertyChangedEvent.ExecuteIfBound(PropertyChangedEvent.Property);
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName MemberPropertyName = PropertyChangedEvent.GetMemberPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UFlowNode, InputPins) || PropertyName == GET_MEMBER_NAME_CHECKED(UFlowNode, OutputPins)
	    || MemberPropertyName == GET_MEMBER_NAME_CHECKED(UFlowNode, InputPins) || MemberPropertyName == GET_MEMBER_NAME_CHECKED(UFlowNode, OutputPins))
	{
		// Potentially need to rebuild the pins from the this node
		OnReconstructionRequested.ExecuteIfBound();
	}
}

void UFlowNode::PostLoad()
{
	Super::PostLoad();

	// fix Class Default Object
	FixNode(nullptr);
}

void UFlowNode::OnPropertyChanged(FName PropertyName)
{
	UpdateNodeConfigText();
	RequestReconstruction();
}

#endif

void UFlowNode::InitializeInstance()
{
	Super::InitializeInstance();
	CachePinProperties();
}

bool UFlowNode::IsSupportedInputPinName(const FName& PinName) const
{
	const FFlowPin* InputPin = FindFlowPinByName(PinName, InputPins);

	if (AddOns.IsEmpty())
	{
		checkf(InputPin, TEXT("Only AddOns should introduce unknown Pins to a FlowNode, so if we have no AddOns, we should have no unknown pins"));
		return true;
	}

	return (InputPin != nullptr);
}

void UFlowNode::AddInputPins(const TArray<FFlowPin>& Pins)
{
	for (const FFlowPin& Pin : Pins)
	{
		InputPins.AddUnique(Pin);
	}
}

void UFlowNode::AddOutputPins(const TArray<FFlowPin>& Pins)
{
	for (const FFlowPin& Pin : Pins)
	{
		OutputPins.AddUnique(Pin);
	}
}

#if WITH_EDITOR

bool UFlowNode::RebuildPinArray(const TArray<FName>& NewPinNames, TArray<FFlowPin>& InOutPins, const FFlowPin& DefaultPin)
{
	bool bIsChanged;

	TArray<FFlowPin> NewPins;

	if (NewPinNames.Num() == 0)
	{
		bIsChanged = true;

		NewPins.Reserve(1);

		NewPins.Add(DefaultPin);
	}
	else
	{
		const bool bIsSameNum = (NewPinNames.Num() == InOutPins.Num());

		bIsChanged = !bIsSameNum;

		NewPins.Reserve(NewPinNames.Num());

		for (int32 NewPinIndex = 0; NewPinIndex < NewPinNames.Num(); ++NewPinIndex)
		{
			const FName& NewPinName = NewPinNames[NewPinIndex];
			NewPins.Add(FFlowPin(NewPinName));

			if (bIsSameNum)
			{
				bIsChanged = bIsChanged || (NewPinName != InOutPins[NewPinIndex].PinName);
			}
		}
	}

	if (bIsChanged)
	{
		InOutPins.Reset();

		check(NewPins.Num() > 0);

		if (&InOutPins == &InputPins)
		{
			AddInputPins(NewPins);
		}
		else
		{
			checkf(&InOutPins == &OutputPins, TEXT("Only expected to be called with one or the other of the pin arrays"));

			AddOutputPins(NewPins);
		}
	}

	return bIsChanged;
}

bool UFlowNode::RebuildPinArray(const TArray<FFlowPin>& NewPins, TArray<FFlowPin>& InOutPins, const FFlowPin& DefaultPin)
{
	TArray<FName> NewPinNames;
	NewPinNames.Reserve(NewPins.Num());

	for (const FFlowPin& NewPin : NewPins)
	{
		NewPinNames.Add(NewPin.PinName);
	}

	return RebuildPinArray(NewPinNames, InOutPins, DefaultPin);
}

#endif // WITH_EDITOR

void UFlowNode::SetNumberedInputPins(const uint8 FirstNumber, const uint8 LastNumber)
{
	InputPins.Empty();

	for (uint8 i = FirstNumber; i <= LastNumber; i++)
	{
		InputPins.Emplace(i);
	}
}

void UFlowNode::SetNumberedOutputPins(const uint8 FirstNumber /*= 0*/, const uint8 LastNumber /*= 1*/)
{
	OutputPins.Empty();

	for (uint8 i = FirstNumber; i <= LastNumber; i++)
	{
		OutputPins.Emplace(i);
	}
}

uint8 UFlowNode::CountNumberedInputs() const
{
	uint8 Result = 0;
	for (const FFlowPin& Pin : InputPins)
	{
		if (Pin.PinName.ToString().IsNumeric())
		{
			Result++;
		}
	}
	return Result;
}

uint8 UFlowNode::CountNumberedOutputs() const
{
	uint8 Result = 0;
	for (const FFlowPin& Pin : OutputPins)
	{
		if (Pin.PinName.ToString().IsNumeric())
		{
			Result++;
		}
	}
	return Result;
}

TArray<FName> UFlowNode::GetInputNames() const
{
	TArray<FName> Result;
	for (const FFlowPin& Pin : InputPins)
	{
		if (!Pin.PinName.IsNone())
		{
			Result.Emplace(Pin.PinName);
		}
	}
	return Result;
}

TArray<FName> UFlowNode::GetOutputNames() const
{
	TArray<FName> Result;
	for (const FFlowPin& Pin : OutputPins)
	{
		if (!Pin.PinName.IsNone())
		{
			Result.Emplace(Pin.PinName);
		}
	}
	return Result;
}

#if WITH_EDITOR

bool UFlowNode::SupportsContextPins() const
{
	if (Super::SupportsContextPins())
	{
		return true;
	}

	for (const UFlowNodeAddOn* AddOn : AddOns)
	{
		if (IsValid(AddOn) && AddOn->SupportsContextPins())
		{
			return true;
		}
	}

	return false;
}

TArray<FFlowPin> UFlowNode::GetContextInputs() const
{
	TArray<FFlowPin> ContextOutputs = Super::GetContextInputs();
	return ContextOutputs;
}

TArray<FFlowPin> UFlowNode::GetContextOutputs() const
{
	TArray<FFlowPin> ContextOutputs = Super::GetContextOutputs();
	return ContextOutputs;
}

bool UFlowNode::CanUserAddInput() const
{
	return K2_CanUserAddInput();
}

bool UFlowNode::CanUserAddOutput() const
{
	return K2_CanUserAddOutput();
}

void UFlowNode::RemoveUserInput(const FName& PinName)
{
	Modify();

	int32 RemovedPinIndex = INDEX_NONE;
	for (int32 i = 0; i < InputPins.Num(); i++)
	{
		if (InputPins[i].PinName == PinName)
		{
			InputPins.RemoveAt(i);
			RemovedPinIndex = i;
			break;
		}
	}

	// update remaining pins
	if (RemovedPinIndex > INDEX_NONE)
	{
		for (int32 i = RemovedPinIndex; i < InputPins.Num(); ++i)
		{
			if (InputPins[i].PinName.ToString().IsNumeric())
			{
				InputPins[i].PinName = *FString::FromInt(i);
			}
		}
	}
}

void UFlowNode::RemoveUserOutput(const FName& PinName)
{
	Modify();

	int32 RemovedPinIndex = INDEX_NONE;
	for (int32 i = 0; i < OutputPins.Num(); i++)
	{
		if (OutputPins[i].PinName == PinName)
		{
			OutputPins.RemoveAt(i);
			RemovedPinIndex = i;
			break;
		}
	}

	// update remaining pins
	if (RemovedPinIndex > INDEX_NONE)
	{
		for (int32 i = RemovedPinIndex; i < OutputPins.Num(); ++i)
		{
			if (OutputPins[i].PinName.ToString().IsNumeric())
			{
				OutputPins[i].PinName = *FString::FromInt(i);
			}
		}
	}
}

#endif // WITH_EDITOR

TOptional<FConnectedPin> UFlowNode::GetInputConnection(const FName InputPinName) const
{
	if (const FConnectedPin* FoundPin = InputConnections.Find(InputPinName))
	{
		return *FoundPin;
	}

	return TOptional<FConnectedPin>();
}

TArray<FConnectedPin> UFlowNode::GetOutputConnections(const FName OutputPinName) const
{
	if (const FPinConnectionList* ConnectionList = OutputConnections.Find(OutputPinName))
	{
		return ConnectionList->Connections;
	}

	return TArray<FConnectedPin>();
}

bool UFlowNode::IsInputConnected(const FName& PinName, bool bErrorIfPinNotFound /*= true*/)
{
	if (!FindInputPinByName(PinName))
	{
		if (bErrorIfPinNotFound)
		{
			LogError(FString::Printf(TEXT("Checked connection status for unknown Input Pin '%s'"), *PinName.ToString()), EFlowOnScreenMessageType::Temporary);
		}
		return false;
	}

	return InputConnections.Contains(PinName);
}

bool UFlowNode::IsOutputConnected(const FName& PinName, bool bErrorIfPinNotFound /*= true*/)
{
	if (!FindOutputPinByName(PinName))
	{
		if (bErrorIfPinNotFound)
		{
			LogError(FString::Printf(TEXT("Checked connection status for unknown Output Pin '%s'"), *PinName.ToString()), EFlowOnScreenMessageType::Temporary);
		}

		return false;
	}

	const FPinConnectionList* ConnectionList = OutputConnections.Find(PinName);
	return ConnectionList != nullptr && ConnectionList->Connections.Num() > 0;
}

FName UFlowNode::GetInputPinConnectedFromNode(const FGuid& SourceNodeGuid) const
{
	for (const TPair<FName, FConnectedPin>& Connection : InputConnections)
	{
		if (Connection.Value.NodeGuid == SourceNodeGuid)
		{
			return Connection.Key;
		}
	}
	return NAME_None;
}

TArray<FName> UFlowNode::GetOutputPinsConnectedToNode(const FGuid& TargetNodeGuid) const
{
	TArray<FName> ConnectedOutputPins;
	for (const TPair<FName, FPinConnectionList>& OutputConnectionPair : OutputConnections)
	{
		for (const FConnectedPin& Target : OutputConnectionPair.Value.Connections)
		{
			if (Target.NodeGuid == TargetNodeGuid)
			{
				ConnectedOutputPins.AddUnique(OutputConnectionPair.Key);
				// @note: Do not break here, multiple output pins might connect to the same target node.
			}
		}
	}
	return ConnectedOutputPins;
}

TSet<UFlowNode*> UFlowNode::GatherConnectedNodes() const
{
	TSet<UFlowNode*> Result;
	const UFlowAsset* Asset = GetFlowAsset();
	if (!Asset)
	{
		LogError(TEXT("Attempted GatherConnectedNodes with no valid FlowAsset."), EFlowOnScreenMessageType::Permanent);
		return Result;
	}

	for (const TPair<FName, FPinConnectionList>& OutputConnectionPair : OutputConnections)
	{
		for (const FConnectedPin& Target : OutputConnectionPair.Value.Connections)
		{
			if (UFlowNode* ConnectedNode = Asset->GetNode(Target.NodeGuid))
			{
				Result.Emplace(ConnectedNode);
			}
			else
			{
				LogError(FString::Printf(TEXT("GatherConnectedNodes found connection from pin '%s' to invalid NodeGuid '%s'."),
				             *OutputConnectionPair.Key.ToString(), *Target.NodeGuid.ToString()),
				    EFlowOnScreenMessageType::Temporary);
			}
		}
	}
	return Result;
}

FFlowPin* UFlowNode::FindInputPinByName(const FName& PinName)
{
	if (FFlowPin* FlowPin = FindFlowPinByName(PinName, InputPins))
	{
		return FlowPin;
	}

	return nullptr;
}

FFlowPin* UFlowNode::FindOutputPinByName(const FName& PinName)
{
	if (FFlowPin* FlowPin = FindFlowPinByName(PinName, OutputPins))
	{
		return FlowPin;
	}

	return nullptr;
}

void UFlowNode::RecursiveFindNodesByClass(UFlowNode* Node, const TSubclassOf<UFlowNode> Class, uint8 Depth, TArray<UFlowNode*>& OutNodes)
{
	if (Node)
	{
		// Record the node if it is the desired type
		if (Node->GetClass() == Class)
		{
			OutNodes.AddUnique(Node);
		}

		if (OutNodes.Num() == Depth)
		{
			return;
		}

		// Recurse
		for (UFlowNode* ConnectedNode : Node->GatherConnectedNodes())
		{
			RecursiveFindNodesByClass(ConnectedNode, Class, Depth, OutNodes);
		}
	}
}

bool UFlowNode::IsPureNode_Implementation() const
{
	return false;
}

void UFlowNode::CachePinProperties()
{
	InputPropertyCache.Empty();
	OutputPropertyCache.Empty();
	const UClass* NodeClass = GetClass();
	if (!NodeClass)
	{
		return;
	}

	// Helper lambda to process a property
	auto ProcessProperty = [&](FProperty* Property, const bool bIsInput) {
		if (!Property)
		{
			return;
		}
		FName PropertyName = Property->GetFName();
		if (bIsInput)
		{
			InputPropertyCache.Emplace(PropertyName, Property);
		}
		else
		{
			OutputPropertyCache.Emplace(PropertyName, Property);
		}
	};

	if (const UFlowNodeBaseBlueprintGeneratedClass* FlowBPClass = Cast<const UFlowNodeBaseBlueprintGeneratedClass>(NodeClass))
	{
		for (const TPair<FName, FFlowVarConfig>& VarPair : FlowBPClass->FlowVarSettings)
		{
			if (VarPair.Value.bIsDataPin)
			{
				if (FProperty* Property = FindFProperty<FProperty>(NodeClass, VarPair.Key))
				{
					ProcessProperty(Property, VarPair.Value.IsInputPin());
				}
			}
		}
	}
}

void* UFlowNode::GetPropertyContainer(const FProperty* Property) const
{
	return const_cast<UFlowNode*>(this);
}

bool UFlowNode::PrepareInputs()
{
	bool bOverallSuccess = true;
	const UFlowSettings* FlowSettings = GetDefault<UFlowSettings>();
	const bool bAllowImplicitConversion = FlowSettings ? FlowSettings->bAllowImplicitConversion : true;

	// Iterate through all known input connections for this node
	for (const TPair<FName, FConnectedPin>& InputConnPair : InputConnections)
	{
		const FName LocalInputPinName = InputConnPair.Key;
		const FConnectedPin& SourcePinInfo = InputConnPair.Value;

		// Find the corresponding FProperty for our input pin
		FProperty* TargetInputProperty = GetInputProperty(LocalInputPinName);
		if (!TargetInputProperty)
		{
			continue;
		}

		// Find the source node instance
		const UFlowAsset* OwningAsset = GetFlowAsset();
		if (!OwningAsset)
		{
			bOverallSuccess = false;
			break;
		}
		UFlowNode* SourceNode = OwningAsset->GetNode(SourcePinInfo.NodeGuid);
		if (!SourceNode)
		{
			LogError(FString::Printf(TEXT("PrepareInputs: Failed to find source node (GUID: %s) for input pin '%s' on node '%s'."), *SourcePinInfo.NodeGuid.ToString(), *LocalInputPinName.ToString(), *GetNameSafe(this)));
			bOverallSuccess = false;
			continue;
		}

		// Recursively evaluate the source node's output value
		const void* SourceDataPtr = nullptr;
		FProperty* SourceOutputProperty = nullptr;
		if (!SourceNode->EvaluateAndGetOutputValue(SourcePinInfo.PinName, /*out*/ SourceOutputProperty, /*out*/ SourceDataPtr))
		{
			LogError(FString::Printf(TEXT("PrepareInputs: Source node '%s' failed to evaluate output pin '%s' needed by node '%s' pin '%s'."), *SourceNode->GetName(), *SourcePinInfo.PinName.ToString(), *GetNameSafe(this), *LocalInputPinName.ToString()));
			bOverallSuccess = false;
			continue;
		}

		void* Container = GetPropertyContainer(TargetInputProperty);
		void* TargetDataPtr = TargetInputProperty->ContainerPtrToValuePtr<void>(Container);
		if (!SourceDataPtr || !TargetDataPtr || !SourceOutputProperty)
		{
			LogError(FString::Printf(TEXT("PrepareInputs: Invalid data pointers or source property for transfer from %s.%s to %s.%s."), *SourceNode->GetName(), *SourcePinInfo.PinName.ToString(), *GetNameSafe(this), *LocalInputPinName.ToString()));
			bOverallSuccess = false;
			continue;
		}

		// Perform a runtime compatibility check before transfer.
		// @note: While the editor schema aims to prevent incompatible connections,
		// this checks against potential issues from pin type changes without a break,
		// graph loading or programmatic modifications.
		if (!FlowPropertyUtils::ArePropertiesCompatible(SourceOutputProperty, TargetInputProperty, bAllowImplicitConversion))
		{
			LogError(FString::Printf(TEXT("PrepareInputs: Incompatible types for transfer from %s.%s (Type: %s) to %s.%s (Type: %s). Implicit conversion policy: %s."),
			    *SourceNode->GetName(), *SourcePinInfo.PinName.ToString(), *SourceOutputProperty->GetClass()->GetName(),
			    *GetNameSafe(this), *LocalInputPinName.ToString(), *TargetInputProperty->GetClass()->GetName(),
			    bAllowImplicitConversion ? TEXT("Allowed") : TEXT("Disallowed")));
			bOverallSuccess = false;
			continue;
		}

		if (FString TransferErrorMsg; !FlowPropertyUtils::PerformTransfer(SourceOutputProperty, SourceDataPtr, TargetInputProperty, TargetDataPtr, bAllowImplicitConversion, TransferErrorMsg))
		{
			LogError(FString::Printf(TEXT("PrepareInputs: Failed to transfer input data in node '%s'. Reason: %s"), *GetNameSafe(this), *TransferErrorMsg));
			bOverallSuccess = false;
		}
	}

	return bOverallSuccess;
}

bool UFlowNode::PerformPureCalculation_Implementation()
{
	// Base implementation does nothing and assumes success.
	// Derived pure nodes MUST override this to perform their logic,
	// reading input properties and writing to output properties.
	return true;
}

bool UFlowNode::EvaluateAndGetOutputValue(const FName OutputPinName, FProperty*& OutProperty, const void*& OutDataPtr)
{
	OutProperty = nullptr;
	OutDataPtr = nullptr;

	FProperty* OutputProperty = GetOutputProperty(OutputPinName);
	if (!OutputProperty)
	{
		LogError(FString::Printf(TEXT("EvaluateAndGetOutputValue: Output property cache miss for pin '%s' on node '%s'."), *OutputPinName.ToString(), *GetNameSafe(this)));
		return false;
	}
	OutProperty = OutputProperty;

	// If this node is pure, it needs to calculate its value now
	if (IsPureNode())
	{
		ActivationState = EFlowNodeState::Active;
		if (!PrepareInputs())
		{
			LogError(FString::Printf(TEXT("EvaluateAndGetOutputValue: Failed to prepare inputs for pure node '%s' calculation (output '%s')."), *GetNameSafe(this), *OutputPinName.ToString()));
			return false;
		}

		// Perform the node's specific pure logic
		if (!PerformPureCalculation())
		{
			LogError(FString::Printf(TEXT("EvaluateAndGetOutputValue: Pure calculation failed for node '%s' (output '%s')."), *GetNameSafe(this), *OutputPinName.ToString()));
			return false;
		}
		// @TODO Pure nodes don't really "complete" in the exec sense... Maybe we need a proper state for pure nodes?
		ActivationState = EFlowNodeState::Completed;
	}

	// @note For non-pure nodes, we assume the value in the property
	// is the correct one based on the previous execution flow.

	void* Container = GetPropertyContainer(OutputProperty);
	OutDataPtr = OutputProperty->ContainerPtrToValuePtr<void>(Container);
	if (!OutDataPtr)
	{
		LogError(FString::Printf(TEXT("EvaluateAndGetOutputValue: Failed to get data pointer for output property '%s' on node '%s'."), *OutputPinName.ToString(), *GetNameSafe(this)));
		return false;
	}

#if !UE_BUILD_SHIPPING
	RecordPinActivation(this, OutputPinName, EGPD_Output);
#endif

	return true;
}

FProperty* UFlowNode::GetInputProperty(const FName PinName) const
{
	return InputPropertyCache.FindRef(PinName);
}

FProperty* UFlowNode::GetOutputProperty(const FName PinName) const
{
	return OutputPropertyCache.FindRef(PinName);
}

void UFlowNode::TriggerPreload()
{
	bPreloaded = true;
	PreloadContent();
}

void UFlowNode::TriggerFlush()
{
	bPreloaded = false;
	FlushContent();
}

void UFlowNode::TriggerInput(const FName& PinName, const EFlowPinActivationType ActivationType /*= Default*/)
{
	if (!PrepareInputs())
	{
		LogError(FString::Printf(TEXT("Failed to prepare inputs for node %s when triggering pin %s. Aborting execution."), *GetNameSafe(this), *PinName.ToString()), EFlowOnScreenMessageType::Permanent);
		return;
	}

	if (SignalMode == EFlowSignalMode::Disabled)
	{
		return;
	}
	if (!IsSupportedInputPinName(PinName))
	{
#if !UE_BUILD_SHIPPING
		LogError(FString::Printf(TEXT("TriggerInput: Input Pin name %s is not supported by node %s or its addons"), *PinName.ToString(), *GetNameSafe(this)));
#endif
		return;
	}
	if (SignalMode == EFlowSignalMode::Enabled)
	{
		const EFlowNodeState PreviousActivationState = ActivationState;
		if (PreviousActivationState != EFlowNodeState::Active)
		{
			OnActivate();
		}
		ActivationState = EFlowNodeState::Active;
	}

#if !UE_BUILD_SHIPPING
	RecordPinActivation(this, PinName, EGPD_Input, ActivationType);
#endif

	switch (SignalMode)
	{
		case EFlowSignalMode::Enabled:
			ExecuteInputForSelfAndAddOns(PinName);
			break;
		case EFlowSignalMode::PassThrough:
			if (UFlowSettings::Get()->bLogOnSignalPassthrough)
			{
				LogNote(FString::Printf(TEXT("Signal pass-through on triggering input %s"), *PinName.ToString()));
			}
			OnPassThrough();
			break;
		default:;
	}
}

void UFlowNode::TriggerFirstOutput(const bool bFinish)
{
	if (OutputPins.Num() > 0)
	{
		TriggerOutput(OutputPins[0].PinName, bFinish);
	}
}

void UFlowNode::TriggerOutput(const FName PinName, const bool bFinish /*= false*/, const EFlowPinActivationType ActivationType /*= Default*/)
{
	if (ActivationState == EFlowNodeState::Completed || ActivationState == EFlowNodeState::Aborted)
	{
		LogError(TEXT("Trying to TriggerOutput after finished or aborted"));
		return;
	}

#if !UE_BUILD_SHIPPING
	RecordPinActivation(this, PinName, EGPD_Output, ActivationType);
#endif

	const FPinConnectionList* ConnectionList = OutputConnections.Find(PinName);
	if (ConnectionList && ConnectionList->Connections.Num() > 0)
	{
		if (UFlowAsset* OwningAsset = GetFlowAsset(); !OwningAsset)
		{
			LogError(TEXT("Cannot TriggerOutput, owning FlowAsset is invalid."), EFlowOnScreenMessageType::Permanent);
		}
		else
		{
#if WITH_EDITOR
			const FFlowPin* PinDefinition = FindOutputPinByName(PinName);
			const bool bIsDataPin = GetOutputProperty(PinName) != nullptr;
			const bool bIsExec = PinDefinition ? PinDefinition->IsExecPin() : !bIsDataPin;
			if (bIsExec && ConnectionList->Connections.Num() > 1)
			{
				LogWarning(FString::Printf(TEXT("Execution Pin '%s' has multiple connections (%d). Triggering all targets."), *PinName.ToString(), ConnectionList->Connections.Num()));
			}
#endif
			
			for (const FConnectedPin& TargetConnection : ConnectionList->Connections)
			{
				if (TargetConnection.IsValid())
				{
					OwningAsset->TriggerInput(TargetConnection.NodeGuid, TargetConnection.PinName);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogFlow, VeryVerbose, TEXT("TriggerOutput: Pin '%s' on node '%s' is not connected."), *PinName.ToString(), *GetNameSafe(this));
	}

	if (bFinish)
	{
		Finish();
	}
}

void UFlowNode::Finish()
{
	Deactivate();
	GetFlowAsset()->FinishNode(this);
}

void UFlowNode::Deactivate()
{
	if (SignalMode == EFlowSignalMode::PassThrough)
	{
		// there is nothing to deactivate, node was never active
		return;
	}

	if (GetFlowAsset()->FinishPolicy == EFlowFinishPolicy::Abort)
	{
		ActivationState = EFlowNodeState::Aborted;
	}
	else
	{
		ActivationState = EFlowNodeState::Completed;
	}

	Cleanup();
}

void UFlowNode::ResetRecords()
{
	ActivationState = EFlowNodeState::NeverActivated;

#if !UE_BUILD_SHIPPING
	InputRecords.Empty();
	OutputRecords.Empty();
#endif
}

void UFlowNode::SaveInstance(FFlowNodeSaveData& NodeRecord)
{
	NodeRecord.NodeGuid = NodeGuid;
	OnSave();

	FMemoryWriter MemoryWriter(NodeRecord.NodeData, true);
	FFlowArchive Ar(MemoryWriter);
	Serialize(Ar);
}

void UFlowNode::LoadInstance(const FFlowNodeSaveData& NodeRecord)
{
	FMemoryReader MemoryReader(NodeRecord.NodeData, true);
	FFlowArchive Ar(MemoryReader);
	Serialize(Ar);

	if (UFlowAsset* FlowAsset = GetFlowAsset())
	{
		FlowAsset->OnActivationStateLoaded(this);
	}

	switch (SignalMode)
	{
		case EFlowSignalMode::Enabled:
			OnLoad();
			break;
		case EFlowSignalMode::Disabled:
			// designer doesn't want to execute this node's logic at all, so we kill it
			LogNote(TEXT("Signal disabled while loading Flow Node from SaveGame"));
			Finish();
			break;
		case EFlowSignalMode::PassThrough:
			LogNote(TEXT("Signal pass-through on loading Flow Node from SaveGame"));
			OnPassThrough();
			break;
		default:;
	}
}

void UFlowNode::OnSave_Implementation()
{
}

void UFlowNode::OnLoad_Implementation()
{
}

void UFlowNode::OnPassThrough_Implementation()
{
	// trigger all connected outputs
	// pin connections aren't serialized to the SaveGame, so users can safely change connections post game release
	for (const FFlowPin& OutputPin : OutputPins)
	{
		if (OutputConnections.Contains(OutputPin.PinName))
		{
			TriggerOutput(OutputPin.PinName, false, EFlowPinActivationType::PassThrough);
		}
	}

	// deactivate node, so it doesn't get saved to a new SaveGame
	Finish();
}

#if WITH_EDITOR
TArray<FPinRecord> UFlowNode::GetPinRecords(const FName& PinName, const EEdGraphPinDirection PinDirection) const
{
	switch (PinDirection)
	{
		case EGPD_Input:
			return InputRecords.FindRef(PinName);
		case EGPD_Output:
			return OutputRecords.FindRef(PinName);
		default:
			return TArray<FPinRecord>();
	}
}

#endif

FString UFlowNode::GetIdentityTagDescription(const FGameplayTag& Tag)
{
	return Tag.IsValid() ? Tag.ToString() : MissingIdentityTag;
}

FString UFlowNode::GetIdentityTagsDescription(const FGameplayTagContainer& Tags)
{
	return Tags.IsEmpty() ? MissingIdentityTag : FString::JoinBy(Tags, LINE_TERMINATOR, [](const FGameplayTag& Tag) { return Tag.ToString(); });
}

FString UFlowNode::GetNotifyTagsDescription(const FGameplayTagContainer& Tags)
{
	return Tags.IsEmpty() ? MissingNotifyTag : FString::JoinBy(Tags, LINE_TERMINATOR, [](const FGameplayTag& Tag) { return Tag.ToString(); });
}

FString UFlowNode::GetClassDescription(const TSubclassOf<UObject> Class)
{
	return Class ? Class->GetName() : MissingClass;
}

FString UFlowNode::GetProgressAsString(const float Value)
{
	return FString::Printf(TEXT("%.*f"), 2, Value);
}


#if !UE_BUILD_SHIPPING
void UFlowNode::RecordPinActivation(const UFlowNode* Node, FName PinName, EEdGraphPinDirection PinDirection, EFlowPinActivationType ActivationType /*= EFlowPinActivationType::Default*/)
{
	if (!Node || PinName.IsNone())
	{
		return;
	}

	TMap<FName, TArray<FPinRecord>>* Records;
	if (PinDirection == EGPD_Input)
	{
		Records = const_cast<TMap<FName, TArray<FPinRecord>>*>(&Node->InputRecords);
	}
	else // EGPD_Output
	{
		Records = const_cast<TMap<FName, TArray<FPinRecord>>*>(&Node->OutputRecords);
	}

	if (Records)
	{
		TArray<FPinRecord>& PinRecords = Records->FindOrAdd(PinName);
		PinRecords.Add(FPinRecord(FApp::GetCurrentTime(), ActivationType));

		if (const UFlowAsset* FlowAssetTemplate = Node->GetFlowAsset()->GetTemplateAsset())
		{
			const_cast<UFlowAsset*>(FlowAssetTemplate)->OnPinTriggered.ExecuteIfBound(Node->GetGuid(), PinName);
		}
	}
}
#endif

#if WITH_EDITOR
UFlowNode* UFlowNode::GetInspectedInstance() const
{
	if (const UFlowAsset* FlowInstance = GetFlowAsset()->GetInspectedInstance())
	{
		return FlowInstance->GetNode(GetGuid());
	}

	return nullptr;
}

FString UFlowNode::GetStatusStringForNodeAndAddOns() const
{
	FString CombinedStatusString = GetStatusString();

	// Give all of the AddOns a chance to add their status strings as well
	(void)ForEachAddOnConst(
	    [&CombinedStatusString](const UFlowNodeAddOn& AddOn) {
		    const FString AddOnStatusString = AddOn.GetStatusString();

		    if (!AddOnStatusString.IsEmpty())
		    {
			    if (!CombinedStatusString.IsEmpty())
			    {
				    CombinedStatusString += TEXT("\n");
			    }

			    CombinedStatusString += AddOnStatusString;
		    }

		    return EFlowForEachAddOnFunctionReturnValue::Continue;
	    });

	return CombinedStatusString;
}

bool UFlowNode::GetStatusBackgroundColor(FLinearColor& OutColor) const
{
	return K2_GetStatusBackgroundColor(OutColor);
}

FString UFlowNode::GetAssetPath()
{
	return K2_GetAssetPath();
}

UObject* UFlowNode::GetAssetToEdit()
{
	return K2_GetAssetToEdit();
}

AActor* UFlowNode::GetActorToFocus()
{
	return K2_GetActorToFocus();
}
#endif
