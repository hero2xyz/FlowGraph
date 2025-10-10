// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Nodes/Developer/FlowNode_FormatText.h"

#include "FlowAsset.h"
#include "FlowNodeMacros.h"
#include "Kismet/KismetTextLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlowNode_FormatText)

#define LOCTEXT_NAMESPACE "FlowNode_FormatText"

UFlowNode_FormatText::UFlowNode_FormatText(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
#if WITH_EDITOR
	Category = TEXT("Utilities|Text");
	NodeDisplayStyle = FlowNodeStyle::Default;
#endif

	// Example default format string
	FormatString = LOCTEXT("DefaultFormatTextNodeFormat", "Value of {Arg1} and {Arg2}");
}

#if WITH_EDITOR
void UFlowNode_FormatText::GetDynamicPins(TArray<FFlowPin>& OutDynamicInputPins, TArray<FFlowPin>& OutDynamicOutputPins) const
{
	Super::GetDynamicPins(OutDynamicInputPins, OutDynamicOutputPins);
	const TArray<FString> Placeholders = ExtractPlaceholders();
	for (const FString& PlaceholderStr : Placeholders)
	{
		const FName PlaceholderName(*PlaceholderStr);
		FFlowPin PlaceholderFPin(PlaceholderName);
		PlaceholderFPin.PinFriendlyName = FText::FromName(PlaceholderName);
		PlaceholderFPin.SetPinType(FEdGraphPinType(FFlowPin::PC_Wildcard, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType()));

		OutDynamicInputPins.Add(PlaceholderFPin);
	}
}

void UFlowNode_FormatText::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UFlowNode_FormatText, FormatString))
	{
		OnReconstructionRequested.ExecuteIfBound();
		UpdateNodeConfigText();
	}
}
#endif

bool UFlowNode_FormatText::PrepareInputs()
{
	// We need to gather values for all connected placeholders.
	ResolvedInputValues.Empty();
	const UFlowAsset* OwningAsset = GetFlowAsset();
	if (!OwningAsset)
	{
		LogError(TEXT("FormatText: Owning FlowAsset is null during PrepareInputs."));
		return false;
	}

	const TArray<FString> CurrentPlaceholders = ExtractPlaceholders();
	TSet<FName> ValidPlaceholderNames;
	for (const FString& PName : CurrentPlaceholders)
	{
		ValidPlaceholderNames.Add(*PName);
	}
	for (const TPair<FName, FConnectedPin>& ConnectionPair : InputConnections)
	{
		const FName PlaceholderName = ConnectionPair.Key;
		if (!ValidPlaceholderNames.Contains(PlaceholderName))
		{
			continue;
		}

		const FConnectedPin& SourcePinInfo = ConnectionPair.Value;
		UFlowNode* SourceNode = OwningAsset->GetNode(SourcePinInfo.NodeGuid);
		if (!SourceNode)
		{
			LogError(FString::Printf(TEXT("FormatText: Source node for placeholder '%s' not found."), *PlaceholderName.ToString()));
			ResolvedInputValues.Add(PlaceholderName, FText::Format(LOCTEXT("ErrorUnresolvedNode", "[NODE ERR:{0}]"), FText::FromName(PlaceholderName)));
			continue;
		}

		const void* SourceDataPtr = nullptr;
		FProperty* SourceOutputProperty = nullptr;
		if (SourceNode->EvaluateAndGetOutputValue(SourcePinInfo.PinName, /*out*/ SourceOutputProperty, /*out*/ SourceDataPtr))
		{
			if (SourceOutputProperty && SourceDataPtr)
			{
				ResolvedInputValues.Add(PlaceholderName, GetPropertyValueAsText(SourceOutputProperty, SourceDataPtr));
			}
			else
			{
				LogError(FString::Printf(TEXT("FormatText: Failed to get valid data or property for placeholder '%s' from pin '%s' on node '%s'."), *PlaceholderName.ToString(), *SourcePinInfo.PinName.ToString(), *SourceNode->GetName()));
				ResolvedInputValues.Add(PlaceholderName, FText::Format(LOCTEXT("ErrorNoData", "[NO DATA:{0}]"), FText::FromName(PlaceholderName)));
			}
		}
		else
		{
			LogError(FString::Printf(TEXT("FormatText: Evaluation failed for placeholder '%s' from pin '%s' on node '%s'."), *PlaceholderName.ToString(), *SourcePinInfo.PinName.ToString(), *SourceNode->GetName()));
			ResolvedInputValues.Add(PlaceholderName, FText::Format(LOCTEXT("ErrorEvalFailed", "[EVAL ERR:{0}]"), FText::FromName(PlaceholderName)));
		}
	}
	return true;
}

void UFlowNode_FormatText::CachePinProperties()
{
	Super::CachePinProperties();
    
	DECLARE_INPUT_PIN(FormatString);
	DECLARE_OUTPUT_PIN(FormattedText);
}

bool UFlowNode_FormatText::PerformPureCalculation_Implementation()
{
	FFormatNamedArguments FormatArguments;
	const TArray<FString> Placeholders = ExtractPlaceholders();
	for (const FString& PlaceholderStr : Placeholders)
	{
		const FName PlaceholderName(*PlaceholderStr);
		if (const FText* FoundValue = ResolvedInputValues.Find(PlaceholderName))
		{
			FormatArguments.Add(PlaceholderStr, *FoundValue);
		}
		else
		{
			// Placeholder was in FormatString but not connected or value fetch failed.
			FormatArguments.Add(PlaceholderStr, FText::GetEmpty());
			// @note: to show the placeholder:
			// FormatArguments.Add(PlaceholderStr, FText::FromString(FString::Printf(TEXT("{%s}"), *PlaceholderStr)));
		}
	}

	FormattedText = FText::Format(FormatString, FormatArguments);
	return true;
}

TArray<FString> UFlowNode_FormatText::ExtractPlaceholders() const
{
	TArray<FString> ArgumentNames;
	FText::GetFormatPatternParameters(FormatString, ArgumentNames);
	return ArgumentNames;
}

FText UFlowNode_FormatText::GetPropertyValueAsText(const FProperty* Property, const void* DataPtr)
{
	if (!Property || !DataPtr)
	{
		return FText::GetEmpty();
	}

	if (const FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		return TextProp->GetPropertyValue(DataPtr);
	}
	if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		return FText::FromString(StrProp->GetPropertyValue(DataPtr));
	}
	if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		return FText::FromName(NameProp->GetPropertyValue(DataPtr));
	}
	if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		return UKismetTextLibrary::Conv_IntToText(IntProp->GetPropertyValue(DataPtr));
	}
	if (const FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
	{
		return UKismetTextLibrary::Conv_Int64ToText(Int64Prop->GetPropertyValue(DataPtr));
	}
	if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		return FText::AsNumber(FloatProp->GetPropertyValue(DataPtr));
	}
	if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		return FText::AsNumber(DoubleProp->GetPropertyValue(DataPtr));
	}
	if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return UKismetTextLibrary::Conv_BoolToText(BoolProp->GetPropertyValue(DataPtr));
	}
	if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (ByteProp->Enum)
		{
			return ByteProp->Enum->GetDisplayNameTextByValue(ByteProp->GetPropertyValue(DataPtr));
		}
		return UKismetTextLibrary::Conv_ByteToText(ByteProp->GetPropertyValue(DataPtr));
	}
	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		return EnumProp->GetEnum()->GetDisplayNameTextByValue(EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(DataPtr));
	}
	if (const FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
	{
		UObject* Obj = ObjectProp->GetObjectPropertyValue(DataPtr);
		return UKismetTextLibrary::Conv_ObjectToText(Obj);
	}

	// Fallback for other types: Use ExportText
	// This is a general approach but might not always produce the most user-friendly FText.
	FString ExportedValue;
	Property->ExportTextItem_Direct(ExportedValue, DataPtr, nullptr, nullptr, PPF_PropertyWindow); // PPF_PropertyWindow often gives good results
	return FText::FromString(ExportedValue);
}

#if WITH_EDITOR
void UFlowNode_FormatText::UpdateNodeConfigText_Implementation()
{
	// Show a shortened version if too long
	const FString CurrentFormatString = FormatString.ToString();
	if (CurrentFormatString.Len() > 60)
	{
		SetNodeConfigText(FText::FromString(CurrentFormatString.Left(57) + TEXT("...")));
	}
	else
	{
		SetNodeConfigText(FormatString);
	}
}
#endif

#undef LOCTEXT_NAMESPACE