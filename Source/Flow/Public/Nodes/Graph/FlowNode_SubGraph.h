// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "Nodes/FlowNode.h"
#include "StructUtils/PropertyBag.h"

#include "FlowNode_SubGraph.generated.h"

// Forward Declarations
class UFlowNode;
#if WITH_EDITOR
class FFlowMessageLog;
#endif // WITH_EDITOR

/**
 * Executes a referenced Flow Asset as a self-contained unit.
 * Acts as a proxy, exposing data pins based on the interface nodes
 * (Start, Finish, CustomInput, CustomOutput) within the referenced Flow Asset.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Sub Graph"))
class FLOW_API UFlowNode_SubGraph : public UFlowNode
{
	GENERATED_UCLASS_BODY()
	virtual ~UFlowNode_SubGraph() override;

public:
	friend class UFlowAsset;
	friend class FFlowNode_SubGraphDetails;
	friend class UFlowGraphNode_SubGraph;
	friend class UFlowSubsystem;
	static FFlowPin StartPin;
	static FFlowPin FinishPin;

protected:
	UPROPERTY(EditAnywhere, Category = "Graph")
	TSoftObjectPtr<UFlowAsset> Asset;

	/*
	 * Allow to create instance of the same Flow Asset as the asset containing this node
	 * Enabling it may cause an infinite loop, if graph would keep creating copies of itself
	 */
	UPROPERTY(EditAnywhere, Category = "Graph")
	bool bCanInstanceIdenticalAsset;

private:
	UPROPERTY(SaveGame)
	FString SavedAssetInstanceName;

protected:

	/**
	 * Output Data Pins defined by the referenced subgraph's Finish/CustomOutput nodes.
	 * The editor harvests structure from the referenced asset.
	 * Values are sent to the parent graph at runtime after subgraph completion.
	 */
	UPROPERTY(VisibleDefaultsOnly, SaveGame, Category = "Data Pins", meta = (FlowPropertyBag = "Output", DisplayName = "Subgraph Outputs"))
	FInstancedPropertyBag OutputDataBag;
	
	/**
	 * Input Data Pins defined by the referenced subgraph's Start/CustomInput nodes.
	 * The editor harvests structure from the referenced asset.
	 * Values are received from the parent graph at runtime.
	 */
	UPROPERTY(VisibleDefaultsOnly, SaveGame, Category = "Data Pins", meta = (FlowPropertyBag = "Input", DisplayName = "Subgraph Inputs"))
	FInstancedPropertyBag InputDataBag;
protected:
	/** Checks if the configured Asset can be instanced based on recursion setting and validity. */
	virtual bool CanBeAssetInstanced() const;

	// --- UObject Overrides ---
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// --- UFlowNode Overrides ---
public:
	virtual void CachePinProperties() override;
	virtual void PreloadContent() override;
	virtual void FlushContent() override;
	virtual void ExecuteInput(const FName& PinName) override;
	virtual void Cleanup() override;
	virtual void ForceFinishNode() override;
	virtual bool IsSupportedInputPinName(const FName& PinName) const override;
	virtual void* GetPropertyContainer(const FProperty* Property) const override;

	/**
	 * Called by an internal Finish or CustomOutput node within the subgraph instance
	 * to signal completion and transfer output data back to this node.
	 * @param InternalNode The Finish or CustomOutput node instance that triggered the exit.
	 * @param OutputExecPinName The name of the execution pin to trigger on this SubGraph node.
	 */
	virtual void NotifySubGraphOutput(UFlowNode* InternalNode, const FName OutputExecPinName);

protected:
	virtual void OnLoad_Implementation() override;

#if WITH_EDITORONLY_DATA

protected:
	// All the classes allowed to be used as assets on this subgraph node
	UPROPERTY()
	TArray<TSubclassOf<UFlowAsset>> AllowedAssignedAssetClasses;

	// All the classes disallowed to be used as assets on this subgraph node
	UPROPERTY()
	TArray<TSubclassOf<UFlowAsset>> DeniedAssignedAssetClasses;
#endif

#if WITH_EDITOR

public:
	virtual FText GetNodeTitle() const override;
	virtual FString GetNodeDescription() const override;
	virtual UObject* GetAssetToEdit() override;
	virtual EDataValidationResult ValidateNode() override;

	// UObject
	virtual void OnGraphLoaded() override;
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	void Clear();
	void HarvestDataPins();
	virtual bool SupportsContextPins() const override { return true; }
	virtual TArray<FFlowPin> GetContextInputs() const override;
	virtual TArray<FFlowPin> GetContextOutputs() const override;

private:
	void SubscribeToAssetChanges();
	void OnAssetParametersChanged();
	FDelegateHandle OnSubGraphReconstructionRequestedHandle;
#endif
};
