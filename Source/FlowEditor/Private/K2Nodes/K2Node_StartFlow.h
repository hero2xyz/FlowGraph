// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "FlowAsset.h"
#include "K2Node_StartFlow.generated.h"

class UFlowNode_DefineProperties;

UCLASS()
class FLOWEDITOR_API UK2Node_StartFlow : public UK2Node
{
	GENERATED_BODY()

public:
	UK2Node_StartFlow();
	virtual ~UK2Node_StartFlow() override;

	//~ UEdGraphNode interface
	virtual void PostLoad() override;
	virtual void DestroyNode() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void AllocateDefaultPins() override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void ReconstructNode() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	//~ End UEdGraphNode interface

	//~ UK2Node interface
	virtual void PreloadRequiredAssets() override;
	virtual bool IsNodePure() const override { return false; }
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	//~ End UK2Node interface

private:
	/** Sets the Flow Asset for this node and reconstructs the node */
	void SetSelectedFlowAsset(UFlowAsset* NewAsset);
	
	/** Get the Flow Asset that is currently selected in the node's pin. */
	UFlowAsset* GetSelectedFlowAsset() const;
	
	/** Get the entry node from the currently selected Flow Asset. */
	UFlowNode_DefineProperties* GetFlowEntryNode() const;

	/** Subscribe to notifications when the referenced Flow Asset is changed. */
	void SubscribeToAssetChanges();

	/** Unsubscribe from the currently referenced Flow Asset. */
	void UnsubscribeFromAssetChanges();

	/** Handler for when the referenced asset is modified. */
	void OnAssetChanged();
	
	/** Helper function to get the name of the FlowAsset pin. */
	static FName GetFlowAssetPinName();

private:
	/** The Flow Asset that defines the parameters to show on this node. */
	UPROPERTY()
	TObjectPtr<UFlowAsset> FlowAssetTemplate;
	
	/** Handle to the delegate that listens for changes in the asset. */
	FDelegateHandle OnSubGraphReconstructionRequestedHandle;
};
