// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "K2Nodes/K2Node_StartFlow.h"

#include "FlowBlueprintFunctionLibrary.h"
#include "FlowAsset.h"
#include "Nodes/Graph/FlowNode_DefineProperties.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "FlowEditorStyle.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "StructUtils/PropertyBag.h"

#define LOCTEXT_NAMESPACE "K2Node_StartFlow"

/** Helper function to convert a Property Bag Descriptor's type to an FEdGraphPinType. */
static FEdGraphPinType GetPropertyDescAsPinType(const FPropertyBagPropertyDesc& Desc)
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

UK2Node_StartFlow::UK2Node_StartFlow()
{
}

UK2Node_StartFlow::~UK2Node_StartFlow()
{
	UnsubscribeFromAssetChanges();
}

void UK2Node_StartFlow::PostLoad()
{
	Super::PostLoad();
	SubscribeToAssetChanges();
}

void UK2Node_StartFlow::DestroyNode()
{
	UnsubscribeFromAssetChanges();
	Super::DestroyNode();
}

FText UK2Node_StartFlow::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Start Flow");
}

FText UK2Node_StartFlow::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Starts a Flow Graph with the specified parameters.");
}

FSlateIcon UK2Node_StartFlow::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIcon(FFlowEditorStyle::GetStyleSetName(), "ClassIcon.FlowAsset");
}

FText UK2Node_StartFlow::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "Flow");
}

void UK2Node_StartFlow::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

UObject* UK2Node_StartFlow::GetJumpTargetForDoubleClick() const
{
	return GetSelectedFlowAsset();
}

void UK2Node_StartFlow::PreloadRequiredAssets()
{
	if (GetSelectedFlowAsset())
	{
		PreloadObject(GetSelectedFlowAsset());
	}

	Super::PreloadRequiredAssets();
}

FName UK2Node_StartFlow::GetFlowAssetPinName()
{
	return TEXT("FlowAsset");
}

void UK2Node_StartFlow::AllocateDefaultPins()
{
    Super::AllocateDefaultPins();

    // Execution pins
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

    // Static input pins
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UObject::StaticClass(), TEXT("Owner"));
    UEdGraphPin* FlowAssetPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UFlowAsset::StaticClass(), GetFlowAssetPinName());
    FlowAssetPin->PinToolTip = LOCTEXT("FlowAssetPinTooltip", "The Flow Asset to start.").ToString();
    FlowAssetPin->bNotConnectable = true;
	
    if (FlowAssetTemplate)
    {
        if (UFlowNode_DefineProperties* EntryNode = GetFlowEntryNode())
        {
            if (EntryNode->HasAnyFlags(RF_NeedLoad))
            {
                EntryNode->GetLinker()->Preload(EntryNode);
            }
            
            const FInstancedPropertyBag& Bag = EntryNode->GetOutputProperties();
            if (const UPropertyBag* BagStruct = Bag.GetPropertyBagStruct())
            {
                if (BagStruct->HasAnyFlags(RF_NeedLoad))
                {
                    BagStruct->GetLinker()->Preload(const_cast<UPropertyBag*>(BagStruct));
                }
                
                for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
                {
                    if (Desc.ValueType != EPropertyBagPropertyType::None)
                    {
                        FEdGraphPinType PinType = GetPropertyDescAsPinType(Desc);
                        UEdGraphPin* NewPin = CreatePin(EGPD_Input, PinType, Desc.Name);
                        NewPin->PinToolTip = FString::Printf(TEXT("Parameter: %s (must be connected)"), *Desc.Name.ToString());
                        NewPin->bDefaultValueIsIgnored = true;
                    }
                }
            }
        }
    }

    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Object, UFlowAsset::StaticClass(), UEdGraphSchema_K2::PN_ReturnValue)->PinToolTip = LOCTEXT("ReturnValuePinTooltip", "The runtime instance of the started Flow Asset.").ToString();
}

void UK2Node_StartFlow::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	TMap<FName, UEdGraphPin*> OldPinMap;
	for (UEdGraphPin* OldPin : OldPins)
	{
		OldPinMap.Add(OldPin->PinName, OldPin);
	}

	AllocateDefaultPins();
	TArray<UEdGraphPin*> NewPinsCopy = Pins;
	for (UEdGraphPin* NewPin : NewPinsCopy)
	{
		if (UEdGraphPin** OldPinPtr = OldPinMap.Find(NewPin->PinName))
		{
			UEdGraphPin* OldPin = *OldPinPtr;
			
			if (NewPin->PinType == OldPin->PinType)
			{
				NewPin->MovePersistentDataFromOldPin(*OldPin);
			}
			
			OldPinMap.Remove(NewPin->PinName);
		}
	}
	
	for (const auto& Pair : OldPinMap)
	{
		UEdGraphPin* OldPin = Pair.Value;
		if (OldPin->LinkedTo.Num() > 0)
		{
			OldPin->bOrphanedPin = true;
			Pins.Add(OldPin);
		}
	}
}

void UK2Node_StartFlow::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);

	if (Pin && Pin->PinName == GetFlowAssetPinName())
	{
		SetSelectedFlowAsset(Cast<UFlowAsset>(Pin->DefaultObject));
	}
}

void UK2Node_StartFlow::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UK2Node_StartFlow, FlowAssetTemplate))
	{
		UnsubscribeFromAssetChanges();
		SubscribeToAssetChanges();
		
		ReconstructNode();
	}
}

void UK2Node_StartFlow::ReconstructNode()
{
	if (FlowAssetTemplate && FlowAssetTemplate->HasAnyFlags(RF_NeedLoad))
	{
		FlowAssetTemplate->GetLinker()->Preload(FlowAssetTemplate);
	}

	Super::ReconstructNode();
}

UFlowAsset* UK2Node_StartFlow::GetSelectedFlowAsset() const
{
	UEdGraphPin* FlowAssetPin = FindPin(GetFlowAssetPinName());
	if (FlowAssetPin && FlowAssetPin->DefaultObject)
	{
		return Cast<UFlowAsset>(FlowAssetPin->DefaultObject);
	}
	return FlowAssetTemplate;
}

UFlowNode_DefineProperties* UK2Node_StartFlow::GetFlowEntryNode() const
{
	if (const UFlowAsset* Asset = GetSelectedFlowAsset())
	{
		if (Asset->HasAnyFlags(RF_NeedLoad))
		{
			Asset->GetLinker()->Preload(const_cast<UFlowAsset*>(Asset));
		}
		
		return Cast<UFlowNode_DefineProperties>(Asset->GetDefaultEntryNode());
	}
	return nullptr;
}

void UK2Node_StartFlow::SubscribeToAssetChanges()
{
	if (FlowAssetTemplate)
	{
		if (!OnSubGraphReconstructionRequestedHandle.IsValid())
		{
			OnSubGraphReconstructionRequestedHandle = FlowAssetTemplate->OnSubGraphReconstructionRequested.AddUObject(this, &UK2Node_StartFlow::OnAssetChanged);
		}
	}
}

void UK2Node_StartFlow::UnsubscribeFromAssetChanges()
{
	if (OnSubGraphReconstructionRequestedHandle.IsValid())
	{
		if (FlowAssetTemplate)
		{
			FlowAssetTemplate->OnSubGraphReconstructionRequested.Remove(OnSubGraphReconstructionRequestedHandle);
		}
		OnSubGraphReconstructionRequestedHandle.Reset();
	}
}

void UK2Node_StartFlow::OnAssetChanged()
{
	ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
}

void UK2Node_StartFlow::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	const UFlowAsset* Asset = GetSelectedFlowAsset();
	if (!Asset)
	{
		for (const UEdGraphPin* Pin : Pins)
		{
			if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec && Pin->PinName != GetFlowAssetPinName() && Pin->PinName != "Owner" && Pin->LinkedTo.Num() > 0)
			{
				MessageLog.Error(*LOCTEXT("NoAssetWithParametersError", "Node @@ has parameter connections but no Flow Asset is selected.").ToString(), this);
				break;
			}
		}
	}
	else
	{
		const UFlowNode_DefineProperties* EntryNode = GetFlowEntryNode();
		const FInstancedPropertyBag* Bag = EntryNode ? &EntryNode->GetOutputProperties() : nullptr;
		for (const UEdGraphPin* Pin : Pins)
		{
			if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec && Pin->PinName != GetFlowAssetPinName() && Pin->PinName != "Owner" && Pin->LinkedTo.Num() > 0)
			{
				if (!Bag || !Bag->FindPropertyDescByName(Pin->PinName))
				{
					MessageLog.Error(*FText::Format(LOCTEXT("ParameterNotFoundError", "Parameter pin '{0}' on node @@ is no longer present in the selected Flow Asset."), FText::FromName(Pin->PinName)).ToString(), this);
				}
			}
		}
	}
}

void UK2Node_StartFlow::SetSelectedFlowAsset(UFlowAsset* NewAsset)
{
	if (FlowAssetTemplate != NewAsset)
	{
		const FScopedTransaction Transaction(LOCTEXT("ChangeFlowAsset", "Change Flow Asset"));
		Modify();
        
		UnsubscribeFromAssetChanges();
		FlowAssetTemplate = NewAsset;
		SubscribeToAssetChanges();

		if (UEdGraphPin* FlowAssetPin = FindPin(GetFlowAssetPinName()))
		{
			FlowAssetPin->DefaultObject = NewAsset;
		}

		ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
	}
}

void UK2Node_StartFlow::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    Super::ExpandNode(CompilerContext, SourceGraph);

    const UEdGraphSchema_K2* K2Schema = CompilerContext.GetSchema();
    UEdGraphPin* OriginalExecPin = GetExecPin();
    UEdGraphPin* OriginalThenPin = GetThenPin();
    UEdGraphPin* OriginalFlowAssetPin = FindPinChecked(GetFlowAssetPinName());
    UEdGraphPin* OriginalOwnerPin = FindPinChecked(TEXT("Owner"));
    UEdGraphPin* OriginalReturnValuePin = FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue);

	if (OriginalOwnerPin->LinkedTo.Num() == 0)
	{
		if (UK2Node_Self* SelfNode = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(this, SourceGraph))
		{
			SelfNode->AllocateDefaultPins();
			if (UEdGraphPin* SelfPin = SelfNode->FindPin(UEdGraphSchema_K2::PSC_Self))
			{
				if (!K2Schema->TryCreateConnection(SelfPin, OriginalOwnerPin))
				{
					CompilerContext.MessageLog.Error(*LOCTEXT("SelfConnectionFailedError", "Node @@: Failed to connect 'self' context to the Owner pin.").ToString(), this);
					BreakAllNodeLinks();
					return;
				}
			}
			else
			{
				CompilerContext.MessageLog.Error(*LOCTEXT("NoSelfPinError_Intermediate", "Node @@: Spawned 'self' node has no output pin.").ToString(), this);
				BreakAllNodeLinks();
				return;
			}
		}
		else
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("SelfSpawnFailedError", "Node @@: Failed to spawn a 'self' context node.").ToString(), this);
			BreakAllNodeLinks();
			return;
		}
	}

    UFlowAsset* Asset = GetSelectedFlowAsset();
    if (!Asset || OriginalExecPin->LinkedTo.Num() == 0)
    {
        BreakAllNodeLinks();
        return;
    }

    // --- Create the initial parameter bag ---
    UK2Node_CallFunction* CreateParamsNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    CreateParamsNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFlowBlueprintFunctionLibrary, CreateFlowParameters), UFlowBlueprintFunctionLibrary::StaticClass());
    CreateParamsNode->AllocateDefaultPins();
    
    CompilerContext.MovePinLinksToIntermediate(*OriginalExecPin, *CreateParamsNode->GetExecPin());
    
    UEdGraphPin* MasterParametersPin = CreateParamsNode->GetReturnValuePin();
    UEdGraphPin* LastThenPin = CreateParamsNode->GetThenPin();

    // --- Chain setter calls ---
    if (const UFlowNode_DefineProperties* EntryNode = GetFlowEntryNode())
    {
        if (const UPropertyBag* BagStruct = EntryNode->GetOutputProperties().GetPropertyBagStruct())
        {
            for (UEdGraphPin* Pin : Pins)
            {
                if (Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() > 0 && Pin->PinName != GetFlowAssetPinName() && Pin->PinName != TEXT("Owner"))
                {
                    const FPropertyBagPropertyDesc* Desc = BagStruct->FindPropertyDescByName(Pin->PinName);
                    if (!Desc)
                    {
	                    continue;
                    }
                    UK2Node_CallFunction* SetParamNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
                    SetParamNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFlowBlueprintFunctionLibrary, K2_SetFlowParameterProperty), UFlowBlueprintFunctionLibrary::StaticClass());
                    SetParamNode->AllocateDefaultPins();

                    K2Schema->TryCreateConnection(LastThenPin, SetParamNode->GetExecPin());
                    LastThenPin = SetParamNode->GetThenPin();

                    UEdGraphPin* ParamInputPin = SetParamNode->FindPinChecked(TEXT("Parameters"), EGPD_Input);
                    K2Schema->TryCreateConnection(MasterParametersPin, ParamInputPin);

                    SetParamNode->FindPinChecked(TEXT("PropertyID"))->DefaultValue = Desc->ID.ToString();
                    UEdGraphPin* ValuePin = SetParamNode->FindPinChecked(TEXT("Value"));
                    ValuePin->PinType = Pin->PinType;
                    CompilerContext.MovePinLinksToIntermediate(*Pin, *ValuePin);
                }
            }
        }
    }

    // --- Final call to StartFlowWithParameters ---
    UK2Node_CallFunction* StartFlow_CallNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    StartFlow_CallNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFlowBlueprintFunctionLibrary, StartFlowWithParameters), UFlowBlueprintFunctionLibrary::StaticClass());
    StartFlow_CallNode->AllocateDefaultPins();

    K2Schema->TryCreateConnection(LastThenPin, StartFlow_CallNode->GetExecPin());
    CompilerContext.MovePinLinksToIntermediate(*OriginalThenPin, *StartFlow_CallNode->GetThenPin());
    
    CompilerContext.MovePinLinksToIntermediate(*OriginalOwnerPin, *StartFlow_CallNode->FindPinChecked(TEXT("Owner")));
    
    CompilerContext.CopyPinLinksToIntermediate(*OriginalFlowAssetPin, *CreateParamsNode->FindPinChecked(TEXT("FlowAsset")));
    CompilerContext.CopyPinLinksToIntermediate(*OriginalFlowAssetPin, *StartFlow_CallNode->FindPinChecked(TEXT("FlowAsset")));
    
    K2Schema->TryCreateConnection(MasterParametersPin, StartFlow_CallNode->FindPinChecked(TEXT("Parameters"))); 
    
    CompilerContext.MovePinLinksToIntermediate(*OriginalReturnValuePin, *StartFlow_CallNode->GetReturnValuePin());

    BreakAllNodeLinks();
}

#undef LOCTEXT_NAMESPACE