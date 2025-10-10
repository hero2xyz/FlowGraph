// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "DetailCustomizations/FlowPropertyBagDragDrop.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCustomizations/FlowGraphDetailsDataSource.h"
#include "Framework/Application/SlateApplication.h"
#include "Asset/FlowAssetEditor.h"

#define LOCTEXT_NAMESPACE "FlowPropertyBagDragDrop"

/** @return true property handle holds struct property of type T.  */
template <typename T>
bool IsScriptStruct(const TSharedPtr<IPropertyHandle>& PropertyHandle)
{
	if (!PropertyHandle)
	{
		return false;
	}

	FStructProperty* StructProperty = CastField<FStructProperty>(PropertyHandle->GetProperty());
	return StructProperty && StructProperty->Struct->IsA(TBaseStructure<T>::Get()->GetClass());
}

/** @return property bag struct common to all edited properties. */
const UPropertyBag* GetCommonBagStruct(TSharedPtr<IPropertyHandle> StructProperty)
{
	const UPropertyBag* CommonBagStruct = nullptr;

	if (ensure(IsScriptStruct<FInstancedPropertyBag>(StructProperty)))
	{
		StructProperty->EnumerateConstRawData(
			[&CommonBagStruct](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
			{
				if (RawData)
				{
					const FInstancedPropertyBag* Bag = static_cast<const FInstancedPropertyBag*>(RawData);

					const UPropertyBag* BagStruct = Bag->GetPropertyBagStruct();
					if (CommonBagStruct && CommonBagStruct != BagStruct)
					{
						// Multiple struct types on the sources - show nothing set
						CommonBagStruct = nullptr;
						return false;
					}
					CommonBagStruct = BagStruct;
				}

				return true;
			});
	}

	return CommonBagStruct;
}

// FFlowGraphVariableDragDropAction Implementation
TSharedRef<FFlowGraphVariableDragDropAction> FFlowGraphVariableDragDropAction::New(
	FName InVariableName, TSharedPtr<FFlowGraphSchemaAction_NewGetVariableNode> InGetAction,
	TSharedPtr<FFlowGraphSchemaAction_NewSetVariableNode> InSetAction)
{
	TSharedRef<FFlowGraphVariableDragDropAction> Operation = MakeShareable(new FFlowGraphVariableDragDropAction);
	Operation->VariableName = InVariableName;
	Operation->GetAction = InGetAction;
	Operation->SetAction = InSetAction;
	Operation->Construct();
	return Operation;
}

void FFlowGraphVariableDragDropAction::HoverTargetChanged()
{
	const FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
	const bool bIsSetting = ModifierKeys.IsControlDown() || ModifierKeys.IsAltDown();

	const FSlateBrush* Icon = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
	FText Message;

	if (bIsSetting)
	{
		Message = FText::Format(LOCTEXT("SetVariableFeedback", "Set {0}"), FText::FromName(VariableName));
	}
	else
	{
		Message = FText::Format(LOCTEXT("GetVariableFeedback", "Get {0}"), FText::FromName(VariableName));
	}
	SetSimpleFeedbackMessage(Icon, FLinearColor::White, Message);
}

FReply FFlowGraphVariableDragDropAction::DroppedOnPanel(const TSharedRef<SWidget>& Panel,
                                                        const FVector2f& ScreenPosition, const FVector2f& GraphPosition,
                                                        UEdGraph& Graph)
{
	const FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
	const bool bIsSetting = ModifierKeys.IsControlDown() || ModifierKeys.IsAltDown();

	TSharedPtr<FEdGraphSchemaAction> ActionToPerform = bIsSetting
		                                                   ? StaticCastSharedPtr<FEdGraphSchemaAction>(SetAction)
		                                                   : StaticCastSharedPtr<FEdGraphSchemaAction>(GetAction);

	if (ActionToPerform.IsValid())
	{
		ActionToPerform->PerformAction(&Graph, nullptr, GraphPosition);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply FFlowGraphVariableDragDropAction::DroppedOnPin(const FVector2f& ScreenPosition, const FVector2f& GraphPosition)
{
	if (UEdGraphPin* CurrentHoveredPin = GetHoveredPin())
	{
		if (UEdGraph* Graph = CurrentHoveredPin->GetOwningNode()->GetGraph())
		{
			return DroppedOnPanel(SNullWidget::NullWidget, ScreenPosition, GraphPosition, *Graph);
		}
	}
	return FReply::Unhandled();
}

// FFlowGraphVariableDragDropHandler Implementation
FFlowGraphVariableDragDropHandler::FFlowGraphVariableDragDropHandler(const FPropertyBagPropertyDesc& InPropertyDesc,
                                                                     TWeakPtr<FFlowAssetEditor> InEditor)
	: PropertyDesc(InPropertyDesc), EditorPtr(InEditor)
{
}

TSharedPtr<FDragDropOperation> FFlowGraphVariableDragDropHandler::CreateDragDropOperation() const
{
	const FName VariableName = PropertyDesc.Name;
	const FText Category = FText::GetEmpty();

	TSharedPtr<FFlowGraphSchemaAction_NewGetVariableNode> GetAction =
		MakeShared<FFlowGraphSchemaAction_NewGetVariableNode>(
			Category, FText::Format(LOCTEXT("GetVariableAction", "Get {0}"), FText::FromName(VariableName)), FText(), 0,
			VariableName);

	TSharedPtr<FFlowGraphSchemaAction_NewSetVariableNode> SetAction =
		MakeShared<FFlowGraphSchemaAction_NewSetVariableNode>(
			Category, FText::Format(LOCTEXT("SetVariableAction", "Set {0}"), FText::FromName(VariableName)), FText(), 0,
			VariableName);

	return FFlowGraphVariableDragDropAction::New(VariableName, GetAction, SetAction);
}

bool FFlowGraphVariableDragDropHandler::AcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone) const
{
	return false;
}

TOptional<EItemDropZone> FFlowGraphVariableDragDropHandler::CanAcceptDrop(
	const FDragDropEvent& DragDropEvent, EItemDropZone DropZone) const
{
	return TOptional<EItemDropZone>();
}

// FFlowGraphPropertyBagDataDetails Implementation
FFlowGraphPropertyBagDataDetails::FFlowGraphPropertyBagDataDetails(const FConstructParams& ConstructParams,
                                                                   TWeakPtr<FFlowAssetEditor> InEditor)
	: FPropertyBagInstanceDataDetails(ConstructParams), EditorPtr(InEditor)
{
}

void FFlowGraphPropertyBagDataDetails::OnChildRowAdded(IDetailPropertyRow& ChildRow)
{
	FPropertyBagInstanceDataDetails::OnChildRowAdded(ChildRow);

	const UPropertyBag* BagStruct = GetCommonBagStruct(BagStructProperty);
	const FProperty* ChildProperty = ChildRow.GetPropertyHandle()->GetProperty();
	const FPropertyBagPropertyDesc* Desc = BagStruct ? BagStruct->FindPropertyDescByProperty(ChildProperty) : nullptr;

	if (Desc && EnumHasAnyFlags(ChildRowFeatures, EPropertyBagChildRowFeatures::DragAndDrop))
	{
		TSharedPtr<FFlowGraphVariableDragDropHandler> DragDropHandler = MakeShared<FFlowGraphVariableDragDropHandler>(
			*Desc, EditorPtr);
		ChildRow.DragDropHandler(DragDropHandler);
	}
}

// FFlowGraphDataSourceDetails Implementation
TSharedRef<IDetailCustomization> FFlowGraphDataSourceDetails::MakeInstance(TWeakPtr<FFlowAssetEditor> InEditor)
{
	return MakeShared<FFlowGraphDataSourceDetails>(InEditor);
}

void FFlowGraphDataSourceDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const TSharedRef<IPropertyHandle> VarsHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UFlowGraphDetailsDataSource, GraphVariables));
	DetailBuilder.HideProperty(VarsHandle);

	FPropertyBagInstanceDataDetails::FConstructParams Params;
	Params.BagStructProperty = VarsHandle;
	Params.PropUtils = DetailBuilder.GetPropertyUtilities();
	Params.ChildRowFeatures = EPropertyBagChildRowFeatures::Extended;

	const auto BagDetails = MakeShared<FFlowGraphPropertyBagDataDetails>(Params, EditorPtr);
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("GraphVariables", LOCTEXT("GraphVariablesCategory", "Graph Variables"));
	Category.AddCustomBuilder(BagDetails, /*bForAdvanced=*/false);
}

#undef LOCTEXT_NAMESPACE
