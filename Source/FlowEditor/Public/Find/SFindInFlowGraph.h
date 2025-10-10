// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "HAL/PlatformCrt.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include "Types/SlateEnums.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STreeView.h"

class ITableRow;
class SWidget;
class UFlowGraphNode;
class UEdGraphNode;
class UEdGraphPin;

/** Item that matched the search results */
class FFindInFlowGraphResult : public TSharedFromThis<FFindInFlowGraphResult>
{
public:
	/** Create a root or a simple text-only result. */
	FFindInFlowGraphResult(const FString& InValue);

	/** Create a graph node result */
	FFindInFlowGraphResult(const FString& InValue, const TSharedPtr<FFindInFlowGraphResult>& InParent, UEdGraphNode* InNode,
	                  bool bInIsSubGraphNode = false);

	/** Create a graph pin result */
	FFindInFlowGraphResult(const FString& InValue, const TSharedPtr<FFindInFlowGraphResult>& InParent, UEdGraphNode* InOwningNode,
	                  UEdGraphPin* InPin, bool bInIsSubGraphNode = false);

	/** Called when the user clicks on the search item */
	FReply OnClick(TWeakPtr<class FFlowAssetEditor> FlowAssetEditor);

	/** Called when a user double-clicks on the search item */
	FReply OnDoubleClick(TWeakPtr<class FFlowAssetEditor> FlowAssetEditor) const;

	/** Create an icon to represent the result */
	TSharedRef<SWidget> CreateIcon() const;

	/** Gets the description on the flow node if any */
	FString GetDescriptionText() const;

	/** Gets the comment on this node if any */
	FString GetCommentText() const;

	/** Gets the node type text */
	FString GetNodeTypeText() const;

	/** Gets the pin type text if this is a pin result */
	FString GetPinTypeText() const;

	/** Gets the pin direction text if this is a pin result */
	FString GetPinDirectionText() const;

	/** Gets the tooltip for this search result item */
	FText GetRowToolTipText() const;

	/** Children listed under this item. */
	TArray<TSharedPtr<FFindInFlowGraphResult>> Children;

	/** The string value to display for this result */
	FString Value;

	/** The graph node that this search result refers to (or the owning node for a pin) */
	TWeakObjectPtr<UEdGraphNode> GraphNode;

	/** The graph pin that this search result refers to (if applicable) */
	UEdGraphPin* GraphPin;

	/** Search result parent in the tree */
	TWeakPtr<FFindInFlowGraphResult> Parent;

	/** Whether this item represents a node within a subgraph */
	bool bIsSubGraphNode = false;

	/** Whether this item represents a pin */
	bool bIsPinResult = false;
};

/** Widget for searching within the focused Flow Asset */
class SFindInFlowGraph : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFindInFlowGraph)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class FFlowAssetEditor> InFlowAssetEditor);

	/** Focuses this widget's search box for use */
	void FocusForUse() const;

private:
	typedef TSharedPtr<FFindInFlowGraphResult> FSearchResult;
	typedef STreeView<FSearchResult> STreeViewType;

	/** Called when a user changes the text they are searching for */
	void OnSearchTextChanged(const FText& Text);

	/** Called when a user commits text. */
	void OnSearchTextCommitted(const FText& Text, ETextCommit::Type CommitType);

	/** Get the children of a tree row item */
	void OnGetChildren(FSearchResult InItem, TArray<FSearchResult>& OutChildren);

	/** Called when the user clicks on a new result in the tree */
	void OnTreeSelectionChanged(FSearchResult Item, ESelectInfo::Type SelectInfo);

	/** Called when a user double-clicks on a result in the tree */
	void OnTreeSelectionDoubleClicked(FSearchResult Item);

	/** Called when the 'Find In SubGraph' checkbox state changes */
	void OnFindInSubGraphStateChanged(ECheckBoxState CheckBoxState);
	ECheckBoxState GetFindInSubGraphCheckedState() const;

	/** Called when the 'Find In Pins' checkbox state changes */
	void OnFindInPinsStateChanged(ECheckBoxState CheckBoxState);
	ECheckBoxState GetFindInPinsCheckedState() const;

	/** Called when a new row is being generated for the tree view */
	TSharedRef<ITableRow> OnGenerateRow(FSearchResult InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Begins the search based on the current SearchValue and filter settings */
	void InitiateSearch();

	/** Finds any results that contain all the specified tokens */
	void MatchTokensInGraph(const TArray<FString>& Tokens, const UEdGraph* Graph, FSearchResult CurrentParentResult,
	                        bool bIsSearchingSubGraph);

	/** Helper function to recursively search within a node */
	void MatchTokensInNode(const TArray<FString>& Tokens, UEdGraphNode* Node, FSearchResult ParentResultInTree,
	                       bool bIsSubGraphNode);

	/** Determines if a string matches all the provided search tokens */
	static bool StringMatchesSearchTokens(const TArray<FString>& Tokens, const FString& ComparisonString);

private:
	/** Pointer back to the flow editor that owns this widget */
	TWeakPtr<class FFlowAssetEditor> FlowAssetEditorPtr;

	/** The tree view that displays the search results */
	TSharedPtr<STreeViewType> TreeView;

	/** The search text box input field */
	TSharedPtr<class SSearchBox> SearchTextField;

	/** This buffer stores the currently displayed top-level results */
	TArray<FSearchResult> ItemsFound;

	/** A root search result, used as a conceptual parent for top-level graph items */
	FSearchResult RootSearchResult;

	/** The text to highlight in the search results */
	FText HighlightText;

	/** The current string value being searched for */
	FString SearchValue;

	/** Controls whether the search should include nodes within subgraphs */
	bool bFindInSubGraph = true;

	/** Controls whether the search should include pins */
	bool bFindInPins = true;
};
