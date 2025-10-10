// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Graph/Widgets/SFlowGraphNode_Variable.h"

#include "FlowEditorStyle.h"
#include "GraphEditorSettings.h"
#include "SCommentBubble.h"
#include "TutorialMetaData.h"
#include "Graph/Nodes/FlowGraphNode_Conversion.h"
#include "Graph/Nodes/FlowGraphNode_Variable.h"
#include "Nodes/FlowNode.h"

void SFlowGraphNode_Variable::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();

	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	FMargin ContentAreaMargin = FMargin(0.0f, 4.0f);
	FMargin TitleMargin = FMargin(0.0f, 8.0f);
	EHorizontalAlignment TitleHAlign = HAlign_Center;
	FText TitleText;
	TSharedPtr<SWidget> TitleWidget;

	UFlowGraphNode_GetVariable* GetNode = Cast<UFlowGraphNode_GetVariable>(GraphNode);
	UFlowGraphNode_SetVariable* SetNode = Cast<UFlowGraphNode_SetVariable>(GraphNode);
	UFlowGraphNode_Conversion* ConversionNode = Cast<UFlowGraphNode_Conversion>(GraphNode);
	
	if (SetNode)
	{
		TitleText = NSLOCTEXT("FlowGraphEditor", "VariableSet", "SET");
	}
	else if (GetNode)
	{
		if (GetNode->GetInputPin() != nullptr)
		{
			TitleText = NSLOCTEXT("FlowGraphEditor", "VariableGet", "GET");
			ContentAreaMargin.Top += 16.0f;
		}
	}
	else if (ConversionNode)
	{
		TitleText = ConversionNode->GetNodeTitle(ENodeTitleType::ListView);
	}
	if (!TitleText.IsEmpty())
	{
		TitleWidget = SNew(STextBlock)
			.TextStyle(FAppStyle::Get(), "Graph.Node.NodeTitle")
			.Text(TitleText);
	}
	else
	{
		TitleWidget = SNullWidget::NullWidget;
	}

	SetupErrorReporting();

	FGraphNodeMetaData TagMeta(TEXT("Graphnode"));
	PopulateMetaTag(&TagMeta);

	//             ________________
	//            | (>) L |  R (>) |
	//            | (>) E |  I (>) |
	//            | (>) F |  G (>) |
	//            | (>) T |  H (>) |
	//            |       |  T (>) |
	//            |_______|________|
	//
	this->ContentScale.Bind( this, &SGraphNode::GetContentScale );
	this->GetOrAddSlot( ENodeZone::Center )
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			SNew(SOverlay)
			.AddMetaData<FGraphNodeMetaData>(TagMeta)
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image( FAppStyle::GetBrush("Graph.VarNode.Body") )
			]
			+ SOverlay::Slot()
			.VAlign(VAlign_Top)
			[
				SNew(SImage)
				.Image( FAppStyle::GetBrush("Graph.VarNode.ColorSpill") )
				.ColorAndOpacity( this, &SFlowGraphNode_Variable::GetVariableColor )
			]
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image( FAppStyle::GetBrush("Graph.VarNode.Gloss") )
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
			+ SOverlay::Slot()
			.VAlign(VAlign_Top)
			.HAlign(TitleHAlign)
			.Padding(TitleMargin)
			[
				TitleWidget.ToSharedRef()
			]
			+ SOverlay::Slot()
			.Padding( ContentAreaMargin )
			[
				// NODE CONTENT AREA
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(1.0f)
				.Padding( FMargin(2,0) )
				[
					// LEFT
					SAssignNew(LeftNodeBox, SVerticalBox)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.Padding( FMargin(2,0) )
				[
					// RIGHT
					SAssignNew(RightNodeBox, SVerticalBox)
				]
			]
		]
		+SVerticalBox::Slot()
		.VAlign(VAlign_Top)
		.AutoHeight() 
		.Padding( FMargin(5.0f, 1.0f) )
		[
			ErrorReporting->AsWidget()
		]
	];

	float VerticalPaddingAmount = 0.0f;
	bool bIsImpure = false;
	if (SetNode)
	{
		bIsImpure = true;
	}
	else if (GetNode)
	{
		bIsImpure = (GetNode->GetInputPin() != nullptr);
	}

	if (bIsImpure)
	{
		VerticalPaddingAmount += 7.0f;
	}

	if (VerticalPaddingAmount > 0.0f)
	{
		LeftNodeBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SSpacer).Size(FVector2D(0.0f, VerticalPaddingAmount))
		];

		RightNodeBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SSpacer).Size(FVector2D(0.0f, VerticalPaddingAmount))
		];
	}

	// Create comment bubble
	TSharedPtr<SCommentBubble> CommentBubble;
	const FSlateColor CommentColor = GetDefault<UGraphEditorSettings>()->DefaultCommentNodeTitleColor;

	SAssignNew( CommentBubble, SCommentBubble )
	.GraphNode( GraphNode )
	.Text( this, &SGraphNode::GetNodeComment )
	.OnTextCommitted( this, &SGraphNode::OnCommentTextCommitted )
	.ColorAndOpacity( CommentColor )
	.AllowPinning( true )
	.EnableTitleBarBubble( true )
	.EnableBubbleCtrls( true )
	.GraphLOD( this, &SGraphNode::GetCurrentLOD )
	.IsGraphNodeHovered( this, &SGraphNode::IsHovered );

	GetOrAddSlot( ENodeZone::TopCenter )
	.SlotOffset2f( TAttribute<FVector2f>( CommentBubble.Get(), &SCommentBubble::GetOffset2f ))
	.SlotSize2f( TAttribute<FVector2f>( CommentBubble.Get(), &SCommentBubble::GetSize2f ))
	.AllowScaling( TAttribute<bool>( CommentBubble.Get(), &SCommentBubble::IsScalingAllowed ))
	.VAlign( VAlign_Top )
	[
		CommentBubble.ToSharedRef()
	];

	CreatePinWidgets();
}

void SFlowGraphNode_Variable::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	UFlowGraphNode* MyGraphNode = Cast<UFlowGraphNode>(GraphNode);
	if (MyGraphNode && MyGraphNode->IsA<UFlowGraphNode_Conversion>())
	{
		if (const UEdGraphPin* PinObj = PinToAdd->GetPinObj())
		{
			const bool bIsDefaultInput = (PinObj->PinName == UFlowNode::DefaultInputPin.PinName);
			const bool bIsDefaultOutput = (PinObj->PinName == UFlowNode::DefaultOutputPin.PinName);

			if (bIsDefaultInput || bIsDefaultOutput)
			{
				PinToAdd->SetShowLabel(false);
			}
		}
	}

	SFlowGraphNode::AddPin(PinToAdd);
}

FSlateColor SFlowGraphNode_Variable::GetVariableColor() const
{
	// Check output pins first (for getters, the variable pin is an output)
	for (const TSharedRef<SGraphPin>& PinWidget : OutputPins)
	{
		if (const UEdGraphPin* Pin = PinWidget->GetPinObj())
		{
			// Skip execution pins, we want the variable data pin
			if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				return GetDefault<UEdGraphSchema_K2>()->GetPinTypeColor(Pin->PinType);
			}
		}
	}

	// Check input pins (for setters, the variable pin is an input)
	for (const TSharedRef<SGraphPin>& PinWidget : InputPins)
	{
		if (const UEdGraphPin* Pin = PinWidget->GetPinObj())
		{
			// Skip execution pins, we want the variable data pin
			if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				return GetDefault<UEdGraphSchema_K2>()->GetPinTypeColor(Pin->PinType);
			}
		}
	}

	return FSlateColor(FColor::Black);
}

const FSlateBrush* SFlowGraphNode_Variable::GetShadowBrush(bool bSelected) const
{
	if (GEditor->PlayWorld)
	{
		switch (FlowGraphNode->GetActivationState())
		{
		case EFlowNodeState::NeverActivated:
			return SGraphNode::GetShadowBrush(bSelected);
		case EFlowNodeState::Active:
			return FFlowEditorStyle::Get()->GetBrush(TEXT("Flow.Node.ActiveShadow"));
		case EFlowNodeState::Completed:
		case EFlowNodeState::Aborted:
			return FFlowEditorStyle::Get()->GetBrush(TEXT("Flow.Node.WasActiveShadow"));
		default: ;
		}
	}

	return SGraphNode::GetShadowBrush(bSelected);
}

void SFlowGraphNode_Variable::GetDiffHighlightBrushes(const FSlateBrush*& BackgroundOut, const FSlateBrush*& ForegroundOut) const
{
	BackgroundOut = FAppStyle::GetBrush(TEXT("Graph.VarNode.DiffHighlight"));
	ForegroundOut = FAppStyle::GetBrush(TEXT("Graph.VarNode.DiffHighlightShading"));
}
