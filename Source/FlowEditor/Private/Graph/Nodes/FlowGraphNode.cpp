// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Graph/Nodes/FlowGraphNode.h"

#include "FlowAsset.h"
#include "AddOns/FlowNodeAddOn.h"
#include "Nodes/FlowNode.h"

#include "Debugger/FlowDebuggerSubsystem.h"

#include "FlowEditorCommands.h"
#include "Graph/FlowGraph.h"
#include "Graph/FlowGraphEditorSettings.h"
#include "Graph/FlowGraphSchema.h"
#include "Graph/FlowGraphSettings.h"
#include "Graph/Widgets/SFlowGraphNode.h"
#include "Graph/Widgets/SGraphEditorActionMenuFlow.h"

#include "BlueprintNodeHelpers.h"
#include "Developer/ToolMenus/Public/ToolMenus.h"
#include "DiffResults.h"
#include "Editor.h"
#include "FlowEditorLogChannels.h"
#include "Framework/Commands/GenericCommands.h"
#include "GraphDiffControl.h"
#include "GraphEditorActions.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "SourceCodeNavigation.h"
#include "Textures/SlateIcon.h"
#include "ToolMenuSection.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Nodes/FlowNodeBlueprint.h"
#include "StructUtils/PropertyBag.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlowGraphNode)

#define LOCTEXT_NAMESPACE "FlowGraphNode"

UFlowGraphNode::UFlowGraphNode(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer), NodeInstance(nullptr), bBlueprintCompilationPending(false), bIsReconstructingNode(false), bIsDestroyingNode(false), bNeedsFullReconstruction(false)
{
	OrphanedPinSaveMode = ESaveOrphanPinMode::SaveAll;
}

void UFlowGraphNode::SetNodeTemplate(UFlowNodeBase* InNodeInstance)
{
	ensure(InNodeInstance);
	NodeInstance = InNodeInstance;
	NodeInstanceClass = InNodeInstance->GetClass();
}

const UFlowNodeBase* UFlowGraphNode::GetNodeTemplate() const
{
	return NodeInstance;
}

UFlowNodeBase* UFlowGraphNode::GetFlowNodeBase() const
{
	if (NodeInstance)
	{
		if (const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance))
		{
			if (const UFlowAsset* InspectedInstance = FlowNode->GetFlowAsset()->GetInspectedInstance())
			{
				return InspectedInstance->GetNode(FlowNode->GetGuid());
			}
		}

		return NodeInstance;
	}

	return nullptr;
}

void UFlowGraphNode::PostLoad()
{
	Super::PostLoad();

	if (NodeInstance)
	{
		NodeInstance->FixNode(this); // fix already created nodes
		SubscribeToExternalChanges();
	}

	RebuildPinArrays();
}

void UFlowGraphNode::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bDuplicateForPIE)
	{
		CreateNewGuid();

		UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
		if (FlowNode && FlowNode->GetFlowAsset())
		{
			FlowNode->GetFlowAsset()->RegisterNode(NodeGuid, FlowNode);
		}
	}
}

void UFlowGraphNode::PostEditImport()
{
	Super::PostEditImport();

	PostCopyNode();
	SubscribeToExternalChanges();

	// Reset the owning graph after an edit import
	ResetNodeOwner();

	if (NodeInstance)
	{
		InitializeInstance();
	}
}

void UFlowGraphNode::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();

	SubscribeToExternalChanges();

	// note: NodeInstance can be already spawned by paste operation, don't override it
	if (NodeInstanceClass.IsPending())
	{
		NodeInstanceClass.LoadSynchronous();
	}

	if (NodeInstance == nullptr)
	{
		if (const UClass* NodeClass = NodeInstanceClass.Get())
		{
			const UEdGraph* Graph = GetGraph();
			if (Graph && Graph->GetOuter())
			{
				NodeInstance = NewObject<UFlowNodeBase>(Graph->GetOuter(), NodeClass);
				NodeInstance->SetFlags(RF_Transactional);

				InitializeInstance();
				bNeedsFullReconstruction = true;
			}
		}
	}
}

void UFlowGraphNode::PrepareForCopying()
{
	Super::PrepareForCopying();

	if (NodeInstance)
	{
		// Temporarily take ownership of the node instance, so that it is not deleted when cutting
		NodeInstance->Rename(nullptr, this, REN_DontCreateRedirectors | REN_DoNotDirty);
	}
}

void UFlowGraphNode::PostPasteNode()
{
	Super::PostPasteNode();
	// prep reconstruct the node, necessary for copy-paste to handle the reconstruct.
	bNeedsFullReconstruction = true;
}

void UFlowGraphNode::PostCopyNode()
{
	// Make sure this NodeInstance is owned by the FlowAsset it's being pasted into
	if (NodeInstance)
	{
		UFlowAsset* FlowAsset = GetFlowAsset();

		if (NodeInstance->GetOuter() != FlowAsset)
		{
			// Ensures NodeInstance is owned by the FlowAsset
			NodeInstance->Rename(nullptr, FlowAsset, REN_DontCreateRedirectors);
		}

		NodeInstance->SetGraphNode(this);
	}

	// Reset the node's owning graph prior to copying
	ResetNodeOwner();
}

void UFlowGraphNode::SubscribeToExternalChanges()
{
	if (NodeInstance)
	{
		NodeInstance->OnReconstructionRequested.BindUObject(this, &UFlowGraphNode::OnExternalChange);
		if (UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance))
		{
			FlowNode->OnPropertyChangedEvent.BindUObject(this, &UFlowGraphNode::OnNodeInstancePropertyChanged);
		}
	}
}

void UFlowGraphNode::OnExternalChange()
{
	if (bIsReconstructingNode)
	{
		return;
	}

	// Do not create transaction here, since this function triggers from modifying UFlowNode's property, which itself already made inside of transaction.
	Modify();

	bNeedsFullReconstruction = true;
	ReconstructNode();

	GetGraph()->NotifyNodeChanged(this);
}

void UFlowGraphNode::OnGraphRefresh()
{
	ReconstructNode();
}

bool UFlowGraphNode::CanPlaceBreakpoints() const
{
	return true;
}

void UFlowGraphNode::OnGraphNodeLoaded()
{
	if (NodeInstance)
	{
		NodeInstance->OnGraphLoaded();
	}
}

bool UFlowGraphNode::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const
{
	return Schema->IsA(UFlowGraphSchema::StaticClass());
}

void UFlowGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (FromPin != nullptr)
	{
		UEdGraphNode* FromNode = FromPin->GetOwningNode();
		if (!FromNode)
		{
			return;
		}
		const UFlowGraphSchema* Schema = CastChecked<UFlowGraphSchema>(GetSchema());

		TSet<UEdGraphNode*> NodeList;

		// auto-connect from dragged pin to first compatible pin on the new node
		for (UEdGraphPin* Pin : Pins)
		{
			check(Pin);
			FPinConnectionResponse Response = Schema->CanCreateConnection(FromPin, Pin);
			if (CONNECT_RESPONSE_MAKE == Response.Response)
			{
				if (Schema->TryCreateConnection(FromPin, Pin))
				{
					NodeList.Add(FromNode);
					NodeList.Add(this);
				}
				break;
			}
			else if (CONNECT_RESPONSE_BREAK_OTHERS_A == Response.Response)
			{
				InsertNewNode(FromPin, Pin, NodeList);
				break;
			}
		}

		// Send all nodes that received a new pin connection a notification
		for (auto It = NodeList.CreateConstIterator(); It; ++It)
		{
			UEdGraphNode* Node = (*It);
			Node->NodeConnectionListChanged();
		}
	}
}

void UFlowGraphNode::InsertNewNode(UEdGraphPin* FromPin, UEdGraphPin* NewLinkPin, TSet<UEdGraphNode*>& OutNodeList)
{
	const UFlowGraphSchema* Schema = CastChecked<UFlowGraphSchema>(GetSchema());

	// The pin we are creating from already has a connection that needs to be broken. We want to "insert" the new node in between, so that the output of the new node is hooked up too
	UEdGraphPin* OldLinkedPin = FromPin->LinkedTo[0];
	check(OldLinkedPin);

	FromPin->BreakAllPinLinks();

	// Hook up the old linked pin to the first valid output pin on the new node
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		UEdGraphPin* OutputExecPin = Pins[PinIndex];
		check(OutputExecPin);
		if (CONNECT_RESPONSE_MAKE == Schema->CanCreateConnection(OldLinkedPin, OutputExecPin).Response)
		{
			if (Schema->TryCreateConnection(OldLinkedPin, OutputExecPin))
			{
				OutNodeList.Add(OldLinkedPin->GetOwningNode());
				OutNodeList.Add(this);
			}
			break;
		}
	}

	if (Schema->TryCreateConnection(FromPin, NewLinkPin))
	{
		OutNodeList.Add(FromPin->GetOwningNode());
		OutNodeList.Add(this);
	}
}

void UFlowGraphNode::ReconstructNode()
{
	UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	if (!FlowNode)
	{
		return;
	}

	// Check if reconstruction is allowed
	if (!CanReconstructNode())
	{
		return;
	}

	bIsReconstructingNode = true;
	ON_SCOPE_EXIT
	{
		bIsReconstructingNode = false;
	};

	// Determine if reconstruction is needed
	bool bPinsMismatch = false;
	if (!bNeedsFullReconstruction)
	{
		TSet<FFlowPinSignature> ExpectedPins;
		GetExpectedPinSignatures(/*out*/ ExpectedPins);
		bPinsMismatch = !DoVisualPinsMatchExpected(ExpectedPins);
	}

	if (bNeedsFullReconstruction || bPinsMismatch)
	{
		const FScopedTransaction Transaction(LOCTEXT("ReconstructNode", "Reconstruct Node"));
		Modify();

		FlowNode->CachePinProperties();
		TArray<UEdGraphPin*> OldPins = Pins;

		Pins.Reset();
		InputPins.Reset();
		OutputPins.Reset();

		AllocateDefaultPins();
		SpecializeNewlyAllocatedPinTypes(OldPins);
		RewireOldPinsToNewPins(OldPins);

		// Destroy old pins
		for (UEdGraphPin* OldPin : OldPins)
		{
			if (OldPin && !Pins.Contains(OldPin))
			{
				OldPin->Modify();
				OldPin->BreakAllPinLinks(true);
				DestroyPin(OldPin);
			}
		}

		// Clear any breakpoints associated with pins that no longer exist
		if (UFlowDebuggerSubsystem* DebuggerSubsystem = GEngine->GetEngineSubsystem<UFlowDebuggerSubsystem>())
		{
			DebuggerSubsystem->RemoveObsoletePinBreakpoints(this);
		}
		bNeedsFullReconstruction = false;
		GetGraph()->NotifyNodeChanged(this);
	}

	// This ensures the graph editor 'Refresh' button still rebuilds all the graph widgets even if the FlowGraphNode has nothing to update
	// Ideally we could get rid of the 'Refresh' button, but I think it will keep being useful, esp. for users making rough custom widgets
	(void)OnReconstructNodeCompleted.ExecuteIfBound();
}

void UFlowGraphNode::AllocateDefaultPins()
{
	// Helper lambda for pin creation and configuration
	auto ConfigurePin = [&](UEdGraphPin* NewPin, const FFlowPin& FlowPinData) {
		if (!NewPin)
		{
			return;
		}
		NewPin->PinFriendlyName = FlowPinData.PinFriendlyName;
		NewPin->PinToolTip = FlowPinData.PinToolTip;
		NewPin->bAllowFriendlyName = !NewPin->PinFriendlyName.IsEmpty() && (NewPin->PinFriendlyName.ToString() != NewPin->PinName.ToString());
	};

	const auto K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (!K2Schema)
	{
		return;
	}

	TArray<FFlowPin> ExpectedInputs;
	TArray<FFlowPin> ExpectedOutputs;
	if (GatherAllExpectedPins(/*out*/ ExpectedInputs, /*out*/ ExpectedOutputs))
	{
		for (const FFlowPin& FlowPinData : ExpectedInputs)
		{
			UEdGraphPin* NewPin = CreatePin(EGPD_Input, FlowPinData.PinType, FlowPinData.PinName);
			ConfigurePin(NewPin, FlowPinData);
			K2Schema->SetPinAutogeneratedDefaultValueBasedOnType(NewPin);
			K2Schema->ResetPinToAutogeneratedDefaultValue(NewPin, false);
			SyncPropertyValueToPin(NewPin);
		}
		for (const FFlowPin& FlowPinData : ExpectedOutputs)
		{
			UEdGraphPin* NewPin = CreatePin(EGPD_Output, FlowPinData.PinType, FlowPinData.PinName);
			ConfigurePin(NewPin, FlowPinData);
		}
	}

	RebuildPinArrays();
}

void UFlowGraphNode::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);
	if (GIsTransacting)
	{
		return;
	}

	if (!Pin || !NodeInstance || Pin->Direction != EGPD_Input || Pin->LinkedTo.Num() > 0 || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		return;
	}

	UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	if (!FlowNode)
	{
		return;
	}

	const FProperty* BoundProp = FlowNode->GetInputProperty(Pin->PinName);
	if (!BoundProp)
	{
		FlowNode->CachePinProperties();
		BoundProp = FlowNode->GetInputProperty(Pin->PinName);
		if (!BoundProp)
		{
			UE_LOG(LogFlowEditor, Warning, TEXT("PinDefaultValueChanged: Property not found for pin %s even after re-caching."), *Pin->PinName.ToString());
			return;
		}
	}

	const auto Container = FlowNode->GetPropertyContainer(BoundProp);
	uint8* PropertyDataPtr = BoundProp->ContainerPtrToValuePtr<uint8>(Container);
	if (!PropertyDataPtr)
	{
		return;
	}

	const FString ValueStrFromPin = Pin->GetDefaultAsString();
	FString CurrentPropertyValueAsString;
	BoundProp->ExportTextItem_Direct(CurrentPropertyValueAsString, PropertyDataPtr, PropertyDataPtr, nullptr, PPF_None);

	if (CurrentPropertyValueAsString != ValueStrFromPin)
	{
		NodeInstance->Modify();
		FBlueprintEditorUtils::PropertyValueFromString_Direct(BoundProp, ValueStrFromPin, PropertyDataPtr, FlowNode, PPF_None);
		FlowNode->OnPropertyChanged(BoundProp->GetFName());
	}
}

void UFlowGraphNode::RewireOldPinsToNewPins(TArray<UEdGraphPin*>& InOldPins)
{
	TArray<UEdGraphPin*> OrphanedOldPins;
	TArray<bool> NewPinMatched;                   // Tracks whether a NewPin has already been matched to an OldPin
	TMap<UEdGraphPin*, UEdGraphPin*> MatchedPins; // Old to New

	const int32 NumNewPins = Pins.Num();
	NewPinMatched.AddDefaulted(NumNewPins);

	// Rewire any connection to pins that are matched by name (O(N^2) right now)
	// NOTE: we iterate backwards through the list because ReconstructSinglePin()
	//       destroys pins as we go along (clearing out parent pointers, etc.);
	//       we need the parent pin chain intact for DoPinsMatchForReconstruction();
	//       we want to destroy old pins from the split children (leaves) up, so
	//       we do this since split child pins are ordered later in the list
	//       (after their parents)
	for (int32 OldPinIndex = InOldPins.Num() - 1; OldPinIndex >= 0; --OldPinIndex)
	{
		UEdGraphPin* OldPin = InOldPins[OldPinIndex];

		// common case is for InOldPins and Pins to match, so we start searching from the current index:
		bool bMatched = false;
		int32 NewPinIndex = (NumNewPins ? OldPinIndex % NumNewPins : 0);
		for (int32 NewPinCount = NumNewPins - 1; NewPinCount >= 0; --NewPinCount)
		{
			// if Pins grows then we may skip entries and fail to find a match or NewPinMatched will not be accurate
			check(NumNewPins == Pins.Num());
			if (!NewPinMatched[NewPinIndex])
			{
				UEdGraphPin* NewPin = Pins[NewPinIndex];

				if (NewPin->PinName == OldPin->PinName)
				{
					ReconstructSinglePin(NewPin, OldPin);

					MatchedPins.Add(OldPin, NewPin);
					bMatched = true;
					NewPinMatched[NewPinIndex] = true;
					break;
				}
			}
			NewPinIndex = (NewPinIndex + 1) % Pins.Num();
		}

		// Orphaned pins are those that existed in the OldPins array but do not in the NewPins.
		// We will save these pins and add them to the NewPins array if they are linked to other pins or have non-default value unless:
		// * The node has been flagged to not save orphaned pins
		// * The pin has been flagged not be saved if orphaned
		// * The pin is hidden
		if (UEdGraphPin::AreOrphanPinsEnabled() && !bDisableOrphanPinSaving && OrphanedPinSaveMode == ESaveOrphanPinMode::SaveAll
		    && !bMatched && !OldPin->bHidden && OldPin->ShouldSavePinIfOrphaned() && OldPin->LinkedTo.Num() > 0)
		{
			OldPin->bOrphanedPin = true;
			OldPin->bNotConnectable = true;
			OrphanedOldPins.Add(OldPin);
			InOldPins.RemoveAt(OldPinIndex, 1, EAllowShrinking::No);
		}
	}

	// The orphaned pins get placed after the rest of the new pins
	for (int32 OrphanedIndex = OrphanedOldPins.Num() - 1; OrphanedIndex >= 0; --OrphanedIndex)
	{
		UEdGraphPin* OrphanedPin = OrphanedOldPins[OrphanedIndex];
		if (OrphanedPin->ParentPin == nullptr)
		{
			Pins.Add(OrphanedPin);

			switch (OrphanedPin->Direction)
			{
				case EGPD_Input:
					InputPins.Add(OrphanedPin);
					break;
				case EGPD_Output:
					OutputPins.Add(OrphanedPin);
					break;
				default:;
			}
		}
	}
}

void UFlowGraphNode::ReconstructSinglePin(UEdGraphPin* NewPin, UEdGraphPin* OldPin)
{
	check(NewPin && OldPin);

	// Copy over modified persistent data
	NewPin->MovePersistentDataFromOldPin(*OldPin);
}

void UFlowGraphNode::GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	const FGenericCommands& GenericCommands = FGenericCommands::Get();
	const FGraphEditorCommandsImpl& GraphCommands = FGraphEditorCommands::Get();
	const FFlowGraphCommands& FlowGraphCommands = FFlowGraphCommands::Get();

	if (Context->Pin)
	{
		{
			FToolMenuSection& Section = Menu->AddSection("FlowGraphPinActions", LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
			if (Context->Pin->LinkedTo.Num() > 0)
			{
				Section.AddMenuEntry(GraphCommands.BreakPinLinks);
			}

			if (Context->Pin->Direction == EGPD_Input && CanUserRemoveInput(Context->Pin))
			{
				Section.AddMenuEntry(FlowGraphCommands.RemovePin);
			}
			else if (Context->Pin->Direction == EGPD_Output && CanUserRemoveOutput(Context->Pin))
			{
				Section.AddMenuEntry(FlowGraphCommands.RemovePin);
			}
		}

		{
			FToolMenuSection& Section = Menu->AddSection("FlowGraphPinBreakpoints", LOCTEXT("PinBreakpointsMenuHeader", "Pin Breakpoints"));
			Section.AddMenuEntry(FlowGraphCommands.AddPinBreakpoint);
			Section.AddMenuEntry(FlowGraphCommands.RemovePinBreakpoint);
			Section.AddMenuEntry(FlowGraphCommands.EnablePinBreakpoint);
			Section.AddMenuEntry(FlowGraphCommands.DisablePinBreakpoint);
			Section.AddMenuEntry(FlowGraphCommands.TogglePinBreakpoint);
		}

		{
			FToolMenuSection& Section = Menu->AddSection("FlowGraphPinExecutionOverride", LOCTEXT("PinExecutionOverrideMenuHeader", "Execution Override"));
			Section.AddMenuEntry(FlowGraphCommands.ForcePinActivation);
		}
	}
	else if (Context->Node)
	{
		{
			FToolMenuSection& Section = Menu->AddSection("FlowGraphNodeAddOns", LOCTEXT("NodeAddOnsMenuHeader", "AddOns"));
			Section.AddSubMenu(
			    "AttachAddOn",
			    LOCTEXT("AttachAddOn", "Attach AddOn..."),
			    LOCTEXT("AttachAddOnTooltip", "Attaches an AddOn to the Node"),
			    FNewToolMenuDelegate::CreateUObject(this, &UFlowGraphNode::CreateAttachAddOnSubMenu, static_cast<UEdGraph*>(Context->Graph)));
		}

		{
			FToolMenuSection& Section = Menu->AddSection("FlowGraphNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
			Section.AddMenuEntry(GenericCommands.Delete);
			Section.AddMenuEntry(GenericCommands.Cut);
			Section.AddMenuEntry(GenericCommands.Copy);
			Section.AddMenuEntry(GenericCommands.Duplicate);

			Section.AddMenuEntry(GraphCommands.BreakNodeLinks);

			if (SupportsContextPins())
			{
				Section.AddMenuEntry(FlowGraphCommands.ReconstructNode);
			}

			if (CanUserAddInput())
			{
				Section.AddMenuEntry(FlowGraphCommands.AddInput);
			}
			if (CanUserAddOutput())
			{
				Section.AddMenuEntry(FlowGraphCommands.AddOutput);
			}
		}

		{
			FToolMenuSection& Section = Menu->AddSection("FlowGraphNodeBreakpoints", LOCTEXT("NodeBreakpointsMenuHeader", "Node Breakpoints"));
			Section.AddMenuEntry(GraphCommands.AddBreakpoint);
			Section.AddMenuEntry(GraphCommands.RemoveBreakpoint);
			Section.AddMenuEntry(GraphCommands.EnableBreakpoint);
			Section.AddMenuEntry(GraphCommands.DisableBreakpoint);
			Section.AddMenuEntry(GraphCommands.ToggleBreakpoint);
		}

		{
			FToolMenuSection& Section = Menu->AddSection("FlowGraphNodeExecutionOverride", LOCTEXT("NodeExecutionOverrideMenuHeader", "Execution Override"));
			if (CanSetSignalMode(EFlowSignalMode::Enabled))
			{
				Section.AddMenuEntry(FlowGraphCommands.EnableNode);
			}
			if (CanSetSignalMode(EFlowSignalMode::Disabled))
			{
				Section.AddMenuEntry(FlowGraphCommands.DisableNode);
			}
			if (CanSetSignalMode(EFlowSignalMode::PassThrough))
			{
				Section.AddMenuEntry(FlowGraphCommands.SetPassThrough);
			}
		}

		{
			FToolMenuSection& Section = Menu->AddSection("FlowGraphNodeJumps", LOCTEXT("NodeJumpsMenuHeader", "Jumps"));
			if (CanFocusViewport())
			{
				Section.AddMenuEntry(FlowGraphCommands.FocusViewport);
			}
			if (CanJumpToDefinition())
			{
				Section.AddMenuEntry(FlowGraphCommands.JumpToNodeDefinition);
			}
		}

		{
			FToolMenuSection& Section = Menu->AddSection("FlowGraphNodeOrganisation", LOCTEXT("NodeOrganisation", "Organisation"));
			Section.AddSubMenu("Alignment", LOCTEXT("AlignmentHeader", "Alignment"), FText(), FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu) {
				FToolMenuSection& SubMenuSection = SubMenu->AddSection("EdGraphSchemaAlignment", LOCTEXT("AlignHeader", "Align"));
				SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesTop);
				SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesMiddle);
				SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesBottom);
				SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesLeft);
				SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesCenter);
				SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesRight);
				SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().StraightenConnections);
			}));
		}
	}
}

void UFlowGraphNode::CreateAttachAddOnSubMenu(UToolMenu* Menu, UEdGraph* Graph) const
{
	UFlowGraphNode* MutableThis = const_cast<UFlowGraphNode*>(this);

	const TSharedRef<SGraphEditorActionMenuFlow> Widget =
	    SNew(SGraphEditorActionMenuFlow)
	        .GraphObj(Graph)
	        .GraphNode(MutableThis)
	        .AutoExpandActionMenu(true);

	Menu->AddMenuEntry("Section", FToolMenuEntry::InitWidget("Widget", Widget, FText(), true));
}

bool UFlowGraphNode::CanUserDeleteNode() const
{
	return NodeInstance ? NodeInstance->bCanDelete : Super::CanUserDeleteNode();
}

bool UFlowGraphNode::CanDuplicateNode() const
{
	if (NodeInstance)
	{
		return NodeInstance->bCanDuplicate;
	}

	// support code paths calling this method on CDO, where there's no Flow Node Instance
	if (AssignedNodeClasses.Num() > 0)
	{
		// we simply allow action if any Assigned Node Class accepts it, as the action is disallowed in special node likes StartNode
		for (const UClass* Class : AssignedNodeClasses)
		{
			const UFlowNode* NodeDefaults = Class->GetDefaultObject<UFlowNode>();
			if (NodeDefaults && NodeDefaults->bCanDuplicate)
			{
				return true;
			}
		}

		return false;
	}

	return true;
}

bool UFlowGraphNode::CanPasteHere(const UEdGraph* TargetGraph) const
{
	const UFlowGraph* FlowGraph = Cast<UFlowGraph>(TargetGraph);
	if (FlowGraph == nullptr)
	{
		return false;
	}

	return Super::CanPasteHere(TargetGraph) && FlowGraph->GetFlowAsset()->IsNodeOrAddOnClassAllowed(NodeInstanceClass.Get());
}

TSharedPtr<SGraphNode> UFlowGraphNode::CreateVisualWidget()
{
	return SNew(SFlowGraphNode, this);
}

FText UFlowGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (NodeInstance)
	{
		if (UFlowGraphEditorSettings::Get()->bShowNodeClass)
		{
			FString CleanAssetName;
			if (NodeInstance->GetClass()->ClassGeneratedBy)
			{
				NodeInstance->GetClass()->GetPathName(nullptr, CleanAssetName);
				const int32 SubStringIdx = CleanAssetName.Find(".", ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				CleanAssetName.LeftInline(SubStringIdx);
			}
			else
			{
				CleanAssetName = NodeInstance->GetClass()->GetName();
			}

			FFormatNamedArguments Args;
			Args.Add(TEXT("NodeTitle"), NodeInstance->GetNodeTitle());
			Args.Add(TEXT("AssetName"), FText::FromString(CleanAssetName));
			return FText::Format(INVTEXT("{NodeTitle}\n{AssetName}"), Args);
		}

		return NodeInstance->GetNodeTitle();
	}

	return Super::GetNodeTitle(TitleType);
}

FLinearColor UFlowGraphNode::GetNodeTitleColor() const
{
	if (NodeInstance)
	{
		FLinearColor DynamicColor;
		if (NodeInstance->GetDynamicTitleColor(DynamicColor))
		{
			return DynamicColor;
		}

		if (const FLinearColor* StyleColor = UFlowGraphSettings::Get()->LookupNodeTitleColorForNode(*NodeInstance))
		{
			return *StyleColor;
		}
	}

	return Super::GetNodeTitleColor();
}

FSlateIcon UFlowGraphNode::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIcon();
}

FText UFlowGraphNode::GetTooltipText() const
{
	FText Tooltip;
	if (NodeInstance)
	{
		Tooltip = NodeInstance->GetNodeToolTip();
	}
	if (Tooltip.IsEmpty())
	{
		Tooltip = GetNodeTitle(ENodeTitleType::ListView);
	}
	return Tooltip;
}

FString UFlowGraphNode::GetNodeDescription() const
{
	if (NodeInstance && (GEditor->PlayWorld == nullptr || UFlowGraphEditorSettings::Get()->bShowNodeDescriptionWhilePlaying))
	{
		return NodeInstance->GetNodeDescription();
	}

	return FString();
}

UFlowNode* UFlowGraphNode::GetInspectedNodeInstance() const
{
	const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	return FlowNode ? FlowNode->GetInspectedInstance() : nullptr;
}

EFlowNodeState UFlowGraphNode::GetActivationState() const
{
	if (const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance))
	{
		if (const UFlowNode* InspectedInstance = FlowNode->GetInspectedInstance())
		{
			return InspectedInstance->GetActivationState();
		}
	}

	return EFlowNodeState::NeverActivated;
}

FString UFlowGraphNode::GetStatusString() const
{
	if (const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance))
	{
		if (const UFlowNode* InspectedInstance = FlowNode->GetInspectedInstance())
		{
			return InspectedInstance->GetStatusStringForNodeAndAddOns();
		}
	}

	return FString();
}

FLinearColor UFlowGraphNode::GetStatusBackgroundColor() const
{
	if (const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance))
	{
		if (const UFlowNode* InspectedInstance = FlowNode->GetInspectedInstance())
		{
			FLinearColor ObtainedColor;
			if (InspectedInstance->GetStatusBackgroundColor(ObtainedColor))
			{
				return ObtainedColor;
			}
		}
	}

	return UFlowGraphSettings::Get()->NodeStatusBackground;
}

bool UFlowGraphNode::IsContentPreloaded() const
{
	if (const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance))
	{
		if (const UFlowNode* InspectedInstance = FlowNode->GetInspectedInstance())
		{
			return InspectedInstance->bPreloaded;
		}
	}

	return false;
}

bool UFlowGraphNode::CanFocusViewport() const
{
	UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	return FlowNode ? (GEditor->bIsSimulatingInEditor && FlowNode->GetActorToFocus()) : false;
}

bool UFlowGraphNode::CanJumpToDefinition() const
{
	return NodeInstance != nullptr;
}

void UFlowGraphNode::JumpToDefinition() const
{
	if (NodeInstance)
	{
		if (NodeInstance->GetClass()->IsNative())
		{
			if (FSourceCodeNavigation::CanNavigateToClass(NodeInstance->GetClass()))
			{
				if (FSourceCodeNavigation::NavigateToClass(NodeInstance->GetClass()))
				{
					return;
				}
			}

			// Failing that, fall back to the older method which will still get the file open assuming it exists
			FString NativeParentClassHeaderPath;
			if (FSourceCodeNavigation::FindClassHeaderPath(NodeInstance->GetClass(), NativeParentClassHeaderPath) && (IFileManager::Get().FileSize(*NativeParentClassHeaderPath) != INDEX_NONE))
			{
				const FString AbsNativeParentClassHeaderPath = FPaths::ConvertRelativePathToFull(NativeParentClassHeaderPath);
				FSourceCodeNavigation::OpenSourceFile(AbsNativeParentClassHeaderPath);
			}
		}
		else
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(NodeInstance->GetClass());
		}
	}
}

bool UFlowGraphNode::SupportsCommentBubble() const
{
	if (IsSubNode())
	{
		return false;
	}

	return Super::SupportsCommentBubble();
}

void UFlowGraphNode::OnNodeDoubleClicked() const
{
	UFlowNodeBase* FlowNodeBase = GetFlowNodeBase();
	if (IsValid(FlowNodeBase))
	{
		if (UFlowGraphEditorSettings::Get()->NodeDoubleClickTarget == EFlowNodeDoubleClickTarget::NodeDefinition)
		{
			JumpToDefinition();
		}
		else
		{
			FString AssetPath;
			UObject* AssetToEdit = nullptr;
			if (UFlowNode* FlowNode = Cast<UFlowNode>(FlowNodeBase))
			{
				AssetPath = FlowNode->GetAssetPath();
				AssetToEdit = FlowNode->GetAssetToEdit();
			}

			if (!AssetPath.IsEmpty())
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetPath);
			}
			else if (AssetToEdit)
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetToEdit);

				if (GEditor->PlayWorld != nullptr)
				{
					OnNodeDoubleClickedInPIE();
				}
			}
			else if (UFlowGraphEditorSettings::Get()->NodeDoubleClickTarget == EFlowNodeDoubleClickTarget::PrimaryAssetOrNodeDefinition)
			{
				JumpToDefinition();
			}
		}
	}
}

void UFlowGraphNode::CreateInputPin(const FFlowPin& FlowPin, const int32 Index /*= INDEX_NONE*/)
{
	if (FlowPin.PinName.IsNone())
	{
		return;
	}

	const FName PinCategory = UEdGraphSchema_K2::PC_Exec;
	const FName PinSubCategory = NAME_None;
	constexpr bool bIsReference = false;

	const FEdGraphPinType PinType = FEdGraphPinType(PinCategory, PinSubCategory, nullptr, EPinContainerType::None, bIsReference, FEdGraphTerminalType());
	UEdGraphPin* NewPin = CreatePin(EGPD_Input, PinType, FlowPin.PinName, Index);
	check(NewPin);

	if (!FlowPin.PinFriendlyName.IsEmpty())
	{
		NewPin->bAllowFriendlyName = true;
		NewPin->PinFriendlyName = FlowPin.PinFriendlyName;
	}

	NewPin->PinToolTip = FlowPin.PinToolTip;

	InputPins.Emplace(NewPin);
}

void UFlowGraphNode::CreateOutputPin(const FFlowPin& FlowPin, const int32 Index /*= INDEX_NONE*/)
{
	if (FlowPin.PinName.IsNone())
	{
		return;
	}

	const FName PinCategory = UEdGraphSchema_K2::PC_Exec;
	const FName PinSubCategory = NAME_None;
	constexpr bool bIsReference = false;

	const FEdGraphPinType PinType = FEdGraphPinType(PinCategory, PinSubCategory, nullptr, EPinContainerType::None, bIsReference, FEdGraphTerminalType());
	UEdGraphPin* NewPin = CreatePin(EGPD_Output, PinType, FlowPin.PinName, Index);
	check(NewPin);

	if (!FlowPin.PinFriendlyName.IsEmpty())
	{
		NewPin->bAllowFriendlyName = true;
		NewPin->PinFriendlyName = FlowPin.PinFriendlyName;
	}

	NewPin->PinToolTip = FlowPin.PinToolTip;

	OutputPins.Emplace(NewPin);
}

void UFlowGraphNode::RemoveOrphanedPin(UEdGraphPin* Pin)
{
	const FScopedTransaction Transaction(LOCTEXT("RemoveOrphanedPin", "Remove Orphaned Pin"));
	Modify();

	if (UFlowDebuggerSubsystem* DebuggerSubsystem = GEngine->GetEngineSubsystem<UFlowDebuggerSubsystem>())
	{
		DebuggerSubsystem->RemovePinBreakpoint(NodeGuid, Pin->PinName);
	}

	Pin->MarkAsGarbage();
	Pins.Remove(Pin);

	ReconstructNode();

	GetGraph()->NotifyNodeChanged(this);
}

bool UFlowGraphNode::SupportsContextPins() const
{
	return NodeInstance && NodeInstance->SupportsContextPins();
}

bool UFlowGraphNode::CanUserAddInput() const
{
	const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	return FlowNode && FlowNode->CanUserAddInput() && InputPins.Num() < 256;
}

bool UFlowGraphNode::CanUserAddOutput() const
{
	const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	return FlowNode && FlowNode->CanUserAddOutput() && OutputPins.Num() < 256;
}

bool UFlowGraphNode::CanUserRemoveInput(const UEdGraphPin* Pin) const
{
	const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	return FlowNode && !FlowNode->GetClass()->GetDefaultObject<UFlowNode>()->InputPins.Contains(Pin->PinName);
}

bool UFlowGraphNode::CanUserRemoveOutput(const UEdGraphPin* Pin) const
{
	const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	return FlowNode && !FlowNode->GetClass()->GetDefaultObject<UFlowNode>()->OutputPins.Contains(Pin->PinName);
}

void UFlowGraphNode::AddUserInput()
{
	const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	AddInstancePin(EGPD_Input, FlowNode->CountNumberedInputs());
}

void UFlowGraphNode::AddUserOutput()
{
	const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	AddInstancePin(EGPD_Output, FlowNode->CountNumberedOutputs());
}

void UFlowGraphNode::AddInstancePin(const EEdGraphPinDirection Direction, const uint8 NumberedPinsAmount)
{
	const FScopedTransaction Transaction(LOCTEXT("AddInstancePin", "Add Instance Pin"));
	Modify();

	const FFlowPin PinName = FFlowPin(FString::FromInt(NumberedPinsAmount));

	UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	if (Direction == EGPD_Input)
	{
		if (FlowNode->InputPins.IsValidIndex(NumberedPinsAmount))
		{
			FlowNode->InputPins.Insert(PinName, NumberedPinsAmount);
		}
		else
		{
			FlowNode->InputPins.Add(PinName);
		}

		CreateInputPin(PinName, NumberedPinsAmount);
	}
	else
	{
		if (FlowNode->OutputPins.IsValidIndex(NumberedPinsAmount))
		{
			FlowNode->OutputPins.Insert(PinName, NumberedPinsAmount);
		}
		else
		{
			FlowNode->OutputPins.Add(PinName);
		}

		CreateOutputPin(PinName, FlowNode->InputPins.Num() + NumberedPinsAmount);
	}

	GetGraph()->NotifyNodeChanged(this);
}

void UFlowGraphNode::RemoveInstancePin(UEdGraphPin* Pin)
{
	const FScopedTransaction Transaction(LOCTEXT("RemoveInstancePin", "Remove Instance Pin"));
	Modify();

	if (UFlowDebuggerSubsystem* DebuggerSubsystem = GEngine->GetEngineSubsystem<UFlowDebuggerSubsystem>())
	{
		DebuggerSubsystem->RemovePinBreakpoint(NodeGuid, Pin->PinName);
	}

	UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	if (Pin->Direction == EGPD_Input)
	{
		if (InputPins.Contains(Pin))
		{
			InputPins.Remove(Pin);
			FlowNode->RemoveUserInput(Pin->PinName);

			Pin->MarkAsGarbage();
			Pins.Remove(Pin);
		}
	}
	else
	{
		if (OutputPins.Contains(Pin))
		{
			OutputPins.Remove(Pin);
			FlowNode->RemoveUserOutput(Pin->PinName);

			Pin->MarkAsGarbage();
			Pins.Remove(Pin);
		}
	}

	ReconstructNode();
	GetGraph()->NotifyNodeChanged(this);
}

void UFlowGraphNode::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	// start with the default hover text (from the pin's tool-tip)
	Super::GetPinHoverText(Pin, HoverTextOut);

	// add information on pin activations
	if (GEditor->PlayWorld)
	{
		if (const UFlowNode* InspectedNodeInstance = GetInspectedNodeInstance())
		{
			if (!HoverTextOut.IsEmpty())
			{
				HoverTextOut.Append(LINE_TERMINATOR).Append(LINE_TERMINATOR);
			}

			const TArray<FPinRecord>& PinRecords = InspectedNodeInstance->GetPinRecords(Pin.PinName, Pin.Direction);
			if (PinRecords.Num() == 0)
			{
				HoverTextOut.Append(FPinRecord::NoActivations);
			}
			else
			{
				HoverTextOut.Append(FPinRecord::PinActivations);
				for (int32 i = 0; i < PinRecords.Num(); i++)
				{
					HoverTextOut.Append(LINE_TERMINATOR);
					HoverTextOut.Appendf(TEXT("%d) %s"), i + 1, *PinRecords[i].HumanReadableTime);

					switch (PinRecords[i].ActivationType)
					{
						case EFlowPinActivationType::Default:
							break;
						case EFlowPinActivationType::Forced:
							HoverTextOut.Append(FPinRecord::ForcedActivation);
							break;
						case EFlowPinActivationType::PassThrough:
							HoverTextOut.Append(FPinRecord::PassThroughActivation);
							break;
						default:;
					}
				}
			}
		}
	}
}

void UFlowGraphNode::ForcePinActivation(const FEdGraphPinReference PinReference) const
{
	UFlowNode* InspectedNodeInstance = GetInspectedNodeInstance();
	if (InspectedNodeInstance == nullptr)
	{
		return;
	}

	if (const UEdGraphPin* FoundPin = PinReference.Get())
	{
		switch (FoundPin->Direction)
		{
			case EGPD_Input:
				InspectedNodeInstance->TriggerInput(FoundPin->PinName, EFlowPinActivationType::Forced);
				break;
			case EGPD_Output:
				InspectedNodeInstance->TriggerOutput(FoundPin->PinName, false, EFlowPinActivationType::Forced);
				break;
			default:;
		}
	}
}

void UFlowGraphNode::SetSignalMode(const EFlowSignalMode Mode)
{
	if (UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance))
	{
		FlowNode->SignalMode = Mode;
		OnSignalModeChanged.ExecuteIfBound();
	}
}

EFlowSignalMode UFlowGraphNode::GetSignalMode() const
{
	if (IsSubNode())
	{
		// SubNodes count as enabled for signal mode queries in the editor
		return EFlowSignalMode::Enabled;
	}

	const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	if (IsValid(FlowNode))
	{
		return FlowNode->SignalMode;
	}
	else
	{
		return EFlowSignalMode::Disabled;
	}
}

bool UFlowGraphNode::CanSetSignalMode(const EFlowSignalMode Mode) const
{
	const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	return FlowNode ? (FlowNode->AllowedSignalModes.Contains(Mode) && FlowNode->SignalMode != Mode) : false;
}

void UFlowGraphNode::InitializeInstance()
{
	check(NodeInstance);

	// link editor and runtime nodes together
	NodeInstance->SetGraphNode(this);
}

void UFlowGraphNode::PostEditUndo()
{
	UEdGraphNode::PostEditUndo();
	ResetNodeOwner();

	if (ParentNode)
	{
		ParentNode->SubNodes.AddUnique(this);

		ParentNode->RebuildRuntimeAddOnsFromEditorSubNodes();
	}
	else
	{
		RebuildRuntimeAddOnsFromEditorSubNodes();
	}
}

UFlowAsset* UFlowGraphNode::GetFlowAsset() const
{
	if (const UFlowGraph* FlowGraph = GetFlowGraph())
	{
		if (UFlowAsset* FlowAsset = FlowGraph->GetFlowAsset())
		{
			return FlowAsset;
		}
	}

	return nullptr;
}

void UFlowGraphNode::LogError(const FString& MessageToLog, const UFlowNodeBase* FlowNodeBase) const
{
	if (const UFlowAsset* FlowAsset = GetFlowAsset())
	{
		FlowAsset->LogError(MessageToLog, FlowNodeBase);
	}
}

void UFlowGraphNode::ResetNodeOwner()
{
	if (NodeInstance)
	{
		const UEdGraph* Graph = GetGraph();
		UObject* GraphOwner = Graph ? Graph->GetOuter() : nullptr;

		NodeInstance->Rename(nullptr, GraphOwner, REN_DontCreateRedirectors | REN_DoNotDirty);
		NodeInstance->ClearFlags(RF_Transient);

		for (const TObjectPtr<UFlowGraphNode>& SubNode : SubNodes)
		{
			SubNode->ResetNodeOwner();
		}
	}
}

FText UFlowGraphNode::GetDescription() const
{
	FString StoredClassName = NodeInstanceClass.GetAssetName();
	StoredClassName.RemoveFromEnd(TEXT("_C"));

	return FText::Format(LOCTEXT("NodeClassError", "Class {0} not found, make sure it's saved!"), FText::FromString(StoredClassName));
}

UEdGraphPin* UFlowGraphNode::GetInputPin(int32 InputIndex) const
{
	check(InputIndex >= 0);

	for (int32 PinIndex = 0, FoundInputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Input)
		{
			if (InputIndex == FoundInputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundInputs++;
			}
		}
	}

	return nullptr;
}

UEdGraphPin* UFlowGraphNode::GetOutputPin(int32 InputIndex) const
{
	check(InputIndex >= 0);

	for (int32 PinIndex = 0, FoundInputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Output)
		{
			if (InputIndex == FoundInputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundInputs++;
			}
		}
	}

	return nullptr;
}

UFlowGraph* UFlowGraphNode::GetFlowGraph() const
{
	return CastChecked<UFlowGraph>(GetGraph());
}

bool UFlowGraphNode::IsSubNode() const
{
	return bIsSubNode || (ParentNode != nullptr);
}

void UFlowGraphNode::NodeConnectionListChanged()
{
	Super::NodeConnectionListChanged();

	if (UFlowGraph* Graph = GetFlowGraph())
	{
		Graph->GetFlowAsset()->HarvestNodeConnections(Cast<UFlowNode>(GetFlowNodeBase()));
		Graph->NotifyNodeChanged(this);
	}

	bNeedsFullReconstruction = true;
	ReconstructNode();
}

FString UFlowGraphNode::GetPropertyNameAndValueForDiff(const FProperty* Prop, const uint8* PropertyAddr) const
{
	return BlueprintNodeHelpers::DescribeProperty(Prop, PropertyAddr);
}

void UFlowGraphNode::SetParentNodeForSubNode(UFlowGraphNode* InParentNode)
{
	if (InParentNode)
	{
		// Once a SubNode, always a SubNode
		bIsSubNode = true;
	}

	ParentNode = InParentNode;
}

void UFlowGraphNode::RebuildRuntimeAddOnsFromEditorSubNodes()
{
	// Whenever we change the SubNodes array, we need to mirror the changes
	// across to the AddOns array in the runtime instance data

	if (IsValid(NodeInstance))
	{
		TArray<UFlowNodeAddOn*>& NodeInstanceAddOns = NodeInstance->GetFlowNodeAddOnChildrenByEditor();
		NodeInstanceAddOns.Reset();
		NodeInstanceAddOns.Reserve(SubNodes.Num());

		for (const UFlowGraphNode* SubNode : SubNodes)
		{
			if (!IsValid(SubNode))
			{
				LogError(FString::Printf(TEXT("%s: Has unexpectedly null SubNode"), *GetName()), NodeInstance);

				continue;
			}

			// Add the runtime AddOn to its runtime UFlowNode or UFlowNodeAddOn container
			UFlowNodeAddOn* AddOnSubNodeInstance = Cast<UFlowNodeAddOn>(SubNode->NodeInstance);
			if (IsValid(AddOnSubNodeInstance))
			{
				NodeInstanceAddOns.AddUnique(AddOnSubNodeInstance);
			}
			else
			{
				LogError(FString::Printf(TEXT("%s: SubNode is missing an AddOn NodeInstance"), *GetName()), NodeInstance);
			}
		}
	}

	// Update the SubNodes as well
	for (UFlowGraphNode* SubNode : SubNodes)
	{
		if (IsValid(SubNode))
		{
			SubNode->RebuildRuntimeAddOnsFromEditorSubNodes();
		}
	}

	// Reconstruct the context pins for all flow nodes after their AddOns have been processed
	if (IsValid(NodeInstance) && NodeInstance->IsA<UFlowNode>())
	{
		ReconstructNode();
	}
}

void UFlowGraphNode::FindDiffs(UEdGraphNode* OtherNode, FDiffResults& Results)
{
	Super::FindDiffs(OtherNode, Results);

	const UFlowGraphNode* OtherGraphNode = Cast<UFlowGraphNode>(OtherNode);
	if (!IsValid(OtherGraphNode))
	{
		return;
	}

	if (NodeInstance && OtherGraphNode->NodeInstance)
	{
		FDiffSingleResult Diff;
		Diff.Diff = EDiffType::NODE_PROPERTY;
		Diff.Node1 = this;
		Diff.Node2 = OtherNode;
		Diff.Object1 = NodeInstance;
		Diff.Object2 = OtherGraphNode->NodeInstance;
		Diff.ToolTip = LOCTEXT("DIF_NodeInstancePropertyToolTip", "A property of the node instance has changed");
		Diff.Category = EDiffType::MODIFICATION;

		DiffProperties(NodeInstance->GetClass(), OtherGraphNode->NodeInstance->GetClass(), NodeInstance, OtherGraphNode->NodeInstance, Results, Diff);
	}

	DiffSubNodes(LOCTEXT("AddOnDiffDisplayName", "AddOn"), SubNodes, OtherGraphNode->SubNodes, Results);
}

void UFlowGraphNode::DiffSubNodes(const FText& NodeTypeDisplayName, const TArray<UFlowGraphNode*>& LhsSubNodes, const TArray<UFlowGraphNode*>& RhsSubNodes, FDiffResults& Results)
{
	TArray<FGraphDiffControl::FNodeMatch> NodeMatches;
	TSet<const UEdGraphNode*> MatchedRhsNodes;

	FGraphDiffControl::FNodeDiffContext AdditiveDiffContext;
	AdditiveDiffContext.NodeTypeDisplayName = NodeTypeDisplayName;
	AdditiveDiffContext.bIsRootNode = false;

	// march through the all the nodes in the rhs and look for matches
	for (UEdGraphNode* RhsSubNode : RhsSubNodes)
	{
		FGraphDiffControl::FNodeMatch NodeMatch;
		NodeMatch.NewNode = RhsSubNode;

		// Do two passes, exact and soft
		for (UEdGraphNode* LhsSubNode : LhsSubNodes)
		{
			if (FGraphDiffControl::IsNodeMatch(LhsSubNode, RhsSubNode, true, &NodeMatches))
			{
				NodeMatch.OldNode = LhsSubNode;
				break;
			}
		}

		if (NodeMatch.NewNode == nullptr)
		{
			for (UEdGraphNode* LhsSubNode : LhsSubNodes)
			{
				if (FGraphDiffControl::IsNodeMatch(LhsSubNode, RhsSubNode, false, &NodeMatches))
				{
					NodeMatch.OldNode = LhsSubNode;
					break;
				}
			}
		}

		// if we found a corresponding node in the lhs graph, track it (so we can prevent future matches with the same nodes)
		if (NodeMatch.IsValid())
		{
			NodeMatches.Add(NodeMatch);
			MatchedRhsNodes.Add(NodeMatch.OldNode);
		}

		NodeMatch.Diff(AdditiveDiffContext, Results);
	}

	FGraphDiffControl::FNodeDiffContext SubtractiveDiffContext = AdditiveDiffContext;
	SubtractiveDiffContext.DiffMode = FGraphDiffControl::EDiffMode::Subtractive;
	SubtractiveDiffContext.DiffFlags = FGraphDiffControl::EDiffFlags::NodeExistance;

	// go through the lhs nodes to catch ones that may have been missing from the rhs graph
	for (UEdGraphNode* LhsSubNode : LhsSubNodes)
	{
		// if this node has already been matched, move on
		if (!LhsSubNode || MatchedRhsNodes.Find(LhsSubNode))
		{
			continue;
		}

		// There can't be a matching node in RhsGraph because it would have been found above
		FGraphDiffControl::FNodeMatch NodeMatch;
		NodeMatch.NewNode = LhsSubNode;

		NodeMatch.Diff(SubtractiveDiffContext, Results);
	}
}

void UFlowGraphNode::AddSubNode(UFlowGraphNode* SubNode, class UEdGraph* ParentGraph)
{
	const FScopedTransaction Transaction(LOCTEXT("AddNode", "Add Node"));
	ParentGraph->Modify();
	Modify();

	SubNode->SetFlags(RF_Transactional);

	// set outer to be the graph so it doesn't go away
	SubNode->Rename(nullptr, ParentGraph, REN_NonTransactional);
	SubNode->SetParentNodeForSubNode(this);

	SubNode->CreateNewGuid();
	SubNode->PostPlacedNewNode();
	SubNode->AllocateDefaultPins();
	SubNode->AutowireNewNode(nullptr);

	SubNode->NodePosX = 0;
	SubNode->NodePosY = 0;

	SubNodes.Add(SubNode);
	OnSubNodeAdded(SubNode);

	ParentGraph->NotifyGraphChanged();
	GetFlowGraph()->UpdateAsset();

	// NOTE - We do not need to RebuildRuntimeAddOnsFromEditorSubNodes here, because UpdateAsset() will do it
}

void UFlowGraphNode::OnSubNodeAdded(UFlowGraphNode* SubNode)
{
	// Empty in base class
}

void UFlowGraphNode::RemoveSubNode(UFlowGraphNode* SubNode)
{
	Modify();
	SubNodes.RemoveSingle(SubNode);

	RebuildRuntimeAddOnsFromEditorSubNodes();

	OnSubNodeRemoved(SubNode);
}

void UFlowGraphNode::RemoveAllSubNodes()
{
	SubNodes.Reset();

	RebuildRuntimeAddOnsFromEditorSubNodes();
}

void UFlowGraphNode::OnSubNodeRemoved(UFlowGraphNode* SubNode)
{
	// Empty in base class
}

int32 UFlowGraphNode::FindSubNodeDropIndex(UFlowGraphNode* SubNode) const
{
	const int32 InsertIndex = SubNodes.IndexOfByKey(SubNode);
	return InsertIndex;
}

void UFlowGraphNode::InsertSubNodeAt(UFlowGraphNode* SubNode, const int32 DropIndex)
{
	if (DropIndex > -1)
	{
		SubNodes.Insert(SubNode, DropIndex);
	}
	else
	{
		SubNodes.Add(SubNode);
	}

	RebuildRuntimeAddOnsFromEditorSubNodes();
}

void UFlowGraphNode::DestroyNode()
{
	if (NodeInstance)
	{
		if (UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance))
		{
			if (FlowNode->OnPropertyChangedEvent.IsBound())
			{
				FlowNode->OnPropertyChangedEvent.Unbind();
			}
		}
	}
	
	bIsDestroyingNode = true;

	if (ParentNode)
	{
		ParentNode->RemoveSubNode(this);

		ParentNode->RebuildRuntimeAddOnsFromEditorSubNodes();
	}
	else
	{
		RebuildRuntimeAddOnsFromEditorSubNodes();
	}

	UEdGraphNode::DestroyNode();

	bIsDestroyingNode = false;
}

bool UFlowGraphNode::UsesBlueprint() const
{
	return NodeInstance && NodeInstance->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
}

bool UFlowGraphNode::RefreshNodeClass()
{
	bool bUpdated = false;
	if (NodeInstance == nullptr)
	{
		if (NodeInstanceClass.IsPending())
		{
			NodeInstanceClass.LoadSynchronous();
		}

		if (NodeInstanceClass.IsValid())
		{
			PostPlacedNewNode();

			bUpdated = (NodeInstance != nullptr);
		}
	}

	return bUpdated;
}

void UFlowGraphNode::UpdateNodeClassData()
{
	if (NodeInstance)
	{
		NodeInstanceClass = NodeInstance->GetClass();
	}
}

bool UFlowGraphNode::HasErrors() const
{
	return ErrorMessage.Len() > 0 || !IsValid(NodeInstance);
}

void UFlowGraphNode::ValidateGraphNode(FFlowMessageLog& MessageLog) const
{
	// Verify that all input data pin connections are legal

	if (!NodeInstance)
	{
		// Missing the node instance!
		MessageLog.Error<UFlowNode>(TEXT("FlowGraphNode is missing its UFlowNode instance!"), nullptr);
		return;
	}

	const UFlowGraphSchema* Schema = CastChecked<UFlowGraphSchema>(GetSchema());
	for (const UEdGraphPin* EdGraphPin : InputPins)
	{
		if (EdGraphPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}

		if (!EdGraphPin->HasAnyConnections())
		{
			continue;
		}

		for (UEdGraphPin* const ConnectedPin : EdGraphPin->LinkedTo)
		{
			const FPinConnectionResponse Response = Schema->CanCreateConnection(ConnectedPin, EdGraphPin);

			if (!Response.CanSafeConnect())
			{
				MessageLog.Error<UFlowNodeBase>(*FString::Printf(TEXT("Pin %s has invalid connection: %s"), *EdGraphPin->GetName(), *Response.Message.ToString()), NodeInstance);
			}
		}
	}
}

TMap<uint8, FPinRecord> UFlowGraphNode::GetWireRecords() const
{
	TMap<uint8, FPinRecord> Result;
	const UFlowAsset* OwningAssetTemplate = GetFlowAsset();
	const UFlowAsset* InspectedInstance = OwningAssetTemplate ? OwningAssetTemplate->GetInspectedInstance() : nullptr;
	if (!InspectedInstance)
	{
		return Result;
	}
	
	const UFlowNode* InspectedRuntimeNode = InspectedInstance->GetNode(this->NodeGuid);
	if (!InspectedRuntimeNode)
	{
		return Result;
	}

	const TMap<FName, TArray<FPinRecord>>& OutputPinRecords = InspectedRuntimeNode->OutputRecords;
	for (const TPair<FName, TArray<FPinRecord>>& RecordPair : OutputPinRecords)
	{
		const FName PinName = RecordPair.Key;
		if (RecordPair.Value.IsEmpty())
		{
			continue;
		}

		int32 VisualPinIndex = INDEX_NONE;
		for (int32 i = 0; i < Pins.Num(); ++i)
		{
			const UEdGraphPin* VisualPin = Pins[i];
			if (VisualPin && VisualPin->Direction == EGPD_Output && VisualPin->PinName == PinName)
			{
				VisualPinIndex = i;
				break;
			}
		}

		if (VisualPinIndex != INDEX_NONE)
		{
			Result.Emplace(static_cast<uint8>(VisualPinIndex), RecordPair.Value.Last());
		}
	}
	return Result;
}

bool UFlowGraphNode::CanReconstructNode() const
{
	// Global states that should prevent ReconstructNode from running
	if (GIsTransacting || bIsReconstructingNode || bIsDestroyingNode)
	{
		return false;
	}

	// This should never happen
	if (!ensureMsgf(IsValid(NodeInstance), TEXT("FlowGraphNode has no NodeInstance, graph may be corrupt! Flow Asset: %s"), *GetFlowAsset()->GetName()))
	{
		return false;
	}

	// This should never happen
	if (!ensureMsgf(IsValid(GetGraph()), TEXT("FlowGraphNode has no owner graph, graph may be corrupt! Flow Node Instance: %s"), *NodeInstance->GetName()))
	{
		return false;
	}

	// Don't do anything if the Flow Graph is preventing it
	if (const UFlowGraph* FlowGraph = GetFlowGraph())
	{
		if (FlowGraph->IsSavingGraph())
		{
			return false;
		}

		if (FlowGraph->IsLocked())
		{
			return false;
		}
	}

	return true;
}

bool UFlowGraphNode::GatherAllExpectedPins(TArray<FFlowPin>& OutInputPins, TArray<FFlowPin>& OutOutputPins) const
{
	OutInputPins.Empty();
	OutOutputPins.Empty();
	TSet<FName> AddedPinNames;

	const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (!FlowNode || !NodeInstance || !NodeInstance->GetClass() || !K2Schema)
	{
		return false;
	}

	const UClass* InstanceClass = NodeInstance->GetClass();
	const bool bIsPureNode = FlowNode->IsPureNode();

	// Helper lambda to attempt adding a pin.
	auto TryAddPin = [&](const FFlowPin& Pin, const bool bIsInput) {
		if (Pin.IsValid() && !AddedPinNames.Contains(Pin.PinName))
		{
			(bIsInput ? OutInputPins : OutOutputPins).Add(Pin);
			AddedPinNames.Add(Pin.PinName);
		}
	};

	// Helper lambda to process a list of pins.
	auto ProcessPinList = [&](const TArray<FFlowPin>& PinsToAdd, const bool bIsInputPinList) {
		for (const FFlowPin& Pin : PinsToAdd)
		{
			TryAddPin(Pin, bIsInputPinList);
		}
	};

	// Explicit pins for non-pure nodes (defined directly on UFlowNode)
	if (!bIsPureNode)
	{
		ProcessPinList(FlowNode->GetInputPins(), true);
		ProcessPinList(FlowNode->GetOutputPins(), false);
	}

	// Helper lambda to create and add a data pin from an FProperty.
	auto ProcessPropertyToPin = [&](FProperty* Property, bool bIsInputProperty, const FName& ExplicitPinName = NAME_None) {
		const FName PinName = ExplicitPinName.IsNone() ? Property->GetFName() : ExplicitPinName;
		if (!AddedPinNames.Contains(PinName))
		{
			if (FEdGraphPinType PinType; K2Schema->ConvertPropertyToPinType(Property, PinType))
			{
				FFlowPin DataPin(PinName);
				DataPin.SetPinType(PinType);
				DataPin.PinFriendlyName = Property->GetDisplayNameText();
				DataPin.PinToolTip = Property->GetToolTipText().ToString();
				TryAddPin(DataPin, bIsInputProperty);
			}
		}
	};

	// Data pins from properties (Blueprint-exposed or C++ with metadata)
	if (const UFlowNodeBaseBlueprintGeneratedClass* FlowBPClass = Cast<const UFlowNodeBaseBlueprintGeneratedClass>(InstanceClass))
	{
		for (const TPair<FName, FFlowVarConfig>& VarPair : FlowBPClass->FlowVarSettings)
		{
			if (VarPair.Value.bIsDataPin && !AddedPinNames.Contains(VarPair.Key))
			{
				if (const FProperty* Property = FindFProperty<FProperty>(InstanceClass, VarPair.Key))
				{
					ProcessPropertyToPin(const_cast<FProperty*>(Property), VarPair.Value.IsInputPin(), VarPair.Key);
				}
			}
		}
	}
	else // C++
	{
		for (TFieldIterator<FProperty> PropIt(InstanceClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property)
			{
				continue;
			}

			const FName PropertyName = Property->GetFName();
			if (Property->HasMetaData(FFlowPin::MetadataKey_FlowDataPin))
			{
				if (!AddedPinNames.Contains(PropertyName))
				{
					const bool bIsInput = Property->GetMetaData(FFlowPin::MetadataKey_FlowDataPin).Equals(TEXT("Input"), ESearchCase::IgnoreCase);
					ProcessPropertyToPin(Property, bIsInput);
				}
			}
			// Check for property bag metadata
			else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
			{
				if (StructProp->Struct == FInstancedPropertyBag::StaticStruct() && Property->HasMetaData(FFlowPin::MetadataKey_FlowPropertyBag))
				{
					// Property bag pins are named by their inner properties, not the bag property itself.
					// So, the AddedPinNames check for PropertyName is not directly applicable here for skipping the whole bag.
					const FString BagDirectionStr = Property->GetMetaData(FFlowPin::MetadataKey_FlowPropertyBag);
					const bool bIsInputBag = BagDirectionStr.Equals(TEXT("Input"), ESearchCase::IgnoreCase);

					if (const FInstancedPropertyBag* BagPtr = StructProp->ContainerPtrToValuePtr<FInstancedPropertyBag>(NodeInstance))
					{
						if (const UPropertyBag* BagStruct = BagPtr->GetPropertyBagStruct())
						{
							for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
							{
								if (Desc.ValueType != EPropertyBagPropertyType::None && !AddedPinNames.Contains(Desc.Name))
								{
									FEdGraphPinType PinType = GetPropertyDescAsPin(Desc);
									FFlowPin PinDef(Desc.Name);
									PinDef.SetPinType(PinType);
									PinDef.PinFriendlyName = FText::FromName(Desc.Name);
									TryAddPin(PinDef, bIsInputBag);
								}
							}
						}
					}
				}
			}
		}
	}

	// Add dynamic context pins (from IFlowContextPinSupplierInterface)
	if (const IFlowContextPinSupplierInterface* ContextSupplier = Cast<IFlowContextPinSupplierInterface>(NodeInstance))
	{
		if (ContextSupplier->SupportsContextPins())
		{
			ProcessPinList(ContextSupplier->GetContextInputs(), true);
			ProcessPinList(ContextSupplier->GetContextOutputs(), false);
		}
	}

	// Add dynamic pins declared by the node instance itself
	if (FlowNode)
	{
		TArray<FFlowPin> DynamicInputsFromNode;
		TArray<FFlowPin> DynamicOutputsFromNode;
		FlowNode->GetDynamicPins(DynamicInputsFromNode, DynamicOutputsFromNode);

		ProcessPinList(DynamicInputsFromNode, true);
		ProcessPinList(DynamicOutputsFromNode, false);
	}

	// Sort pins if configured
	const UFlowGraphEditorSettings* Settings = UFlowGraphEditorSettings::Get();
	if (Settings && Settings->bSortExecPinsFirst)
	{
		auto SortPinsLambda = [Settings](const FFlowPin& A, const FFlowPin& B) {
			const bool bAIsExec = A.PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
			const bool bBIsExec = B.PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;

			if (bAIsExec != bBIsExec)
			{
				return bAIsExec;
			}

			if (Settings->bSortPinsByName)
			{
				return A.PinName.LexicalLess(B.PinName);
			}
			return false;
		};

		OutInputPins.Sort(SortPinsLambda);
		OutOutputPins.Sort(SortPinsLambda);
	}

	return true;
}

void UFlowGraphNode::SpecializeNewlyAllocatedPinTypes(const TArray<UEdGraphPin*>& OldVisualPins)
{
	if (!NodeInstance)
	{
		return;
	}
	const UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);

	// We need to know which pins are dynamic, according to the Flow Node,
	// to avoid trying to specialize non-dynamic wildcards (if any existed for other reasons).
	TArray<FFlowPin> DynamicInputDefinitionsFromNode;
	TArray<FFlowPin> DynamicOutputDefinitionsFromNode;
	FlowNode->GetDynamicPins(DynamicInputDefinitionsFromNode, DynamicOutputDefinitionsFromNode);

	TSet<FName> DynamicNodeInputPinNames;
	for (const FFlowPin& PinDef : DynamicInputDefinitionsFromNode)
	{
		DynamicNodeInputPinNames.Add(PinDef.PinName);
	}
	TSet<FName> DynamicNodeOutputPinNames;
	for (const FFlowPin& PinDef : DynamicOutputDefinitionsFromNode)
	{
		DynamicNodeOutputPinNames.Add(PinDef.PinName);
	}

	for (UEdGraphPin* NewPin : Pins)
	{
		bool bIsDynamicAndWildcard = false;
		if (NewPin->Direction == EGPD_Input && DynamicNodeInputPinNames.Contains(NewPin->PinName) && NewPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
		{
			bIsDynamicAndWildcard = true;
		}
		else if (NewPin->Direction == EGPD_Output && DynamicNodeOutputPinNames.Contains(NewPin->PinName) && NewPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
		{
			bIsDynamicAndWildcard = true;
		}
		if (bIsDynamicAndWildcard)
		{
			const UEdGraphPin* OldMatchingPin = nullptr;
			for (const UEdGraphPin* OldPin : OldVisualPins)
			{
				if (OldPin && OldPin->PinName == NewPin->PinName && OldPin->Direction == NewPin->Direction)
				{
					OldMatchingPin = OldPin;
					break;
				}
			}

			if (OldMatchingPin && !OldMatchingPin->LinkedTo.IsEmpty() && OldMatchingPin->LinkedTo[0] != nullptr)
			{
				if (NewPin->PinType != OldMatchingPin->LinkedTo[0]->PinType)
				{
					NewPin->Modify();
					NewPin->PinType = OldMatchingPin->LinkedTo[0]->PinType;
					GetSchema()->ResetPinToAutogeneratedDefaultValue(NewPin, false);
				}
			}
		}
	}
}

void UFlowGraphNode::SyncPropertyValueToPin(UEdGraphPin* Pin) const
{
	if (!Pin || !NodeInstance || Pin->Direction != EGPD_Input)
	{
		return;
	}

	if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		return;
	}

	UFlowNode* FlowNode = Cast<UFlowNode>(NodeInstance);
	if (!FlowNode)
	{
		return;
	}

	const FProperty* BoundProp = FlowNode->GetInputProperty(Pin->PinName);
	if (!BoundProp)
	{
		FlowNode->CachePinProperties();
		BoundProp = FlowNode->GetInputProperty(Pin->PinName);
		if (!BoundProp)
		{
			return;
		}
	}

	const auto Container = FlowNode->GetPropertyContainer(BoundProp);
	const uint8* PropertyDataPtr = BoundProp->ContainerPtrToValuePtr<uint8>(Container);
	if (!PropertyDataPtr)
	{
		return;
	}

	FString PropertyValueAsString;
	BoundProp->ExportTextItem_Direct(PropertyValueAsString, PropertyDataPtr, PropertyDataPtr, nullptr, PPF_None);
	if (!PropertyValueAsString.IsEmpty())
	{
		GetDefault<UEdGraphSchema_K2>()->SetPinDefaultValueAtConstruction(Pin, PropertyValueAsString);
	}
}

void UFlowGraphNode::SyncPinDefaultValueFromProperty(const FProperty* Property)
{
	if (!Property || !NodeInstance)
	{
		return;
	}

	// Find the pin that corresponds to this property's name. We only care about inputs for default values.
	UEdGraphPin* PinToUpdate = FindPin(Property->GetFName(), EGPD_Input);
	if (PinToUpdate && PinToUpdate->LinkedTo.Num() == 0)
	{
		const uint8* PropertyAddr = Property->ContainerPtrToValuePtr<uint8>(NodeInstance);
		FString PropertyValueAsString;
		Property->ExportTextItem_Direct(PropertyValueAsString, PropertyAddr, PropertyAddr, nullptr, PPF_None);
		if (PinToUpdate->GetDefaultAsString() != PropertyValueAsString)
		{
			const UEdGraphSchema* Schema = GetSchema();
			if (Schema)
			{
				Schema->TrySetDefaultValue(*PinToUpdate, PropertyValueAsString);
			}
		}
	}
}

void UFlowGraphNode::OnNodeInstancePropertyChanged(const FProperty* PropertyChanged)
{
	SyncPinDefaultValueFromProperty(PropertyChanged);
}

void UFlowGraphNode::GetExpectedPinSignatures(TSet<FFlowPinSignature>& OutExpectedPins) const
{
	OutExpectedPins.Empty();
	TArray<FFlowPin> ExpectedInputs;
	TArray<FFlowPin> ExpectedOutputs;

	if (GatherAllExpectedPins(ExpectedInputs, ExpectedOutputs))
	{
		for (const FFlowPin& Pin : ExpectedInputs)
		{
			OutExpectedPins.Add(FFlowPinSignature(Pin.PinName, Pin.PinType, EGPD_Input));
		}
		for (const FFlowPin& Pin : ExpectedOutputs)
		{
			OutExpectedPins.Add(FFlowPinSignature(Pin.PinName, Pin.PinType, EGPD_Output));
		}
	}
}

bool UFlowGraphNode::DoVisualPinsMatchExpected(const TSet<FFlowPinSignature>& ExpectedPins) const
{
	TSet<FFlowPinSignature> CurrentVisualPins;
	for (const UEdGraphPin* Pin : Pins)
	{
		if (Pin && !Pin->bOrphanedPin)
		{
			CurrentVisualPins.Add(FFlowPinSignature(Pin->PinName, Pin->PinType, Pin->Direction));
		}
	}

	if (CurrentVisualPins.Num() != ExpectedPins.Num())
	{
		return false;
	}

	for (const FFlowPinSignature& ExpectedSig : ExpectedPins)
	{
		if (!CurrentVisualPins.Contains(ExpectedSig))
		{
			return false;
		}
	}

	return true;
}

FEdGraphPinType UFlowGraphNode::GetPropertyDescAsPin(const FPropertyBagPropertyDesc& Desc)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	check(K2Schema);

	if (FEdGraphPinType PinType; const FProperty* Property = Desc.CachedProperty)
	{
		if (K2Schema->ConvertPropertyToPinType(Property, PinType))
		{
			return PinType;
		}
	}

	return FEdGraphPinType();
}

bool UFlowGraphNode::IsAncestorNode(const UFlowGraphNode& OtherNode) const
{
	const UFlowGraphNode* CurParentNode = ParentNode;
	while (CurParentNode)
	{
		if (CurParentNode == &OtherNode)
		{
			return true;
		}

		CurParentNode = CurParentNode->ParentNode;
	}

	return false;
}

void UFlowGraphNode::RebuildPinArrays()
{
	InputPins.Empty(Pins.Num());
	OutputPins.Empty(Pins.Num());
	for (UEdGraphPin* Pin : Pins)
	{
		switch (Pin->Direction)
		{
			case EGPD_Input:
				InputPins.Add(Pin);
				break;
			case EGPD_Output:
				OutputPins.Add(Pin);
				break;
			default:
				UE_LOG(LogFlowEditor, Error, TEXT("Encountered Pin with invalid direction!"));
		}
	}
}

bool UFlowGraphNode::CanAcceptSubNodeAsChild(const UFlowGraphNode& OtherSubNode, const TSet<const UEdGraphNode*>& AllRootSubNodesToPaste, FString* OutReasonString) const
{
	const UFlowNodeBase* OtherFlowNodeSubNode = OtherSubNode.NodeInstance;

	if (!OtherFlowNodeSubNode)
	{
		if (OutReasonString)
		{
			*OutReasonString = TEXT("Editor node is missing a runtime AddOn instance");
		}

		return false;
	}

	if (IsAncestorNode(OtherSubNode))
	{
		if (OutReasonString)
		{
			*OutReasonString = TEXT("Cannot be a AddOn of one of our own AddOns");
		}

		return false;
	}

	check(OtherFlowNodeSubNode);
	const UFlowNodeAddOn* AddOnToConsider = Cast<UFlowNodeAddOn>(OtherFlowNodeSubNode);

	// Build the array of other root AddOns that will also be added as children as an atomic operation (eg, multi-paste)
	TArray<UFlowNodeAddOn*> OtherAddOnsToPaste;

	for (TSet<const UEdGraphNode*>::TConstIterator It(AllRootSubNodesToPaste); It; ++It)
	{
		const UFlowGraphNode* NodeToPaste = Cast<UFlowGraphNode>(*It);
		UFlowNodeAddOn* AddOnToPaste = Cast<UFlowNodeAddOn>(NodeToPaste->NodeInstance);

		if (IsValid(AddOnToPaste) && AddOnToPaste != AddOnToConsider)
		{
			OtherAddOnsToPaste.Add(AddOnToPaste);
		}
	}

	const UFlowNodeBase* ThisFlowNodeBase = NodeInstance;
	const EFlowAddOnAcceptResult AcceptResult = ThisFlowNodeBase->CheckAcceptFlowNodeAddOnChild(AddOnToConsider, OtherAddOnsToPaste);

	// Undetermined and Reject both count as Rejection, only TentativeAccept is an 'accept' result

	if (AcceptResult == EFlowAddOnAcceptResult::TentativeAccept)
	{
		FLOW_ASSERT_ENUM_MAX(EFlowAddOnAcceptResult, 3);

		return true;
	}

	if (OutReasonString)
	{
		*OutReasonString = FString::Printf(TEXT("%s cannot accept AddOn type %s"), *ThisFlowNodeBase->GetClass()->GetName(), *OtherFlowNodeSubNode->GetClass()->GetName());
	}

	return false;
}

#undef LOCTEXT_NAMESPACE