// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "FlowAsset.h"

#include "FlowLogChannels.h"
#include "FlowSettings.h"
#include "FlowSubsystem.h"

#include "AddOns/FlowNodeAddOn.h"
#include "Nodes/FlowNodeBase.h"
#include "Nodes/Graph/FlowNode_CustomInput.h"
#include "Nodes/Graph/FlowNode_CustomOutput.h"
#include "Nodes/Graph/FlowNode_Start.h"
#include "Nodes/Graph/FlowNode_SubGraph.h"

#include "Engine/World.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_EDITOR
	#include "Editor.h"
	#include "Editor/EditorEngine.h"

FString UFlowAsset::ValidationError_NodeClassNotAllowed = TEXT("Node class {0} is not allowed in this asset.");
FString UFlowAsset::ValidationError_NullNodeInstance = TEXT("Node with GUID {0} is NULL");

namespace FlowHarvestHelpers
{
	bool AreConnectedPinMapsEqual(const TMap<FName, FConnectedPin>& MapA, const TMap<FName, FConnectedPin>& MapB)
	{
		if (MapA.Num() != MapB.Num())
		{
			return false;
		}
		for (const auto& PairA : MapA)
		{
			const FConnectedPin* ValueB = MapB.Find(PairA.Key);
			if (!ValueB || PairA.Value != *ValueB)
			{
				return false;
			}
		}
		return true;
	}

	// Helper to compare FPinConnectionList (ignoring order in the array)
	bool ArePinConnectionListsEqual(const FPinConnectionList* ListA, const FPinConnectionList* ListB)
	{
		if (!ListA && !ListB)
		{
			return true;
		}
		if (!ListA || !ListB)
		{
			return false;
		}
		if (ListA->Connections.Num() != ListB->Connections.Num())
		{
			return false;
		}

		for (const FConnectedPin& ConnA : ListA->Connections)
		{
			bool bFoundMatch = false;
			for (const FConnectedPin& ConnB : ListB->Connections)
			{
				if (ConnA == ConnB)
				{
					bFoundMatch = true;
					break;
				}
			}
			if (!bFoundMatch)
			{
				return false;
			}
		}
		return true;
	}
} // namespace FlowHarvestHelpers
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlowAsset)

UFlowAsset::UFlowAsset(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer), bWorldBound(true)
#if WITH_EDITORONLY_DATA
      ,
      FlowGraph(nullptr)
#endif
      ,
      AllowedNodeClasses({UFlowNodeBase::StaticClass()}), AllowedInSubgraphNodeClasses({UFlowNode_SubGraph::StaticClass()}), bStartNodePlacedAsGhostNode(false), TemplateAsset(nullptr), FinishPolicy(EFlowFinishPolicy::Keep)
{
	if (!AssetGuid.IsValid())
	{
		AssetGuid = FGuid::NewGuid();
	}

	ExpectedOwnerClass = UFlowSettings::Get()->GetDefaultExpectedOwnerClass();
}

#if WITH_EDITOR
void UFlowAsset::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UFlowAsset* This = CastChecked<UFlowAsset>(InThis);
	Collector.AddReferencedObject(This->FlowGraph, This);

	Super::AddReferencedObjects(InThis, Collector);
}

void UFlowAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UFlowAsset, CustomInputs) || PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UFlowAsset, CustomOutputs)))
	{
		OnSubGraphReconstructionRequested.Broadcast();
	}
}

void UFlowAsset::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bDuplicateForPIE)
	{
		AssetGuid = FGuid::NewGuid();
		Nodes.Empty();
	}
}

void UFlowAsset::PostLoad()
{
	Super::PostLoad();

	// If we removed or moved a flow node blueprint (and there is no redirector) we might loose the reference to it resulting
	// in null pointers in the Nodes FGUID->UFlowNode* Map. So here we iterate over all the Nodes and remove all pairs that
	// are nulled out.

	TSet<FGuid> NodesToRemoveGUID;

	for (auto& [Guid, Node] : GetNodes())
	{
		if (!IsValid(Node))
		{
			NodesToRemoveGUID.Emplace(Guid);
		}
	}

	for (const FGuid& Guid : NodesToRemoveGUID)
	{
		UnregisterNode(Guid);
	}
}

EDataValidationResult UFlowAsset::ValidateAsset(FFlowMessageLog& MessageLog)
{
	// validate nodes
	for (const TPair<FGuid, UFlowNode*>& Node : ObjectPtrDecay(Nodes))
	{
		if (IsValid(Node.Value))
		{
			FText FailureReason;
			if (!IsNodeOrAddOnClassAllowed(Node.Value->GetClass(), &FailureReason))
			{
				const FString ErrorMsg =
				    FailureReason.IsEmpty()
				    ? FString::Format(*ValidationError_NodeClassNotAllowed, {*Node.Value->GetClass()->GetName()})
				    : FailureReason.ToString();

				MessageLog.Error(*ErrorMsg, Node.Value);
			}

			Node.Value->ValidationLog.Messages.Empty();
			if (Node.Value->ValidateNode() == EDataValidationResult::Invalid)
			{
				MessageLog.Messages.Append(Node.Value->ValidationLog.Messages);
			}
		}
		else
		{
			const FString ErrorMsg = FString::Format(*ValidationError_NullNodeInstance, {*Node.Key.ToString()});
			MessageLog.Error(*ErrorMsg, this);
		}
	}

	return MessageLog.Messages.Num() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}

bool UFlowAsset::IsNodeOrAddOnClassAllowed(const UClass* FlowNodeOrAddOnClass, FText* OutOptionalFailureReason) const
{
	if (!IsValid(FlowNodeOrAddOnClass))
	{
		return false;
	}

	if (!CanFlowNodeClassBeUsedByFlowAsset(*FlowNodeOrAddOnClass))
	{
		return false;
	}

	if (!CanFlowAssetUseFlowNodeClass(*FlowNodeOrAddOnClass))
	{
		return false;
	}

	// Confirm plugin reference restrictions are being respected
	if (!CanFlowAssetReferenceFlowNode(*FlowNodeOrAddOnClass, OutOptionalFailureReason))
	{
		return false;
	}

	return true;
}

bool UFlowAsset::CanFlowNodeClassBeUsedByFlowAsset(const UClass& FlowNodeClass) const
{
	UFlowNode* NodeDefaults = Cast<UFlowNode>(FlowNodeClass.GetDefaultObject());
	if (!NodeDefaults)
	{
		check(FlowNodeClass.IsChildOf<UFlowNodeAddOn>());

		// AddOns don't have the AllowedAssetClasses/DeniedAssetClasses
		// (yet?  maybe we move it up to the base?)
		return true;
	}

	// UFlowNode class limits which UFlowAsset class can use it
	const TArray<TSubclassOf<UFlowAsset>>& DeniedAssetClasses = NodeDefaults->DeniedAssetClasses;
	for (const UClass* DeniedAssetClass : DeniedAssetClasses)
	{
		if (DeniedAssetClass && GetClass()->IsChildOf(DeniedAssetClass))
		{
			return false;
		}
	}

	const TArray<TSubclassOf<UFlowAsset>>& AllowedAssetClasses = NodeDefaults->AllowedAssetClasses;
	if (AllowedAssetClasses.Num() > 0)
	{
		bool bAllowedInAsset = false;
		for (const UClass* AllowedAssetClass : AllowedAssetClasses)
		{
			if (AllowedAssetClass && GetClass()->IsChildOf(AllowedAssetClass))
			{
				bAllowedInAsset = true;
				break;
			}
		}
		if (!bAllowedInAsset)
		{
			return false;
		}
	}

	return true;
}

bool UFlowAsset::CanFlowAssetUseFlowNodeClass(const UClass& FlowNodeClass) const
{
	// UFlowAsset class can limit which UFlowNodeBase classes can be used
	if (IsFlowNodeClassInDeniedClasses(FlowNodeClass))
	{
		return false;
	}

	if (!IsFlowNodeClassInAllowedClasses(FlowNodeClass))
	{
		return false;
	}

	return true;
}

bool UFlowAsset::IsFlowNodeClassInDeniedClasses(const UClass& FlowNodeClass) const
{
	for (const TSubclassOf<UFlowNodeBase>& DeniedNodeClass : DeniedNodeClasses)
	{
		if (DeniedNodeClass && FlowNodeClass.IsChildOf(DeniedNodeClass))
		{
			// Subclasses of a DeniedNodeClass can opt back in to being allowed
			if (!IsFlowNodeClassInAllowedClasses(FlowNodeClass, DeniedNodeClass))
			{
				return true;
			}
		}
	}

	return false;
}

bool UFlowAsset::IsFlowNodeClassInAllowedClasses(const UClass& FlowNodeClass,
    const TSubclassOf<UFlowNodeBase>& RequiredAncestor) const
{
	if (AllowedNodeClasses.Num() > 0)
	{
		bool bAllowedInAsset = false;
		for (const TSubclassOf<UFlowNodeBase>& AllowedNodeClass : AllowedNodeClasses)
		{
			// If a RequiredAncestor is provided, the AllowedNodeClass must be a subclass of the RequiredAncestor
			if (AllowedNodeClass && FlowNodeClass.IsChildOf(AllowedNodeClass) && (!RequiredAncestor || AllowedNodeClass->IsChildOf(RequiredAncestor)))
			{
				bAllowedInAsset = true;

				break;
			}
		}

		if (!bAllowedInAsset)
		{
			return false;
		}
	}

	return true;
}

bool UFlowAsset::CanFlowAssetReferenceFlowNode(const UClass& FlowNodeClass, FText* OutOptionalFailureReason) const
{
	if (!GEditor || !IsValid(&FlowNodeClass))
	{
		return false;
	}

	// Confirm plugin reference restrictions are being respected
	FAssetReferenceFilterContext AssetReferenceFilterContext;
	AssetReferenceFilterContext.AddReferencingAsset(FAssetData(this));
	const TSharedPtr<IAssetReferenceFilter> FlowAssetReferenceFilter = GEditor->MakeAssetReferenceFilter(AssetReferenceFilterContext);
	if (FlowAssetReferenceFilter.IsValid())
	{
		const FAssetData FlowNodeAssetData(&FlowNodeClass);
		if (!FlowAssetReferenceFilter->PassesFilter(FlowNodeAssetData, OutOptionalFailureReason))
		{
			return false;
		}
	}

	return true;
}

UFlowNode* UFlowAsset::CreateNode(const UClass* NodeClass, UEdGraphNode* GraphNode)
{
	UFlowNode* NewNode = NewObject<UFlowNode>(this, NodeClass, NAME_None, RF_Transactional);
	NewNode->SetGraphNode(GraphNode);

	RegisterNode(GraphNode->NodeGuid, NewNode);
	return NewNode;
}

void UFlowAsset::RegisterNode(const FGuid& NewGuid, UFlowNode* NewNode)
{
	NewNode->SetGuid(NewGuid);
	Nodes.Emplace(NewGuid, NewNode);

	HarvestNodeConnections();
}

void UFlowAsset::UnregisterNode(const FGuid& NodeGuid)
{
	Nodes.Remove(NodeGuid);
	Nodes.Compact();

	HarvestNodeConnections();

	MarkPackageDirty();
}

void UFlowAsset::GatherNodeConnections(UEdGraphNode* GraphNode, const UFlowAsset* OwningAsset, TMap<FName, FConnectedPin>& OutInputs, TMap<FName, FPinConnectionList>& OutOutputs)
{
	OutInputs.Empty();
	OutOutputs.Empty();

	for (const UEdGraphPin* Pin : GraphNode->Pins)
	{
		// Process output pins (connections FROM this node)
		if (Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
		{
			FName OutputPinName = Pin->PinName;
			auto& [Connections] = OutOutputs.FindOrAdd(OutputPinName);
			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (LinkedPin && LinkedPin->GetOwningNode())
				{
					const FGuid TargetNodeGuid = LinkedPin->GetOwningNode()->NodeGuid;
					const FName TargetPinName = LinkedPin->PinName;

					// Ensure the target node exists in the asset
					if (OwningAsset->GetNode(TargetNodeGuid))
					{
						Connections.AddUnique(FConnectedPin(TargetNodeGuid, TargetPinName));
					}
					else
					{
						UE_LOG(LogFlow, Warning, TEXT("Invalid target node '%s' for connection from '%s'"),
						    *TargetNodeGuid.ToString(), *GraphNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
					}
				}
			}
		}
		// Process input pins (connections TO this node)
		else if (Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() > 0)
		{
			const UEdGraphPin* SourcePin = Pin->LinkedTo[0];
			if (SourcePin && SourcePin->GetOwningNode())
			{
				const FGuid SourceNodeGuid = SourcePin->GetOwningNode()->NodeGuid;
				const FName SourcePinName = SourcePin->PinName;

				if (OwningAsset->GetNode(SourceNodeGuid))
				{
					OutInputs.Emplace(Pin->PinName, FConnectedPin(SourceNodeGuid, SourcePinName));
				}
				else
				{
					UE_LOG(LogFlow, Warning, TEXT("Invalid source node '%s' for connection to '%s'"),
					    *SourceNodeGuid.ToString(), *GraphNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
				}
			}
		}
	}
}

bool UFlowAsset::ApplyConnectionChanges(UFlowNode* RuntimeNode, const TMap<FName, FConnectedPin>& NewInputs, const TMap<FName, FPinConnectionList>& NewOutputs)
{
	bool bModified = false;
	bool bOutputsDiffer = false;
	if (RuntimeNode->OutputConnections.Num() != NewOutputs.Num())
	{
		bOutputsDiffer = true;
	}
	else
	{
		for (const auto& Pair : NewOutputs)
		{
			const FPinConnectionList* ExistingList = RuntimeNode->OutputConnections.Find(Pair.Key);
			if (!FlowHarvestHelpers::ArePinConnectionListsEqual(ExistingList, &Pair.Value))
			{
				bOutputsDiffer = true;
				break;
			}
		}
		if (!bOutputsDiffer)
		{
			for (const auto& Pair : RuntimeNode->OutputConnections)
			{
				if (!NewOutputs.Contains(Pair.Key))
				{
					bOutputsDiffer = true;
					break;
				}
			}
		}
	}

	if (bOutputsDiffer)
	{
		RuntimeNode->OutputConnections = NewOutputs;
		bModified = true;
	}

	if (!FlowHarvestHelpers::AreConnectedPinMapsEqual(RuntimeNode->InputConnections, NewInputs))
	{
		RuntimeNode->InputConnections = NewInputs;
		bModified = true;
	}
	
	if (bModified)
	{
		RuntimeNode->SetFlags(RF_Transactional);
		RuntimeNode->Modify();
	}

	return bModified;
}

void UFlowAsset::UpdateTargetNodesInputs(const UFlowAsset* OwningAsset, const FGuid& SourceNodeGuid, const TMap<FName, FPinConnectionList>& Outputs, TSet<UFlowNode*>& OutModifiedNodes)
{
	for (const auto& Pair : Outputs)
	{
		const FName SourcePinName = Pair.Key;
		for (const FConnectedPin& Target : Pair.Value.Connections)
		{
			UFlowNode* TargetNode = OwningAsset->GetNode(Target.NodeGuid);
			if (!TargetNode)
			{
				continue;
			}

			const FName TargetPinName = Target.PinName;
			FConnectedPin SourceInfo(SourceNodeGuid, SourcePinName);
			
			const FConnectedPin* ExistingInput = TargetNode->InputConnections.Find(TargetPinName);
			if (!ExistingInput || *ExistingInput != SourceInfo) // Needs update?
			{
				TargetNode->InputConnections.Emplace(TargetPinName, SourceInfo);
				TargetNode->SetFlags(RF_Transactional);
				TargetNode->Modify();
				OutModifiedNodes.Add(TargetNode);
			}
		}
	}
}

void UFlowAsset::HarvestNodeConnections(UFlowNode* TargetNode)
{
	TArray<UFlowNode*> TargetNodes;
	TSet<UFlowNode*> ModifiedNodes;

	if (IsValid(TargetNode))
	{
		TargetNodes.Reserve(1);
		TargetNodes.Add(TargetNode);
	}
	else
	{
		TargetNodes.Reserve(Nodes.Num());
		for (const TPair<FGuid, UFlowNode*>& Pair : ObjectPtrDecay(Nodes))
		{
			TargetNodes.Add(Pair.Value);
		}
	}

	// Remove any invalid nodes
	for (auto NodeIt = TargetNodes.CreateIterator(); NodeIt; ++NodeIt)
	{
		if (*NodeIt == nullptr)
		{
			NodeIt.RemoveCurrent();
			Modify();
		}
	}

	for (UFlowNode* FlowNode : TargetNodes)
	{
		TMap<FName, FConnectedPin> NewInputs;
		TMap<FName, FPinConnectionList> NewOutputs;
		GatherNodeConnections(FlowNode->GetGraphNode(), this, NewInputs, NewOutputs);
		
		if (ApplyConnectionChanges(FlowNode, NewInputs, NewOutputs))
		{
			ModifiedNodes.Add(FlowNode);
			UpdateTargetNodesInputs(this, FlowNode->GetGraphNode()->NodeGuid, NewOutputs, ModifiedNodes);
		}
	}

	if (ModifiedNodes.Num() > 0)
	{
		SetFlags(RF_Transactional);
		Modify();
		this->Modify();
		this->MarkPackageDirty();
	}
}

#endif

UFlowNode* UFlowAsset::GetDefaultEntryNode() const
{
	UFlowNode* FirstStartNode = nullptr;

	for (const TPair<FGuid, UFlowNode*>& Node : ObjectPtrDecay(Nodes))
	{
		if (UFlowNode_Start* StartNode = Cast<UFlowNode_Start>(Node.Value))
		{
			if (StartNode->GatherConnectedNodes().Num() > 0)
			{
				return StartNode;
			}
			else if (FirstStartNode == nullptr)
			{
				FirstStartNode = StartNode;
			}
		}
	}

	// If none of the found start nodes have connections, fallback to the first start node we found
	return FirstStartNode;
}

#if WITH_EDITOR
void UFlowAsset::AddCustomInput(const FName& EventName)
{
	if (!CustomInputs.Contains(EventName))
	{
		CustomInputs.Add(EventName);
	}
}

void UFlowAsset::RemoveCustomInput(const FName& EventName)
{
	if (CustomInputs.Contains(EventName))
	{
		CustomInputs.Remove(EventName);
	}
}

void UFlowAsset::AddCustomOutput(const FName& EventName)
{
	if (!CustomOutputs.Contains(EventName))
	{
		CustomOutputs.Add(EventName);
	}
}

void UFlowAsset::RemoveCustomOutput(const FName& EventName)
{
	if (CustomOutputs.Contains(EventName))
	{
		CustomOutputs.Remove(EventName);
	}
}
#endif // WITH_EDITOR

UFlowNode_CustomInput* UFlowAsset::TryFindCustomInputNodeByEventName(const FName& EventName) const
{
	for (const TPair<FGuid, UFlowNode*>& Node : ObjectPtrDecay(Nodes))
	{
		if (UFlowNode_CustomInput* CustomInput = Cast<UFlowNode_CustomInput>(Node.Value))
		{
			if (CustomInput->GetEventName() == EventName)
			{
				return CustomInput;
			}
		}
	}

	return nullptr;
}

UFlowNode_CustomOutput* UFlowAsset::TryFindCustomOutputNodeByEventName(const FName& EventName) const
{
	for (const TPair<FGuid, UFlowNode*>& Node : ObjectPtrDecay(Nodes))
	{
		if (UFlowNode_CustomOutput* CustomOutput = Cast<UFlowNode_CustomOutput>(Node.Value))
		{
			if (CustomOutput->GetEventName() == EventName)
			{
				return CustomOutput;
			}
		}
	}

	return nullptr;
}

TArray<FName> UFlowAsset::GatherCustomInputNodeEventNames() const
{
	// Runtime-safe gathering of the CustomInputs (which is editor-only data)
	//  from the actual flow nodes
	TArray<FName> Results;

	for (const TPair<FGuid, UFlowNode*>& Node : ObjectPtrDecay(Nodes))
	{
		if (UFlowNode_CustomInput* CustomInput = Cast<UFlowNode_CustomInput>(Node.Value))
		{
			Results.Add(CustomInput->GetEventName());
		}
	}

	return Results;
}

TArray<FName> UFlowAsset::GatherCustomOutputNodeEventNames() const
{
	// Runtime-safe gathering of the CustomOutputs (which is editor-only data)
	//  from the actual flow nodes
	TArray<FName> Results;

	for (const TPair<FGuid, UFlowNode*>& Node : ObjectPtrDecay(Nodes))
	{
		if (UFlowNode_CustomOutput* CustomOutput = Cast<UFlowNode_CustomOutput>(Node.Value))
		{
			Results.Add(CustomOutput->GetEventName());
		}
	}

	return Results;
}

TArray<UFlowNode*> UFlowAsset::GetNodesInExecutionOrder(UFlowNode* FirstIteratedNode, const TSubclassOf<UFlowNode> FlowNodeClass)
{
	TArray<UFlowNode*> FoundNodes;
	GetNodesInExecutionOrder<UFlowNode>(FirstIteratedNode, FoundNodes);

	// filter out nodes by class
	for (int32 i = FoundNodes.Num() - 1; i >= 0; i--)
	{
		if (!FoundNodes[i]->GetClass()->IsChildOf(FlowNodeClass))
		{
			FoundNodes.RemoveAt(i);
		}
	}
	FoundNodes.Shrink();

	return FoundNodes;
}

TArray<UFlowNode*> UFlowAsset::GatherNodesConnectedToAllInputs() const
{
	TSet<TObjectKey<UFlowNode>> IteratedNodes;
	TArray<UFlowNode*> ConnectedNodes;

	// Nodes connected to the Start node
	UFlowNode* DefaultEntryNode = GetDefaultEntryNode();
	GetNodesInExecutionOrder_Recursive(DefaultEntryNode, IteratedNodes, ConnectedNodes);

	// Nodes connected to Custom Input node(s)
	for (const TPair<FGuid, UFlowNode*>& Node : ObjectPtrDecay(Nodes))
	{
		if (UFlowNode_CustomInput* CustomInput = Cast<UFlowNode_CustomInput>(Node.Value))
		{
			GetNodesInExecutionOrder_Recursive(CustomInput, IteratedNodes, ConnectedNodes);
		}
	}

	return ConnectedNodes;
}

void UFlowAsset::AddInstance(UFlowAsset* Instance)
{
	ActiveInstances.Add(Instance);
}

int32 UFlowAsset::RemoveInstance(UFlowAsset* Instance)
{
#if WITH_EDITOR
	if (InspectedInstance.IsValid() && InspectedInstance.Get() == Instance)
	{
		SetInspectedInstance(NAME_None);
	}
#endif

	ActiveInstances.Remove(Instance);
	return ActiveInstances.Num();
}

void UFlowAsset::ClearInstances()
{
#if WITH_EDITOR
	if (InspectedInstance.IsValid())
	{
		SetInspectedInstance(NAME_None);
	}
#endif

	for (int32 i = ActiveInstances.Num() - 1; i >= 0; i--)
	{
		if (ActiveInstances.IsValidIndex(i) && ActiveInstances[i])
		{
			ActiveInstances[i]->FinishFlow(EFlowFinishPolicy::Keep);
		}
	}

	ActiveInstances.Empty();
}

#if WITH_EDITOR
void UFlowAsset::GetInstanceDisplayNames(TArray<TSharedPtr<FName>>& OutDisplayNames) const
{
	for (const UFlowAsset* Instance : ActiveInstances)
	{
		OutDisplayNames.Emplace(MakeShareable(new FName(Instance->GetDisplayName())));
	}
}

void UFlowAsset::SetInspectedInstance(const FName& NewInspectedInstanceName)
{
	if (NewInspectedInstanceName.IsNone())
	{
		InspectedInstance = nullptr;
	}
	else
	{
		for (UFlowAsset* ActiveInstance : ActiveInstances)
		{
			if (ActiveInstance && ActiveInstance->GetDisplayName() == NewInspectedInstanceName)
			{
				if (!InspectedInstance.IsValid() || InspectedInstance != ActiveInstance)
				{
					InspectedInstance = ActiveInstance;
				}
				break;
			}
		}
	}

	BroadcastDebuggerRefresh();
}

void UFlowAsset::BroadcastDebuggerRefresh() const
{
	RefreshDebuggerEvent.Broadcast();
}

void UFlowAsset::BroadcastRuntimeMessageAdded(const TSharedRef<FTokenizedMessage>& Message) const
{
	RuntimeMessageEvent.Broadcast(this, Message);
}
#endif // WITH_EDITOR

void UFlowAsset::InitializeInstance(const TWeakObjectPtr<UObject> InOwner, UFlowAsset& InTemplateAsset)
{
	check(!IsInstanceInitialized());

	Owner = InOwner;
	TemplateAsset = &InTemplateAsset;

	for (TPair<FGuid, TObjectPtr<UFlowNode>>& Node : Nodes)
	{
		UFlowNode* NewNodeInstance = NewObject<UFlowNode>(this, Node.Value->GetClass(), NAME_None, RF_Transient, Node.Value, false, nullptr);
		Node.Value = NewNodeInstance;

		if (UFlowNode_CustomInput* CustomInput = Cast<UFlowNode_CustomInput>(NewNodeInstance))
		{
			if (!CustomInput->EventName.IsNone())
			{
				CustomInputNodes.Emplace(CustomInput);
			}
		}

		NewNodeInstance->InitializeInstance();
	}
}

void UFlowAsset::DeinitializeInstance()
{
	if (IsInstanceInitialized())
	{
		for (const TPair<FGuid, UFlowNode*>& Node : ObjectPtrDecay(Nodes))
		{
			if (IsValid(Node.Value))
			{
				Node.Value->DeinitializeInstance();
			}
		}

		const int32 ActiveInstancesLeft = TemplateAsset->RemoveInstance(this);
		if (ActiveInstancesLeft == 0 && GetFlowSubsystem())
		{
			GetFlowSubsystem()->RemoveInstancedTemplate(TemplateAsset);
		}

		TemplateAsset = nullptr;
	}
}

void UFlowAsset::PreStartFlow()
{
	ResetNodes();

#if WITH_EDITOR
	check(IsInstanceInitialized());

	if (TemplateAsset->ActiveInstances.Num() == 1)
	{
		// this instance is the only active one, set it directly as Inspected Instance
		TemplateAsset->SetInspectedInstance(GetDisplayName());
	}
	else
	{
		// request to refresh list to show newly created instance
		TemplateAsset->BroadcastDebuggerRefresh();
	}
#endif
}

void UFlowAsset::StartFlow()
{
	PreStartFlow();

	if (UFlowNode* ConnectedEntryNode = GetDefaultEntryNode())
	{
		RecordedNodes.Add(ConnectedEntryNode);
		ConnectedEntryNode->TriggerFirstOutput(true);
	}
}

void UFlowAsset::FinishFlow(const EFlowFinishPolicy InFinishPolicy, const bool bRemoveInstance /*= true*/)
{
	FinishPolicy = InFinishPolicy;

	// end execution of this asset and all of its nodes
	for (UFlowNode* Node : ActiveNodes)
	{
		Node->Deactivate();
	}
	ActiveNodes.Empty();

	// flush preloaded content
	for (UFlowNode* PreloadedNode : PreloadedNodes)
	{
		PreloadedNode->TriggerFlush();
	}
	PreloadedNodes.Empty();

	// provides option to finish game-specific logic prior to removing asset instance
	if (bRemoveInstance)
	{
		DeinitializeInstance();
	}
}

bool UFlowAsset::HasStartedFlow() const
{
	return RecordedNodes.Num() > 0;
}

AActor* UFlowAsset::TryFindActorOwner() const
{
	const UActorComponent* OwnerAsComponent = Cast<UActorComponent>(GetOwner());
	if (IsValid(OwnerAsComponent))
	{
		return Cast<AActor>(OwnerAsComponent->GetOwner());
	}

	return nullptr;
}

TWeakObjectPtr<UFlowAsset> UFlowAsset::GetFlowInstance(UFlowNode_SubGraph* SubGraphNode) const
{
	return ActiveSubGraphs.FindRef(SubGraphNode);
}

void UFlowAsset::TriggerCustomInput_FromSubGraph(UFlowNode_SubGraph* SubGraphNode, const FName& EventName) const
{
	const TWeakObjectPtr<UFlowAsset> FlowInstance = ActiveSubGraphs.FindRef(SubGraphNode);
	if (FlowInstance.IsValid())
	{
		FlowInstance->TriggerCustomInput(EventName);
	}
}

void UFlowAsset::TriggerCustomInput(const FName& EventName)
{
	for (UFlowNode_CustomInput* CustomInputNode : CustomInputNodes)
	{
		if (CustomInputNode->EventName == EventName)
		{
			RecordedNodes.Add(CustomInputNode);
			CustomInputNode->ExecuteInput(EventName);
		}
	}
}

void UFlowAsset::TriggerCustomOutput(const FName& EventName)
{
	if (NodeOwningThisAssetInstance.IsValid())
	{
		// it's a SubGraph
		NodeOwningThisAssetInstance->TriggerOutput(EventName);
	}
	else
	{
		// it's a Root Flow, so the intention here might be to call event on the Flow Component
		if (UFlowComponent* FlowComponent = Cast<UFlowComponent>(GetOwner()))
		{
			FlowComponent->DispatchRootFlowCustomEvent(this, EventName);
		}
	}
}

void UFlowAsset::TriggerInput(const FGuid& NodeGuid, const FName& PinName)
{
	if (UFlowNode* Node = Nodes.FindRef(NodeGuid))
	{
		if (!ActiveNodes.Contains(Node))
		{
			ActiveNodes.Add(Node);
			RecordedNodes.Add(Node);
		}

		Node->TriggerInput(PinName);
	}
}

void UFlowAsset::FinishNode(UFlowNode* Node)
{
	if (ActiveNodes.Contains(Node))
	{
		ActiveNodes.Remove(Node);

		// if graph reached Finish and this asset instance was created by SubGraph node
		if (Node->CanFinishGraph())
		{
			if (NodeOwningThisAssetInstance.IsValid())
			{
				NodeOwningThisAssetInstance.Get()->TriggerFirstOutput(true);

				return;
			}

			// if this instance is a Root Flow, we need to deregister it from the subsystem first
			if (Owner.IsValid())
			{
				const TSet<UFlowAsset*>& RootFlowInstances = GetFlowSubsystem()->GetRootInstancesByOwner(Owner.Get());
				if (RootFlowInstances.Contains(this))
				{
					GetFlowSubsystem()->FinishRootFlow(Owner.Get(), TemplateAsset, EFlowFinishPolicy::Keep);

					return;
				}
			}

			FinishFlow(EFlowFinishPolicy::Keep);
		}
	}
}

void UFlowAsset::ResetNodes()
{
	for (UFlowNode* Node : RecordedNodes)
	{
		Node->ResetRecords();
	}

	RecordedNodes.Empty();
}

UFlowSubsystem* UFlowAsset::GetFlowSubsystem() const
{
	return Cast<UFlowSubsystem>(GetOuter());
}

FName UFlowAsset::GetDisplayName() const
{
	return GetFName();
}

UFlowNode_SubGraph* UFlowAsset::GetNodeOwningThisAssetInstance() const
{
	return NodeOwningThisAssetInstance.Get();
}

UFlowAsset* UFlowAsset::GetParentInstance() const
{
	return NodeOwningThisAssetInstance.IsValid() ? NodeOwningThisAssetInstance.Get()->GetFlowAsset() : nullptr;
}

FFlowAssetSaveData UFlowAsset::SaveInstance(TArray<FFlowAssetSaveData>& SavedFlowInstances)
{
	FFlowAssetSaveData AssetRecord;
	AssetRecord.WorldName = IsBoundToWorld() ? GetWorld()->GetName() : FString();
	AssetRecord.InstanceName = GetName();

	// opportunity to collect data before serializing asset
	OnSave();

	// iterate nodes
	TArray<UFlowNode*> NodesInExecutionOrder;
	GetNodesInExecutionOrder<UFlowNode>(GetDefaultEntryNode(), NodesInExecutionOrder);
	for (UFlowNode* Node : NodesInExecutionOrder)
	{
		if (Node && Node->ActivationState == EFlowNodeState::Active)
		{
			// iterate SubGraphs
			if (UFlowNode_SubGraph* SubGraphNode = Cast<UFlowNode_SubGraph>(Node))
			{
				const TWeakObjectPtr<UFlowAsset> SubFlowInstance = GetFlowInstance(SubGraphNode);
				if (SubFlowInstance.IsValid())
				{
					const FFlowAssetSaveData SubAssetRecord = SubFlowInstance->SaveInstance(SavedFlowInstances);
					SubGraphNode->SavedAssetInstanceName = SubAssetRecord.InstanceName;
				}
			}

			FFlowNodeSaveData NodeRecord;
			Node->SaveInstance(NodeRecord);

			AssetRecord.NodeRecords.Emplace(NodeRecord);
		}
	}

	// serialize asset
	FMemoryWriter MemoryWriter(AssetRecord.AssetData, true);
	FFlowArchive Ar(MemoryWriter);
	Serialize(Ar);

	// write archive to SaveGame
	SavedFlowInstances.Emplace(AssetRecord);

	return AssetRecord;
}

void UFlowAsset::LoadInstance(const FFlowAssetSaveData& AssetRecord)
{
	FMemoryReader MemoryReader(AssetRecord.AssetData, true);
	FFlowArchive Ar(MemoryReader);
	Serialize(Ar);

	PreStartFlow();

	// iterate graph "from the end", backward to execution order
	// prevents issue when the preceding node would instantly fire output to a not-yet-loaded node
	for (int32 i = AssetRecord.NodeRecords.Num() - 1; i >= 0; i--)
	{
		if (UFlowNode* Node = Nodes.FindRef(AssetRecord.NodeRecords[i].NodeGuid))
		{
			Node->LoadInstance(AssetRecord.NodeRecords[i]);
		}
	}

	OnLoad();
}

void UFlowAsset::OnActivationStateLoaded(UFlowNode* Node)
{
	if (Node->ActivationState != EFlowNodeState::NeverActivated)
	{
		RecordedNodes.Emplace(Node);
	}

	if (Node->ActivationState == EFlowNodeState::Active)
	{
		ActiveNodes.Emplace(Node);
	}
}

void UFlowAsset::OnSave_Implementation()
{
}

void UFlowAsset::OnLoad_Implementation()
{
}

bool UFlowAsset::IsBoundToWorld_Implementation()
{
	return bWorldBound;
}

#if WITH_EDITOR
void UFlowAsset::LogError(const FString& MessageToLog, const UFlowNodeBase* Node) const
{
	// this is runtime log which is should be only called on runtime instances of asset
	if (TemplateAsset)
	{
		UE_LOG(LogFlow, Log, TEXT("Attempted to use Runtime Log on asset instance %s"), *MessageToLog);
	}

	if (RuntimeLog.Get())
	{
		const TSharedRef<FTokenizedMessage> TokenizedMessage = RuntimeLog.Get()->Error(*MessageToLog, Node);
		BroadcastRuntimeMessageAdded(TokenizedMessage);
	}
}

void UFlowAsset::LogWarning(const FString& MessageToLog, const UFlowNodeBase* Node) const
{
	// this is runtime log which is should be only called on runtime instances of asset
	if (TemplateAsset)
	{
		UE_LOG(LogFlow, Log, TEXT("Attempted to use Runtime Log on asset instance %s"), *MessageToLog);
	}

	if (RuntimeLog.Get())
	{
		const TSharedRef<FTokenizedMessage> TokenizedMessage = RuntimeLog.Get()->Warning(*MessageToLog, Node);
		BroadcastRuntimeMessageAdded(TokenizedMessage);
	}
}

void UFlowAsset::LogNote(const FString& MessageToLog, const UFlowNodeBase* Node) const
{
	// this is runtime log which is should be only called on runtime instances of asset
	if (TemplateAsset)
	{
		UE_LOG(LogFlow, Log, TEXT("Attempted to use Runtime Log on asset instance %s"), *MessageToLog);
	}

	if (RuntimeLog.Get())
	{
		const TSharedRef<FTokenizedMessage> TokenizedMessage = RuntimeLog.Get()->Note(*MessageToLog, Node);
		BroadcastRuntimeMessageAdded(TokenizedMessage);
	}
}
#endif
