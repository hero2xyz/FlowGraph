// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Graph/Widgets/SFlowGraphVariablesPanel.h"
#include "Asset/FlowAssetEditor.h"
#include "FlowAsset.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "DetailCustomizations/FlowGraphDetailsDataSource.h"
#include "DetailCustomizations/FlowPropertyBagDragDrop.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "SFlowGraphVariablesPanel"

void SFlowGraphVariablesPanel::Construct(const FArguments& InArgs, TWeakPtr<FFlowAssetEditor> InEditor)
{
	EditorPtr = InEditor;

	DataSource = NewObject<UFlowGraphDetailsDataSource>();
	DataSource->AddToRoot();
	DataSource->Initialize(InEditor);

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bShowObjectLabel = false;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->RegisterInstancedCustomPropertyLayout(UFlowGraphDetailsDataSource::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FFlowGraphDataSourceDetails::MakeInstance, InEditor));
	DetailsView->SetObject(DataSource);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("AddVariable", "+ Add Variable"))
			.OnClicked(this, &SFlowGraphVariablesPanel::AddNewVariableAction)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			DetailsView.ToSharedRef()
		]
	];

	// Perform an initial sync.
	Refresh();
}

SFlowGraphVariablesPanel::~SFlowGraphVariablesPanel()
{
	if (DataSource)
	{
		DataSource->RemoveFromRoot();
		DataSource = nullptr;
	}
}

void SFlowGraphVariablesPanel::Refresh()
{
	UFlowAsset* Asset = EditorPtr.IsValid() ? EditorPtr.Pin()->GetFlowAsset() : nullptr;
	if (Asset && DataSource)
	{
		DataSource->GraphVariables = Asset->GraphVariables;
		DetailsView->SetObject(DataSource, true);
	}
}

void SFlowGraphVariablesPanel::AddNewVariable()
{
	UFlowAsset* Asset = EditorPtr.IsValid() ? EditorPtr.Pin()->GetFlowAsset() : nullptr;
	if (!Asset)
	{
		return;
	}

	FInstancedPropertyBag& TargetBag = Asset->GraphVariables;

	const FScopedTransaction Transaction(LOCTEXT("AddNewVariable", "Add New Flow Graph Variable"));
	Asset->Modify();

	FName NewVarName;
	int32 Index = 0;
	do
	{
		NewVarName = FName(*FString::Printf(TEXT("NewVar_%d"), Index++));
	}
	while (TargetBag.FindPropertyDescByName(NewVarName) != nullptr);
	if (TargetBag.AddProperty(NewVarName, EPropertyBagPropertyType::Bool) == EPropertyBagAlterationResult::Success)
	{
		Refresh();
		Asset->OnGraphVariablesChanged.Broadcast();
	}
}

FReply SFlowGraphVariablesPanel::AddNewVariableAction()
{
	AddNewVariable();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
