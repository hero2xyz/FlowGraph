// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "GraphEditorDragDropAction.h"
#include "IDetailCustomization.h"
#include "IDetailDragDropHandler.h"
#include "PropertyBagDetails.h"
#include "PropertyBagDetails.h"
#include "Graph/FlowGraphSchema_Actions.h"

class FFlowAssetEditor;

/**
 * Custom drag-drop action for Flow Graph variables.
 */
class FFlowGraphVariableDragDropAction : public FGraphEditorDragDropAction
{
public:
    DRAG_DROP_OPERATOR_TYPE(FFlowGraphVariableDragDropAction, FGraphEditorDragDropAction)

    static TSharedRef<FFlowGraphVariableDragDropAction> New(FName InVariableName, TSharedPtr<FFlowGraphSchemaAction_NewGetVariableNode> InGetAction, TSharedPtr<FFlowGraphSchemaAction_NewSetVariableNode> InSetAction);

    //~ FGraphEditorDragDropAction Interface
    virtual void HoverTargetChanged() override;
    virtual FReply DroppedOnPanel(const TSharedRef<SWidget>& Panel, const FVector2f& ScreenPosition, const FVector2f& GraphPosition, UEdGraph& Graph) override;
    virtual FReply DroppedOnPin(const FVector2f& ScreenPosition, const FVector2f& GraphPosition) override;
    //~ End FGraphEditorDragDropAction Interface

private:
    FName VariableName;
    TSharedPtr<FFlowGraphSchemaAction_NewGetVariableNode> GetAction;
    TSharedPtr<FFlowGraphSchemaAction_NewSetVariableNode> SetAction;
};

/**
 * Custom drag-drop handler that creates our graph-specific drag-drop action.
 */
class FFlowGraphVariableDragDropHandler : public IDetailDragDropHandler
{
public:
    FFlowGraphVariableDragDropHandler(const FPropertyBagPropertyDesc& InPropertyDesc, TWeakPtr<FFlowAssetEditor> InEditor);

    //~ IDetailDragDropHandler Interface
    virtual TSharedPtr<FDragDropOperation> CreateDragDropOperation() const override;
    virtual bool UseHandleWidget() const override { return true; }
    virtual bool AcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone) const override;
    virtual TOptional<EItemDropZone> CanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone) const override;
    //~ End IDetailDragDropHandler Interface

private:
    FPropertyBagPropertyDesc PropertyDesc;
    TWeakPtr<FFlowAssetEditor> EditorPtr;
};

/**
 * Custom version of FPropertyBagInstanceDataDetails that installs our drag-drop handler.
 */
class FFlowGraphPropertyBagDataDetails : public FPropertyBagInstanceDataDetails
{
public:
    FFlowGraphPropertyBagDataDetails(const FConstructParams& ConstructParams, TWeakPtr<FFlowAssetEditor> InEditor);

    //~ FInstancedStructDataDetails Interface
    virtual void OnChildRowAdded(IDetailPropertyRow& ChildRow) override;
    //~ End FInstancedStructDataDetails Interface

private:
    TWeakPtr<FFlowAssetEditor> EditorPtr;
};

/**
 * Customization for the UFlowGraphDetailsDataSource object.
 */
class FFlowGraphDataSourceDetails : public IDetailCustomization
{
public:
    FFlowGraphDataSourceDetails(TWeakPtr<FFlowAssetEditor> InEditor) : EditorPtr(InEditor) {}
    static TSharedRef<IDetailCustomization> MakeInstance(TWeakPtr<FFlowAssetEditor> InEditor);
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    TWeakPtr<FFlowAssetEditor> EditorPtr;
};
