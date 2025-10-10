// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "IDetailsView.h"

class FFlowAssetEditor;
class UFlowGraphDetailsDataSource;
class UEdGraph;

/**
 * The main panel for displaying and managing variables for a UFlowAsset.
 */
class FLOWEDITOR_API SFlowGraphVariablesPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFlowGraphVariablesPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<FFlowAssetEditor> InEditor);
	~SFlowGraphVariablesPanel();

	/** Forces the details view to refresh its data source from the asset. */
	void Refresh();

private:
	/** Pointer to the asset editor that owns this panel. */
	TWeakPtr<FFlowAssetEditor> EditorPtr;

	/** The details view widget that displays the variables. */
	TSharedPtr<IDetailsView> DetailsView;

	/** The transient UObject that serves as the data source for the DetailsView. */
	TObjectPtr<UFlowGraphDetailsDataSource> DataSource;

	/** Handles the "+ Add Variable" menu action. */
	void AddNewVariable();
	FReply AddNewVariableAction();
};
