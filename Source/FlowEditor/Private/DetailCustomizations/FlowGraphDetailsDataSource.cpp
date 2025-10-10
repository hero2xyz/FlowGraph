// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "DetailCustomizations/FlowGraphDetailsDataSource.h"
#include "Asset/FlowAssetEditor.h"
#include "FlowAsset.h"
#include "ScopedTransaction.h"
#include "Graph/Nodes/FlowGraphNode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Nodes/Graph/FlowNode_GetVariable.h"
#include "Nodes/Graph/FlowNode_SetVariable.h"

#define LOCTEXT_NAMESPACE "FlowGraphDetailsDataSource"

void UFlowGraphDetailsDataSource::Initialize(TWeakPtr<FFlowAssetEditor> InEditor)
{
	EditorPtr = InEditor;
}

UFlowAsset* UFlowGraphDetailsDataSource::GetAsset() const
{
	return EditorPtr.IsValid() ? EditorPtr.Pin()->GetFlowAsset() : nullptr;
}

void UFlowGraphDetailsDataSource::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	// Before any change is made by the details panel, take a snapshot of the current state.
	// This snapshot will be used in PostEditChangeProperty to figure out what happened.
	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UFlowGraphDetailsDataSource, GraphVariables))
	{
		CachedGraphVariables = GraphVariables;
	}
}

void UFlowGraphDetailsDataSource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UFlowAsset* Asset = GetAsset();
	if (!Asset || PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("UpdateGraphVariables", "Update Graph Variables"));
	Asset->Modify();
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	// Persist ALL changes back to the asset.
	CommitChangesToAsset();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UFlowGraphDetailsDataSource, GraphVariables))
	{
		DetectAndProcessChanges(CachedGraphVariables, GraphVariables);
	}

	// Notify the asset that variables have changed so another UI can update
	Asset->OnGraphVariablesChanged.Broadcast();
}

void UFlowGraphDetailsDataSource::CommitChangesToAsset()
{
	UFlowAsset* Asset = GetAsset();
	if (!Asset)
	{
		return;
	}

	Asset->GraphVariables.MigrateToNewBagInstance(GraphVariables);
	Asset->GraphVariables.CopyMatchingValuesByID(GraphVariables);
}

void UFlowGraphDetailsDataSource::DetectAndProcessChanges(const FInstancedPropertyBag& OldBag, const FInstancedPropertyBag& NewBag)
{
	// This function compares the state of the property bag before and after a
	// change to identify renames, removals, and type changes.
	
	// Create a map of the old properties by their unique ID for a quick lookup.
	TMap<FGuid, const FPropertyBagPropertyDesc*> OldGuidToDescMap;
	if (const UPropertyBag* OldBagStruct = OldBag.GetPropertyBagStruct())
	{
		for (const FPropertyBagPropertyDesc& Desc : OldBagStruct->GetPropertyDescs())
		{
			OldGuidToDescMap.Add(Desc.ID, &Desc);
		}
	}

	// Iterate through the new properties and compare them against the old ones.
	TSet<FGuid> NewGuids;
	if (const UPropertyBag* NewBagStruct = NewBag.GetPropertyBagStruct())
	{
		for (const FPropertyBagPropertyDesc& NewDesc : NewBagStruct->GetPropertyDescs())
		{
			NewGuids.Add(NewDesc.ID);

			if (const FPropertyBagPropertyDesc** OldDescPtr = OldGuidToDescMap.Find(NewDesc.ID))
			{
				const FPropertyBagPropertyDesc* OldDesc = *OldDescPtr;

				// --- RENAME DETECTION ---
				// The ID is the same, but the name is different.
				if (OldDesc->Name != NewDesc.Name)
				{
					HandleVariableRenamed(OldDesc->Name, NewDesc.Name);
				}

				// --- TYPE CHANGE DETECTION ---
				// The ID is the same, but the type information has changed.
				if (OldDesc->ValueType != NewDesc.ValueType || 
					OldDesc->ValueTypeObject != NewDesc.ValueTypeObject || 
					OldDesc->ContainerTypes != NewDesc.ContainerTypes)
				{
					HandleVariableTypeChanged(NewDesc);
				}
			}
			// @note: New variables (where the ID doesn't exist in the old map) are handled implicitly
			// by the details panel adding them. We don't need special logic here for them.
		}
	}

	// Find properties that were in the old bag but not in the new one. This means they were removed.
	for (const auto& Elem : OldGuidToDescMap)
	{
		if (!NewGuids.Contains(Elem.Key))
		{
			HandleVariableRemoved(Elem.Value->Name);
		}
	}
}

void UFlowGraphDetailsDataSource::HandleVariableRenamed(FName OldName, FName NewName)
{
	UFlowAsset* Asset = GetAsset();
	if (!Asset)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("RenameFlowVariable", "Rename Flow Graph Variable"));
	Asset->Modify();
	
	for (const auto& NodePair : Asset->GetNodes())
	{
		UFlowNode* FlowNode = NodePair.Value;
		if (!FlowNode)
		{
			continue;
		}
		bool bNodeNeedsReconstruction = false;

		// Check for GetVariable nodes
		if (UFlowNode_GetVariable* GetDef = Cast<UFlowNode_GetVariable>(FlowNode))
		{
			if (GetDef->VariableName == OldName)
			{
				GetDef->Modify();
				GetDef->VariableName = NewName;
				bNodeNeedsReconstruction = true;
			}
		}
		// Check for SetVariable nodes
		else if (UFlowNode_SetVariable* SetDef = Cast<UFlowNode_SetVariable>(FlowNode))
		{
			if (SetDef->VariableName == OldName)
			{
				SetDef->Modify();
				SetDef->VariableName = NewName;
				bNodeNeedsReconstruction = true;
			}
		}

		if (bNodeNeedsReconstruction)
		{
			if (UFlowGraphNode* VisualNode = Cast<UFlowGraphNode>(FlowNode->GetGraphNode()))
			{
				VisualNode->ReconstructNode();
			}
		}
	}
}

void UFlowGraphDetailsDataSource::HandleVariableTypeChanged(const FPropertyBagPropertyDesc& ChangedDesc)
{
	UFlowAsset* Asset = GetAsset();
	if (!Asset)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ChangeFlowVariableType", "Change Flow Graph Variable Type"));
	Asset->Modify();
	
	const FName VariableName = ChangedDesc.Name;

	// Find any node using this variable.
	for (const auto& NodePair : Asset->GetNodes())
	{
		UFlowNode* FlowNode = NodePair.Value;
		if (!FlowNode)
		{
			continue;
		}
		bool bNodeNeedsReconstruction = false;

		if (const UFlowNode_GetVariable* GetDef = Cast<const UFlowNode_GetVariable>(FlowNode))
		{
			if (GetDef->VariableName == VariableName)
			{
				bNodeNeedsReconstruction = true;
			}
		}
		else if (const UFlowNode_SetVariable* SetDef = Cast<const UFlowNode_SetVariable>(FlowNode))
		{
			if (SetDef->VariableName == VariableName)
			{
				bNodeNeedsReconstruction = true;
			}
		}

		if (bNodeNeedsReconstruction)
		{
			if (UFlowGraphNode* VisualNode = Cast<UFlowGraphNode>(FlowNode->GetGraphNode()))
			{
				VisualNode->ReconstructNode();
			}
		}
	}
}

void UFlowGraphDetailsDataSource::HandleVariableRemoved(FName RemovedName)
{
	UFlowAsset* Asset = GetAsset();
	if (!Asset)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("RemoveFlowVariable", "Remove Flow Graph Variable"));
	Asset->Modify();

	// We need a copy of the nodes to iterate over, as we might be destroying them.
	TArray<UFlowNode*> NodesToRemove;
	for (const auto& NodePair : Asset->GetNodes())
	{
		UFlowNode* FlowNode = NodePair.Value;
		if (!FlowNode)
		{
			continue;
		}
		bool bShouldRemoveNode = false;
		if (const UFlowNode_GetVariable* GetDef = Cast<const UFlowNode_GetVariable>(FlowNode))
		{
			if (GetDef->VariableName == RemovedName)
			{
				bShouldRemoveNode = true;
			}
		}
		else if (const UFlowNode_SetVariable* SetDef = Cast<const UFlowNode_SetVariable>(FlowNode))
		{
			if (SetDef->VariableName == RemovedName)
			{
				bShouldRemoveNode = true;
			}
		}

		if (bShouldRemoveNode)
		{
			NodesToRemove.Add(FlowNode);
		}
	}

	// Now, destroy the collected nodes.
	for (const UFlowNode* NodeToRemove : NodesToRemove)
	{
		if (UFlowGraphNode* VisualNode = Cast<UFlowGraphNode>(NodeToRemove->GetGraphNode()))
		{
			FBlueprintEditorUtils::RemoveNode(nullptr, VisualNode, true);
		}
	}
}

#undef LOCTEXT_NAMESPACE