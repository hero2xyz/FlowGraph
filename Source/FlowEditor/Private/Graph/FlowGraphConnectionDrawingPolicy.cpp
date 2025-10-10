// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Graph/FlowGraphConnectionDrawingPolicy.h"

#include "Graph/FlowGraph.h"
#include "Graph/FlowGraphEditor.h"
#include "Graph/FlowGraphEditorSettings.h"
#include "Graph/FlowGraphSettings.h"
#include "Graph/FlowGraphUtils.h"
#include "Graph/Nodes/FlowGraphNode.h"

#include "FlowAsset.h"
#include "FlowEditorLogChannels.h"
#include "Graph/Nodes/FlowGraphNode_Reroute.h"
#include "Nodes/FlowNode.h"

#include "Misc/App.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlowGraphConnectionDrawingPolicy)

/////////////////////////////////////////////////////
// FFlowGraphConnectionDrawingPolicy

FFlowGraphConnectionDrawingPolicy::FFlowGraphConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
    : FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements), GraphObj(InGraphObj)
{
	// Cache off the editor options
	RecentWireDuration = UFlowGraphSettings::Get()->RecentWireDuration;

	InactiveColor = UFlowGraphSettings::Get()->InactiveWireColor;
	RecentColor = UFlowGraphSettings::Get()->RecentWireColor;
	RecordedColor = UFlowGraphSettings::Get()->RecordedWireColor;
	SelectedColor = UFlowGraphSettings::Get()->SelectedWireColor;

	InactiveWireThickness = UFlowGraphSettings::Get()->InactiveWireThickness;
	RecentWireThickness = UFlowGraphSettings::Get()->RecentWireThickness;
	RecordedWireThickness = UFlowGraphSettings::Get()->RecordedWireThickness;
	SelectedWireThickness = UFlowGraphSettings::Get()->SelectedWireThickness;

	// Don't want to draw ending arrowheads
	ArrowImage = nullptr;
	ArrowRadius = FVector2D::ZeroVector;
}

void FFlowGraphConnectionDrawingPolicy::BuildPaths()
{
	if (const UFlowAsset* FlowInstance = CastChecked<UFlowGraph>(GraphObj)->GetFlowAsset()->GetInspectedInstance())
	{
		const double CurrentTime = FApp::GetCurrentTime();
		for (const auto NodePair : FlowInstance->GetNodes())
		{
			const UFlowGraphNode* FlowGraphNode = Cast<UFlowGraphNode>(NodePair.Value->GetGraphNode());
			for (const TPair<uint8, FPinRecord>& Record : FlowGraphNode->GetWireRecords())
			{
				const uint8 PinIndexInAllPins = Record.Key;
				const FPinRecord& PinRecordData = Record.Value;
				if (!FlowGraphNode->Pins.IsValidIndex(PinIndexInAllPins))
				{
					UE_LOG(LogFlowEditor, Error, TEXT("Flow node '%s' - GetWireRecords returned invalid pin index %d."), *FlowGraphNode->GetName(), PinIndexInAllPins);
					continue;
				}

				UEdGraphPin* OutputPin = FlowGraphNode->Pins[PinIndexInAllPins];
				if (!OutputPin || OutputPin->Direction != EGPD_Output)
				{
					UE_LOG(LogFlowEditor, Warning, TEXT("Flow node '%s' - GetWireRecords key %d did not correspond to an output pin."), *FlowGraphNode->GetName(), PinIndexInAllPins);
					continue;
				}

				if (OutputPin->LinkedTo.Num() > 0)
				{
					const bool bIsRecent = (CurrentTime < PinRecordData.Time + RecentWireDuration);
					for (UEdGraphPin* LinkedPin : OutputPin->LinkedTo)
					{
						if (LinkedPin)
						{
							RecordedPaths.Add(OutputPin, LinkedPin);
							if (bIsRecent)
							{
								RecentPaths.Add(OutputPin, LinkedPin);
							}
						}
					}
				}
			}
		}
	}

	if (GraphObj && (UFlowGraphEditorSettings::Get()->bHighlightInputWiresOfSelectedNodes || UFlowGraphEditorSettings::Get()->bHighlightOutputWiresOfSelectedNodes))
	{
		const TSharedPtr<SFlowGraphEditor> FlowGraphEditor = FFlowGraphUtils::GetFlowGraphEditor(GraphObj);
		if (FlowGraphEditor.IsValid())
		{
			for (UFlowGraphNode* SelectedNode : FlowGraphEditor->GetSelectedFlowNodes())
			{
				for (UEdGraphPin* Pin : SelectedNode->Pins)
				{
					if ((Pin->Direction == EGPD_Input && UFlowGraphEditorSettings::Get()->bHighlightInputWiresOfSelectedNodes)
					    || (Pin->Direction == EGPD_Output && UFlowGraphEditorSettings::Get()->bHighlightOutputWiresOfSelectedNodes))
					{
						for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
						{
							SelectedPaths.Emplace(Pin, LinkedPin);
						}
					}
				}
			}
		}
	}
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6
void FFlowGraphConnectionDrawingPolicy::DrawConnection(int32 LayerId, const FVector2D& Start, const FVector2D& End, const FConnectionParams& Params)
#else
void FFlowGraphConnectionDrawingPolicy::DrawConnection(int32 LayerId, const FVector2f& Start, const FVector2f& End, const FConnectionParams& Params)
#endif
{
	switch (UFlowGraphSettings::Get()->ConnectionDrawType)
	{
		case EFlowConnectionDrawType::Default:
			FConnectionDrawingPolicy::DrawConnection(LayerId, Start, End, Params);
			break;
		case EFlowConnectionDrawType::Circuit:
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6
			DrawCircuitSpline(LayerId, FVector2f(Start), FVector2f(End), Params);
#else
			DrawCircuitSpline(LayerId, Start, End, Params);
#endif
			break;
		default:;
	}
}

// Give specific editor modes a chance to highlight this connection or darken non-interesting connections
void FFlowGraphConnectionDrawingPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, FConnectionParams& Params)
{
	Params.AssociatedPin1 = OutputPin;
	Params.AssociatedPin2 = InputPin;

	// Get the schema and grab the default color from it
	check(OutputPin);
	check(GraphObj);
	const UEdGraphSchema* Schema = GraphObj->GetSchema();

	if (OutputPin->bOrphanedPin || (InputPin && InputPin->bOrphanedPin))
	{
		Params.WireColor = FLinearColor::Red;
	}
	else
	{
		Params.WireColor = Schema->GetPinTypeColor(OutputPin->PinType);
		if (Cast<UFlowGraphNode>(OutputPin->GetOwningNode())->GetSignalMode() == EFlowSignalMode::Disabled)
		{
			Params.WireColor *= 0.5f;
			Params.WireThickness = 0.5f;
		}
		else if (InputPin)
		{
			const bool bIsDataPin = InputPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec;
			if (ContainsPair(SelectedPaths, OutputPin, InputPin) || ContainsPair(SelectedPaths, InputPin, OutputPin))
			{
				if (!bIsDataPin)
				{
					Params.WireColor = SelectedColor;
				}
				Params.WireThickness = SelectedWireThickness;
				Params.bDrawBubbles = false;
			}
			// Recent paths
			else if (ContainsPair(RecentPaths, OutputPin, InputPin))
			{
				if (!bIsDataPin)
				{
					Params.WireColor = RecentColor;
					Params.bDrawBubbles = true;
				}
				else
				{
					Params.bDrawBubbles = true;
				}
				Params.WireThickness = RecentWireThickness;
			}
			// All paths, showing graph history
			else if (ContainsPair(RecordedPaths, OutputPin, InputPin))
			{
				if (!bIsDataPin)
				{
					Params.WireColor = RecordedColor;
				}
				Params.WireThickness = RecordedWireThickness;
				Params.bDrawBubbles = false;
			}
			// It's not followed, fade it and keep it thin
			else
			{
				if (!bIsDataPin)
				{
					Params.WireColor = InactiveColor;
				}
				Params.WireThickness = InactiveWireThickness;
			}
		}
	}

	// If reroute node path goes backwards, we need to flip the direction to make it look nice
	// (all of the logic for this is basically same as in FKismetConnectionDrawingPolicy)
	{
		UEdGraphNode* OutputNode = OutputPin->GetOwningNode();
		UEdGraphNode* InputNode = (InputPin != nullptr) ? InputPin->GetOwningNode() : nullptr;
		if (auto* OutputRerouteNode = Cast<UFlowGraphNode_Reroute>(OutputNode))
		{
			if (ShouldChangeTangentForReroute(OutputRerouteNode))
			{
				Params.StartDirection = EGPD_Input;
			}
		}

		if (auto* InputRerouteNode = Cast<UFlowGraphNode_Reroute>(InputNode))
		{
			if (ShouldChangeTangentForReroute(InputRerouteNode))
			{
				Params.EndDirection = EGPD_Output;
			}
		}
	}

	const bool bDeemphasizeUnhoveredPins = HoveredPins.Num() > 0;

	if (bDeemphasizeUnhoveredPins)
	{
		ApplyHoverDeemphasis(OutputPin, InputPin, /*inout*/ Params.WireThickness, /*inout*/ Params.WireColor);
	}
}

void FFlowGraphConnectionDrawingPolicy::Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries, FArrangedChildren& ArrangedNodes)
{
	BuildPaths();

	FConnectionDrawingPolicy::Draw(InPinGeometries, ArrangedNodes);
}

void FFlowGraphConnectionDrawingPolicy::DrawCircuitSpline(const int32& LayerId, const FVector2f& Start, const FVector2f& End, const FConnectionParams& Params) const
{
	const FVector2f StartingPoint = FVector2f(Start.X + UFlowGraphSettings::Get()->CircuitConnectionSpacing.X, Start.Y);
	const FVector2f EndPoint = FVector2f(End.X - UFlowGraphSettings::Get()->CircuitConnectionSpacing.Y, End.Y);
	const FVector2f ControlPoint = GetControlPoint(StartingPoint, EndPoint);

	const FVector2f StartDirection = (Params.StartDirection == EGPD_Output) ? FVector2f(1.0f, 0.0f) : FVector2f(-1.0f, 0.0f);
	const FVector2f EndDirection = (Params.EndDirection == EGPD_Input) ? FVector2f(1.0f, 0.0f) : FVector2f(-1.0f, 0.0f);

	DrawCircuitConnection(LayerId, Start, StartDirection, StartingPoint, EndDirection, Params);
	DrawCircuitConnection(LayerId, StartingPoint, StartDirection, ControlPoint, EndDirection, Params);
	DrawCircuitConnection(LayerId, ControlPoint, StartDirection, EndPoint, EndDirection, Params);
	DrawCircuitConnection(LayerId, EndPoint, StartDirection, End, EndDirection, Params);
}

void FFlowGraphConnectionDrawingPolicy::DrawCircuitConnection(const int32& LayerId, const FVector2f& Start, const FVector2f& StartDirection, const FVector2f& End, const FVector2f& EndDirection, const FConnectionParams& Params) const
{
	FSlateDrawElement::MakeDrawSpaceSpline(DrawElementsList, LayerId, Start, StartDirection, End, EndDirection, Params.WireThickness, ESlateDrawEffect::None, Params.WireColor);

	if (Params.bDrawBubbles)
	{
		// This table maps distance along curve to alpha
		FInterpCurve<float> SplineReparamTable;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6
		const float SplineLength = MakeSplineReparamTable(FVector2D(Start), FVector2D(StartDirection), FVector2D(End), FVector2D(EndDirection), SplineReparamTable);
#else
		const float SplineLength = MakeSplineReparamTable(Start, StartDirection, End, EndDirection, SplineReparamTable);
#endif

		// Draw bubbles on the spline
		if (Params.bDrawBubbles)
		{
			const float BubbleSpacing = 64.f * ZoomFactor;
			const float BubbleSpeed = 192.f * ZoomFactor;
			const FVector2f BubbleSize = BubbleImage->ImageSize * ZoomFactor * 0.2f * Params.WireThickness;

			const float Time = (FPlatformTime::Seconds() - GStartTime);
			const float BubbleOffset = FMath::Fmod(Time * BubbleSpeed, BubbleSpacing);
			const int32 NumBubbles = FMath::CeilToInt(SplineLength / BubbleSpacing);
			for (int32 i = 0; i < NumBubbles; ++i)
			{
				const float Distance = (static_cast<float>(i) * BubbleSpacing) + BubbleOffset;
				if (Distance < SplineLength)
				{
					const float Alpha = SplineReparamTable.Eval(Distance, 0.f);
					FVector2f BubblePos = FMath::CubicInterp(Start, StartDirection, End, EndDirection, Alpha);
					BubblePos -= (BubbleSize * 0.5f);

					FSlateDrawElement::MakeBox(DrawElementsList, LayerId, FPaintGeometry(BubblePos, BubbleSize, ZoomFactor), BubbleImage, ESlateDrawEffect::None, Params.WireColor);
				}
			}
		}
	}
}

FVector2f FFlowGraphConnectionDrawingPolicy::GetControlPoint(const FVector2f& Source, const FVector2f& Target)
{
	const FVector2f Delta = Target - Source;
	const float Tangent = FMath::Tan(UFlowGraphSettings::Get()->CircuitConnectionAngle * (PI / 180.f));

	const float DeltaX = FMath::Abs(Delta.X);
	const float DeltaY = FMath::Abs(Delta.Y);

	const float SlopeWidth = DeltaY / Tangent;
	if (DeltaX > SlopeWidth)
	{
		return Delta.X > 0.f ? FVector2f(Target.X - SlopeWidth, Source.Y) : FVector2f(Source.X - SlopeWidth, Target.Y);
	}

	const float SlopeHeight = DeltaX * Tangent;
	if (DeltaY > SlopeHeight)
	{
		if (Delta.Y > 0.f)
		{
			return Delta.X < 0.f ? FVector2f(Source.X, Target.Y - SlopeHeight) : FVector2f(Target.X, Source.Y + SlopeHeight);
		}

		if (Delta.X < 0.f)
		{
			return FVector2f(Source.X, Target.Y + SlopeHeight);
		}
	}

	return FVector2f(Target.X, Source.Y - SlopeHeight);
}

bool FFlowGraphConnectionDrawingPolicy::ShouldChangeTangentForReroute(UFlowGraphNode_Reroute* Reroute)
{
	if (const bool* pResult = RerouteToReversedDirectionMap.Find(Reroute))
	{
		return *pResult;
	}
	else
	{
		bool bPinReversed = false;

		FVector2D AverageLeftPin;
		FVector2D AverageRightPin;
		FVector2D CenterPin = FVector2D::ZeroVector;
		const bool bCenterValid = Reroute->OutputPins.Num() == 0 ? false : FindPinCenter(Reroute->OutputPins[0], /*out*/ CenterPin);
		const bool bLeftValid = GetAverageConnectedPosition(Reroute, EGPD_Input, /*out*/ AverageLeftPin);
		const bool bRightValid = GetAverageConnectedPosition(Reroute, EGPD_Output, /*out*/ AverageRightPin);

		if (bLeftValid && bRightValid)
		{
			bPinReversed = AverageRightPin.X < AverageLeftPin.X;
		}
		else if (bCenterValid)
		{
			if (bLeftValid)
			{
				bPinReversed = CenterPin.X < AverageLeftPin.X;
			}
			else if (bRightValid)
			{
				bPinReversed = AverageRightPin.X < CenterPin.X;
			}
		}

		RerouteToReversedDirectionMap.Add(Reroute, bPinReversed);

		return bPinReversed;
	}
}

bool FFlowGraphConnectionDrawingPolicy::FindPinCenter(const UEdGraphPin* Pin, FVector2D& OutCenter) const
{
	if (const TSharedPtr<SGraphPin>* PinWidget = PinToPinWidgetMap.Find(Pin))
	{
		if (const FArrangedWidget* PinEntry = PinGeometries->Find((*PinWidget).ToSharedRef()))
		{
			OutCenter = FGeometryHelper::CenterOf(PinEntry->Geometry);
			return true;
		}
	}

	return false;
}

bool FFlowGraphConnectionDrawingPolicy::GetAverageConnectedPosition(UFlowGraphNode_Reroute* Reroute, EEdGraphPinDirection Direction, FVector2D& OutPos) const
{
	FVector2D Result = FVector2D::ZeroVector;
	int32 ResultCount = 0;

	if (Reroute->InputPins.Num() == 0 || Reroute->OutputPins.Num() == 0)
	{
		return false;
	}

	UEdGraphPin* Pin = (Direction == EGPD_Input) ? Reroute->InputPins[0] : Reroute->OutputPins[0];
	for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
	{
		FVector2D CenterPoint;
		if (FindPinCenter(LinkedPin, /*out*/ CenterPoint))
		{
			Result += CenterPoint;
			ResultCount++;
		}
	}

	if (ResultCount > 0)
	{
		OutPos = Result * (1.0f / ResultCount);
		return true;
	}
	else
	{
		return false;
	}
}

bool FFlowGraphConnectionDrawingPolicy::ContainsPair(const TMultiMap<UEdGraphPin*, UEdGraphPin*>& InMultiMap, const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin)
{
	for (auto It = InMultiMap.CreateConstKeyIterator(OutputPin); It; ++It)
	{
		if (It.Value() == InputPin)
		{
			return true;
		}
	}
	return false;
}
