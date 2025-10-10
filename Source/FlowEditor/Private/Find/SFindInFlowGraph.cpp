// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Find/SFindInFlowGraph.h"
#include "Asset/FlowAssetEditor.h"
#include "Graph/FlowGraphEditor.h"
#include "Graph/FlowGraphUtils.h"
#include "Graph/Nodes/FlowGraphNode.h"

#include "FlowAsset.h"
#include "Nodes/FlowNode.h"
#include "Nodes/Graph/FlowNode_SubGraph.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Views/ITypedTableView.h"
#include "GraphEditor.h"
#include "HAL/PlatformMath.h"
#include "Input/Events.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Children.h"
#include "Layout/WidgetPath.h"
#include "Math/Color.h"
#include "Misc/Attribute.h"
#include "SlotBase.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "Templates/Casts.h"
#include "Types/SlateStructs.h"
#include "UObject/Class.h"
#include "UObject/ObjectPtr.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "FFindInFlowGraphResult"

//////////////////////////////////////////////////////////////////////////
// FFindInFlowGraphResult

FFindInFlowGraphResult::FFindInFlowGraphResult(const FString& InValue)
    : Value(InValue), GraphNode(nullptr), GraphPin(nullptr), bIsSubGraphNode(false), bIsPinResult(false)
{
}

FFindInFlowGraphResult::FFindInFlowGraphResult(const FString& InValue, const TSharedPtr<FFindInFlowGraphResult>& InParent, UEdGraphNode* InNode, const bool bInIsSubGraphNode /*= false*/)
    : Value(InValue), GraphNode(InNode), GraphPin(nullptr), Parent(InParent), bIsSubGraphNode(bInIsSubGraphNode), bIsPinResult(false)
{
}

FFindInFlowGraphResult::FFindInFlowGraphResult(const FString& InValue, const TSharedPtr<FFindInFlowGraphResult>& InParent, UEdGraphNode* InOwningNode, UEdGraphPin* InPin, const bool bInIsSubGraphNode /*= false*/)
    : Value(InValue), GraphNode(InOwningNode), GraphPin(InPin), Parent(InParent), bIsSubGraphNode(bInIsSubGraphNode), bIsPinResult(true)
{
}

TSharedRef<SWidget> FFindInFlowGraphResult::CreateIcon() const
{
	const FSlateBrush* Brush;
	if (bIsPinResult && GraphPin)
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		Brush = FBlueprintEditorUtils::GetIconFromPin(GraphPin->PinType, /*bIsLarge=*/true);

		if (!Brush)
		{
			Brush = FAppStyle::GetBrush(TEXT("GraphEditor.FIB_Event"));
		}
		return SNew(SImage)
		    .Image(Brush)
		    .ColorAndOpacity(Schema->GetPinTypeColor(GraphPin->PinType));
	}

	Brush = FAppStyle::GetBrush(TEXT("GraphEditor.FIB_Event"));
	return SNew(SImage)
	    .Image(Brush);
}

FReply FFindInFlowGraphResult::OnClick(TWeakPtr<class FFlowAssetEditor> FlowAssetEditorPtr)
{
	if (FlowAssetEditorPtr.IsValid() && GraphNode.IsValid())
	{
		const UEdGraphNode* NodeToJump = GraphNode.Get();
		if (bIsSubGraphNode && Parent.IsValid() && Parent.Pin()->GraphNode.IsValid() && GraphNode.Get() != Parent.Pin()->GraphNode.Get())
		{
			FlowAssetEditorPtr.Pin()->JumpToNode(Parent.Pin()->GraphNode.Get());
		}

		FlowAssetEditorPtr.Pin()->JumpToNode(NodeToJump);
	}

	return FReply::Handled();
}

FReply FFindInFlowGraphResult::OnDoubleClick(TWeakPtr<class FFlowAssetEditor> FlowAssetEditorPtr) const
{
	if (!FlowAssetEditorPtr.IsValid())
	{
		return FReply::Handled();
	}

	if (bIsSubGraphNode && Parent.IsValid() && Parent.Pin()->GraphNode.IsValid())
	{
		if (const UFlowGraphNode* ParentFlowGraphNode = Cast<UFlowGraphNode>(Parent.Pin()->GraphNode.Get()))
		{
			if (UFlowNode_SubGraph* SubGraphFlowNode = Cast<UFlowNode_SubGraph>(ParentFlowGraphNode->GetFlowNodeBase()))
			{
				if (UObject* AssetToEdit = SubGraphFlowNode->GetAssetToEdit())
				{
					UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
					if (AssetEditorSubsystem->OpenEditorForAsset(AssetToEdit))
					{
						if (const TSharedPtr<FFlowAssetEditor> SubGraphAssetEditor = FFlowGraphUtils::GetFlowAssetEditor(GraphNode->GetGraph()))
						{
							if (GraphNode.IsValid())
							{
								SubGraphAssetEditor->JumpToNode(GraphNode.Get());
							}
						}
					}
					return FReply::Handled();
				}
			}
		}
	}

	if (GraphNode.IsValid())
	{
		FlowAssetEditorPtr.Pin()->JumpToNode(GraphNode.Get());
	}
	
	return FReply::Handled();
}

FString FFindInFlowGraphResult::GetDescriptionText() const
{
	if (!bIsPinResult && GraphNode.IsValid())
	{
		if (const UFlowGraphNode* FlowGraphNode = Cast<UFlowGraphNode>(GraphNode.Get()))
		{
			return FlowGraphNode->GetNodeDescription();
		}
	}
	return FString();
}

FString FFindInFlowGraphResult::GetCommentText() const
{
	if (!bIsPinResult && GraphNode.IsValid())
	{
		return GraphNode.Get()->NodeComment;
	}
	return FString();
}

FString FFindInFlowGraphResult::GetNodeTypeText() const
{
	if (!bIsPinResult && GraphNode.IsValid())
	{
		FString NodeClassName;
		if (const UFlowGraphNode* FlowGraphNode = Cast<UFlowGraphNode>(GraphNode.Get()))
		{
			if (FlowGraphNode->GetFlowNodeBase())
			{
				NodeClassName = FlowGraphNode->GetFlowNodeBase()->GetClass()->GetName();
			}
			else
			{
				NodeClassName = GraphNode->GetClass()->GetName();
			}
		}
		else
		{
			NodeClassName = GraphNode->GetClass()->GetName();
		}

		NodeClassName.RemoveFromStart(TEXT("FlowNode_"));
		NodeClassName.RemoveFromStart(TEXT("EdGraphNode_"));
		NodeClassName.RemoveFromStart(TEXT("FlowGraphNode_")); // maybe not needed?
		NodeClassName.RemoveFromEnd(TEXT("_C")); // For Blueprint generated classes
		return NodeClassName;
	}
	return FString();
}

FString FFindInFlowGraphResult::GetPinTypeText() const
{
	if (bIsPinResult && GraphPin)
	{
		const UEdGraphPin* Pin = GraphPin;
		FString PinCategory = Pin->PinType.PinCategory.ToString();
		FString PinSubCategory = Pin->PinType.PinSubCategory.ToString();
		FString PinSubCategoryObject = Pin->PinType.PinSubCategoryObject.IsValid() ? Pin->PinType.PinSubCategoryObject->GetName() : TEXT("");

		PinSubCategoryObject.RemoveFromEnd(TEXT("_C"));

		if (!PinSubCategory.IsEmpty())
		{
			PinCategory += TEXT(" (") + PinSubCategory;
			if (!PinSubCategoryObject.IsEmpty())
			{
				PinCategory += TEXT(": ") + PinSubCategoryObject;
			}
			PinCategory += TEXT(")");
		}
		else if (!PinSubCategoryObject.IsEmpty())
		{
			PinCategory += TEXT(" (") + PinSubCategoryObject + TEXT(")");
		}
		return PinCategory;
	}
	return FString();
}

FString FFindInFlowGraphResult::GetPinDirectionText() const
{
	if (bIsPinResult && GraphPin)
	{
		return (GraphPin->Direction == EGPD_Input) ? TEXT("Input") : TEXT("Output");
	}
	return FString();
}

FText FFindInFlowGraphResult::GetRowToolTipText() const
{
	if (bIsPinResult)
	{
		return LOCTEXT("PinResultTooltip", "Click to focus on owning node. Double-click to try and open parent graph if inside subgraph.");
	}
	if (GraphNode.IsValid())
	{
		if (const UFlowGraphNode* FlowGraphNode = Cast<UFlowGraphNode>(GraphNode.Get()))
		{
			if (Cast<UFlowNode_SubGraph>(FlowGraphNode->GetFlowNodeBase()))
			{
				return LOCTEXT("SubGraphNodeResultTooltip", "Click to focus on this SubGraph node. Double-click to open the SubGraph and focus.");
			}
		}
		return LOCTEXT("NodeResultTooltip", "Click to focus on node. Double-click to try and open parent graph if inside subgraph.");
	}
	return LOCTEXT("GenericResultTooltip", "Search result item.");
}

//////////////////////////////////////////////////////////////////////////
// SFindInFlowGraph

void SFindInFlowGraph::Construct(const FArguments& InArgs, TSharedPtr<FFlowAssetEditor> InFlowAssetEditor)
{
	FlowAssetEditorPtr = InFlowAssetEditor;
	RootSearchResult = MakeShared<FFindInFlowGraphResult>(TEXT("ROOT_INTERNAL_"));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(SearchTextField, SSearchBox)
				.HintText(LOCTEXT("FlowEditorSearchHint", "Find in Flow (Nodes, Pins)..."))
				.OnTextChanged(this, &SFindInFlowGraph::OnSearchTextChanged)
				.OnTextCommitted(this, &SFindInFlowGraph::OnSearchTextCommitted)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5, 0, 0, 0)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("FindInPinsLabel", "Pins"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0, 5, 0)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SFindInFlowGraph::GetFindInPinsCheckedState)
				.OnCheckStateChanged(this, &SFindInFlowGraph::OnFindInPinsStateChanged)
				.ToolTipText(LOCTEXT("FlowEditorPinSearchHint",
				                     "If checked, search will include pin names, tooltips, and default values."))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5, 0, 0, 0)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("FindInSubGraphLabel", "SubGraphs"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0, 0, 0)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SFindInFlowGraph::GetFindInSubGraphCheckedState)
				.OnCheckStateChanged(this, &SFindInFlowGraph::OnFindInSubGraphStateChanged)
				.ToolTipText(LOCTEXT("FlowEditorSubGraphSearchHint",
				                     "If checked, search will also include nodes within SubGraphs."))
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Menu.Background"))
			[
				SAssignNew(TreeView, STreeViewType)
				.TreeItemsSource(&ItemsFound)
				.OnGenerateRow(this, &SFindInFlowGraph::OnGenerateRow)
				.OnGetChildren(this, &SFindInFlowGraph::OnGetChildren)
				.OnSelectionChanged(this, &SFindInFlowGraph::OnTreeSelectionChanged)
				.OnMouseButtonDoubleClick(this, &SFindInFlowGraph::OnTreeSelectionDoubleClicked)
				.SelectionMode(ESelectionMode::Single)
			]
		]
	];
}

void SFindInFlowGraph::FocusForUse() const
{
	if (SearchTextField.IsValid())
	{
		FWidgetPath WidgetPath;
		FSlateApplication::Get().GeneratePathToWidgetUnchecked(SearchTextField.ToSharedRef(), WidgetPath);
		FSlateApplication::Get().SetKeyboardFocus(WidgetPath, EFocusCause::SetDirectly);
	}
}

void SFindInFlowGraph::OnSearchTextChanged(const FText& Text)
{
	SearchValue = Text.ToString().TrimStartAndEnd();
	InitiateSearch();
}

void SFindInFlowGraph::OnSearchTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	OnSearchTextChanged(Text);
}

void SFindInFlowGraph::InitiateSearch()
{
	ItemsFound.Empty();

	TArray<FString> Tokens;
	if (!SearchValue.IsEmpty())
	{
		SearchValue.ParseIntoArray(Tokens, TEXT(" "), true /*bCullEmpty*/);
	}

	HighlightText = FText::FromString(SearchValue);
	if (Tokens.Num() > 0)
	{
		const TSharedPtr<FFlowAssetEditor> Editor = FlowAssetEditorPtr.Pin();
		if (Editor.IsValid())
		{
			const TSharedPtr<SGraphEditor> GraphEditor = Editor->GetFlowGraph();
			if (GraphEditor.IsValid())
			{
				if (const UEdGraph* CurrentGraph = GraphEditor->GetCurrentGraph())
				{
					MatchTokensInGraph(Tokens, CurrentGraph, RootSearchResult, false);
				}
			}
		}
	}

	if (ItemsFound.Num() == 0 && !SearchValue.IsEmpty())
	{
		ItemsFound.Add(MakeShared<FFindInFlowGraphResult>(LOCTEXT("FlowEditorSearchNoResults", "No results found.").ToString()));
	}
	else if (ItemsFound.Num() == 0 && SearchValue.IsEmpty())
	{
		ItemsFound.Add(MakeShared<FFindInFlowGraphResult>(LOCTEXT("FlowEditorSearchEmptyPrompt", "Enter text to search...").ToString()));
	}

	TreeView->RequestTreeRefresh();
	for (const FSearchResult& Item : ItemsFound)
	{
		if (Item->Children.Num() > 0 || Item->bIsPinResult)
		{
			TreeView->SetItemExpansion(Item, true);
		}
	}
}

void SFindInFlowGraph::MatchTokensInGraph(const TArray<FString>& Tokens, const UEdGraph* Graph, FSearchResult CurrentParentResultInTree, bool bIsSearchingSubGraphContext)
{
	if (!Graph)
	{
		return;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		MatchTokensInNode(Tokens, Node, CurrentParentResultInTree, bIsSearchingSubGraphContext);
	}
}

void SFindInFlowGraph::MatchTokensInNode(const TArray<FString>& Tokens, UEdGraphNode* Node, FSearchResult ParentResultInTree, bool bIsSubGraphNodeContext)
{
	FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	FString NodeSearchString = NodeTitle;
	NodeSearchString += Node->GetClass()->GetName();
	NodeSearchString += Node->NodeComment;

	const UFlowGraphNode* FlowGraphNode = Cast<UFlowGraphNode>(Node);
	if (FlowGraphNode)
	{
		NodeSearchString += FlowGraphNode->GetNodeDescription();
	}
	NodeSearchString = NodeSearchString.Replace(TEXT(" "), TEXT(""));

	bool bNodeItselfMatches = StringMatchesSearchTokens(Tokens, NodeSearchString);
	FSearchResult NodeResult = MakeShared<FFindInFlowGraphResult>(NodeTitle, ParentResultInTree, Node, bIsSubGraphNodeContext);
	bool bAnyChildMatched = false;

	// Search in Pins if enabled
	if (bFindInPins)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->bHidden)
			{
				continue;
			}
			FString PinName = Pin->GetDisplayName().ToString();
			if (PinName.IsEmpty())
			{
				PinName = Pin->PinName.ToString();
			}
			FString PinSearchableText = PinName;
			PinSearchableText += Pin->PinToolTip;
			PinSearchableText += Pin->GetDefaultAsString();
			PinSearchableText += Pin->PinType.PinCategory.ToString();
			PinSearchableText += Pin->PinType.PinSubCategory.ToString();
			if (Pin->PinType.PinSubCategoryObject.IsValid())
			{
				PinSearchableText += Pin->PinType.PinSubCategoryObject->GetName();
			}
			PinSearchableText = PinSearchableText.Replace(TEXT(" "), TEXT(""));

			if (StringMatchesSearchTokens(Tokens, PinSearchableText))
			{
				bAnyChildMatched = true;
				FString PinDisplayValue = NodeTitle + TEXT(" -> ") + PinName;
				if (!Pin->IsDefaultAsStringEmpty())
				{
					FString DefaultValStr = Pin->GetDefaultAsString();
					constexpr int32 MaxDefaultValueDisplayLength = 30;
					if (DefaultValStr.Len() > MaxDefaultValueDisplayLength)
					{
						DefaultValStr = DefaultValStr.Left(MaxDefaultValueDisplayLength) + TEXT("...");
					}
					PinDisplayValue += TEXT(" [") + DefaultValStr + TEXT("]");
				}

				FSearchResult PinResultItem = MakeShared<FFindInFlowGraphResult>(PinDisplayValue, NodeResult, Node, Pin, bIsSubGraphNodeContext);
				NodeResult->Children.Add(PinResultItem);
			}
		}
	}

	// Search in SubGraph if enabled and applicable
	if (bFindInSubGraph && FlowGraphNode)
	{
		if (UFlowNode_SubGraph* SubGraphFlowNode = Cast<UFlowNode_SubGraph>(FlowGraphNode->GetFlowNodeBase()))
		{
			if (const UFlowAsset* SubFlowAsset = Cast<UFlowAsset>(SubGraphFlowNode->GetAssetToEdit()))
			{
				if (const UEdGraph* SubGraph = SubFlowAsset->GetGraph())
				{
					const int32 ChildrenBeforeSubGraphSearch = NodeResult->Children.Num();
					MatchTokensInGraph(Tokens, SubGraph, NodeResult, true);
					if (NodeResult->Children.Num() > ChildrenBeforeSubGraphSearch)
					{
						bAnyChildMatched = true;
					}
				}
			}
		}
	}

	// Add the NodeResult to the tree if the node itself matched or if any of its children (pins/subgraph nodes) matched.
	if (bNodeItselfMatches || bAnyChildMatched)
	{
		if (bIsSubGraphNodeContext)
		{
			ParentResultInTree->Children.Add(NodeResult);
		}
		else
		{
			ItemsFound.Add(NodeResult);
		}
	}
}

bool SFindInFlowGraph::StringMatchesSearchTokens(const TArray<FString>& Tokens, const FString& ComparisonString)
{
	if (Tokens.IsEmpty())
	{
		return false;
	}
	if (ComparisonString.IsEmpty())
	{
		return false;
	}

	const FString LowercaseComparisonString = ComparisonString.ToLower();
	for (const FString& Token : Tokens)
	{
		if (Token.IsEmpty())
		{
			continue;
		}
		if (!LowercaseComparisonString.Contains(Token.ToLower()))
		{
			return false;
		}
	}
	return true;
}

TSharedRef<ITableRow> SFindInFlowGraph::OnGenerateRow(FSearchResult InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<SHorizontalBox> RowContentBox;
	SAssignNew(RowContentBox, SHorizontalBox)
	// Icon
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	.Padding(2.f, 0.f)
	[
		InItem->CreateIcon()
	]
	// Main Value (Node Title or Pin Path)
	+ SHorizontalBox::Slot()
	.FillWidth(0.45f)
	.VAlign(VAlign_Center)
	.Padding(2.f, 0.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(InItem->Value))
		.HighlightText(HighlightText)
		.ColorAndOpacity(InItem->bIsPinResult
			                 ? FSlateColor(FLinearColor(0.8f, 0.8f, 1.0f))
			                 : FSlateColor::UseForeground())
	];

	if (InItem->bIsPinResult)
	{
		// Pin Type
		RowContentBox->AddSlot()
		             .FillWidth(0.35f)
		             .VAlign(VAlign_Center)
		             .Padding(5.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(InItem->GetPinTypeText()))
			.HighlightText(HighlightText)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
		// Pin Direction
		RowContentBox->AddSlot()
		             .FillWidth(0.20f)
		             .HAlign(HAlign_Right)
		             .VAlign(VAlign_Center)
		             .Padding(5.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(InItem->GetPinDirectionText()))
			.HighlightText(HighlightText)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
	}
	else // It's a Node result
	{
		// Node Description
		RowContentBox->AddSlot()
		             .FillWidth(0.30f)
		             .VAlign(VAlign_Center)
		             .Padding(5.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(InItem->GetDescriptionText()))
			.HighlightText(HighlightText)
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
		];
		// Node Type
		RowContentBox->AddSlot()
		             .FillWidth(0.25f)
		             .VAlign(VAlign_Center)
		             .Padding(5.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(InItem->GetNodeTypeText()))
			.HighlightText(HighlightText)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
		// Node Comment (if any)
		FString Comment = InItem->GetCommentText();
		if (!Comment.IsEmpty())
		{
			RowContentBox->AddSlot()
			             .FillWidth(1.0f)
			             .HAlign(HAlign_Right)
			             .VAlign(VAlign_Center)
			             .Padding(5.f, 0.f)
			[
				SNew(STextBlock)
				                .Text(FText::FromString(Comment))
				                .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.3f))
				                .HighlightText(HighlightText)
				                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			];
		}
	}

	return SNew(STableRow<FSearchResult>, OwnerTable)
		.Padding(FMargin(0, 2))
		.ToolTip(SNew(SToolTip).Text(InItem->GetRowToolTipText()))
		[
			RowContentBox.ToSharedRef()
		];
}

void SFindInFlowGraph::OnGetChildren(FSearchResult InItem, TArray<FSearchResult>& OutChildren)
{
	OutChildren.Append(InItem->Children);
}

void SFindInFlowGraph::OnTreeSelectionChanged(FSearchResult Item, ESelectInfo::Type SelectInfo)
{
	if (Item.IsValid() && SelectInfo != ESelectInfo::OnNavigation)
	{
		Item->OnClick(FlowAssetEditorPtr);
	}
}

void SFindInFlowGraph::OnTreeSelectionDoubleClicked(FSearchResult Item)
{
	if (Item.IsValid())
	{
		Item->OnDoubleClick(FlowAssetEditorPtr);
	}
}

void SFindInFlowGraph::OnFindInSubGraphStateChanged(ECheckBoxState CheckBoxState)
{
	bFindInSubGraph = (CheckBoxState == ECheckBoxState::Checked);
	InitiateSearch();
}

ECheckBoxState SFindInFlowGraph::GetFindInSubGraphCheckedState() const
{
	return bFindInSubGraph ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFindInFlowGraph::OnFindInPinsStateChanged(ECheckBoxState CheckBoxState)
{
	bFindInPins = (CheckBoxState == ECheckBoxState::Checked);
	InitiateSearch();
}

ECheckBoxState SFindInFlowGraph::GetFindInPinsCheckedState() const
{
	return bFindInPins ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

#undef LOCTEXT_NAMESPACE
