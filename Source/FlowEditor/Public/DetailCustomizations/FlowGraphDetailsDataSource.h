// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StructUtils/PropertyBag.h"
#include "FlowGraphDetailsDataSource.generated.h"

class FFlowAssetEditor;
class UFlowAsset;

/**
 * Transient UObject that acts as a data source for the IDetailsView
 * in the Flow Graph variables panel.
 * This allows us to intercept property changes and apply them back to the UFlowAsset in a controlled manner.
 */
UCLASS()
class UFlowGraphDetailsDataSource : public UObject
{
	GENERATED_BODY()

public:
	/** Initializes the data source. */
	void Initialize(TWeakPtr<FFlowAssetEditor> InEditor);

	/** Gets the UFlowAsset being edited. */
	UFlowAsset* GetAsset() const;

	/** The property bag instance that the details view will be editing. This is a temporary copy. */
	UPROPERTY(EditAnywhere, Category = "Graph Variables")
	FInstancedPropertyBag GraphVariables;

protected:
	//~ Begin UObject Interface
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

private:
	/** Persists the changes from this data source back to the UFlowAsset. */
	void CommitChangesToAsset();

	/** Compares the old and new bags to detect structural changes like renames, removals, or type changes. */
	void DetectAndProcessChanges(const FInstancedPropertyBag& OldBag, const FInstancedPropertyBag& NewBag);

	/** Handles the logic for renaming a variable across all nodes in the graph. */
	void HandleVariableRenamed(FName OldName, FName NewName);

	/** Handles the logic for a variable's type changing. */
	void HandleVariableTypeChanged(const FPropertyBagPropertyDesc& ChangedDesc);

	/** Handles the logic for removing all nodes that reference a deleted variable. */
	void HandleVariableRemoved(FName RemovedName);

	/** A snapshot of the property bag taken before any changes are made by the details panel. */
	FInstancedPropertyBag CachedGraphVariables;

	/** A weak pointer back to the asset editor that owns this data source. */
	TWeakPtr<FFlowAssetEditor> EditorPtr;
};
