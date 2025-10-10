// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "DetailCustomizations/FlowNodeBlueprint_VariableDetailsCustomization.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "DetailLayoutBuilder.h"
#include "BlueprintEditorModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SSegmentedControl.h"

#include "Nodes/FlowNode.h"
#include "Nodes/FlowNodeBlueprint.h"

#define LOCTEXT_NAMESPACE "FlowNodeBlueprint_VariableDetailsCustomization"

const FName FFlowNodeBlueprint_VariableDetailsCustomization::MetadataKeyEnum = TEXT("FlowNodeCustomEnumProperty");
TSharedPtr<IDetailCustomization> FFlowNodeBlueprint_VariableDetailsCustomization::MakeInstance(TSharedPtr<IBlueprintEditor> InBlueprintEditor)
{
	const TArray<UObject*>* Objects = (InBlueprintEditor.IsValid() ? InBlueprintEditor->GetObjectsCurrentlyBeingEdited() : nullptr);
	if (Objects && Objects->Num() == 1)
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>((*Objects)[0]))
		{
			return MakeShareable(new FFlowNodeBlueprint_VariableDetailsCustomization(InBlueprintEditor, Blueprint));
		}
	}

	return nullptr;
}

void FFlowNodeBlueprint_VariableDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	if (!IsFlowNodeBlueprint())
	{
		return;
	}

	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailLayout.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() > 0)
	{
		UPropertyWrapper* PropertyWrapper = Cast<UPropertyWrapper>(ObjectsBeingCustomized[0].Get());
		const TWeakFieldPtr<FProperty> PropertyBeingCustomized = PropertyWrapper ? PropertyWrapper->GetProperty() : nullptr;
		if (PropertyBeingCustomized.IsValid())
		{
			const FName VarName = PropertyBeingCustomized->GetFName();
			const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(BlueprintPtr.Get(), VarName);
			if (VarIndex != INDEX_NONE)
			{
				DetailLayout.EditCategory("Data Pin Behavior", LOCTEXT("Data Pin Behavior", "Pin Behavior"), ECategoryPriority::Important);
				AddEnumMetadataRow(DetailLayout, VarName);
			}
		}
	}
}

bool FFlowNodeBlueprint_VariableDetailsCustomization::IsFlowNodeBlueprint() const
{
	if (!BlueprintPtr.IsValid())
	{
		return false;
	}

	if (BlueprintPtr->IsA<UFlowNodeBaseBlueprint>())
	{
		const UClass* GeneratedClass = BlueprintPtr->GeneratedClass;
		if (!GeneratedClass)
		{
			return false;
		}

		// @note We're specifically focusing on UFlowNode rather than UFlowNodeBase
		// to ensure that derived classes like UFlowNodeAddOn, which don't fully support
		// this functionality, are excluded from being treated as valid data sources.
		const bool bIsChildOfFlowNode = GeneratedClass->IsChildOf(UFlowNode::StaticClass());
		return bIsChildOfFlowNode;
	}

	return false;
}

void FFlowNodeBlueprint_VariableDetailsCustomization::InitEnumOptions()
{
	EnumOptions.Add(MakeShareable(new FString("None")));
	EnumOptions.Add(MakeShareable(new FString("Input")));
	EnumOptions.Add(MakeShareable(new FString("Output")));
}

void FFlowNodeBlueprint_VariableDetailsCustomization::AddEnumMetadataRow(IDetailLayoutBuilder& DetailLayout, FName VarName)
{
	const TSharedRef<SSegmentedControl<int32>> SegmentedControl =
	    SNew(SSegmentedControl<int32>)
	        .Value(this, &FFlowNodeBlueprint_VariableDetailsCustomization::GetCurrentEnumIndex, VarName)
	        .OnValueChanged(this, &FFlowNodeBlueprint_VariableDetailsCustomization::OnEnumValueChanged, VarName);

	for (int32 i = 0; i < EnumOptions.Num(); ++i)
	{
		SegmentedControl->AddSlot(i)
		    .Text(FText::FromString(*EnumOptions[i]))
		    .ToolTip(FText::Format(LOCTEXT("EnumOptionTooltip", "Set behavior to {0}"), FText::FromString(*EnumOptions[i])));
	}

	DetailLayout.EditCategory("Data Pin Behavior")
	    .AddCustomRow(LOCTEXT("FlowNodeCustomEnumProperty", "Behavior Type"))
	    .NameContent()
	        [SNew(STextBlock)
	                .Text(LOCTEXT("FlowNodeCustomEnumPropertyName", "Behavior"))
	                .Font(DetailLayout.GetDetailFont())]
	    .ValueContent()
	        [SegmentedControl];
}

void FFlowNodeBlueprint_VariableDetailsCustomization::OnEnumValueChanged(const int32 NewValue, const FName VarName)
{
	if (NewValue >= 0 && NewValue < EnumOptions.Num() && EnumOptions[NewValue].IsValid())
	{
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(BlueprintPtr.Get(), VarName, nullptr, MetadataKeyEnum, *EnumOptions[NewValue]);
	}
}

int32 FFlowNodeBlueprint_VariableDetailsCustomization::GetCurrentEnumIndex(FName VarName) const
{
	FString Value;
	FBlueprintEditorUtils::GetBlueprintVariableMetaData(BlueprintPtr.Get(), VarName, nullptr, MetadataKeyEnum, Value);

	for (int32 i = 0; i < EnumOptions.Num(); ++i)
	{
		if (EnumOptions[i].IsValid() && EnumOptions[i]->Equals(Value))
		{
			return i;
		}
	}

	return 0;
}

#undef LOCTEXT_NAMESPACE