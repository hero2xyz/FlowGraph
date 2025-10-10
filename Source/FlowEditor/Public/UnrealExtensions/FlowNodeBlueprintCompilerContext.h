// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "CoreMinimal.h"
#include "KismetCompiler.h"

class UFlowNodeBaseBlueprint;

/**
 * Compiler context for Flow blueprints.
 * Processes variable metadata to configure data pins.
 */
class FFlowNodeBlueprintCompilerContext : public FKismetCompilerContext
{
public:
	FFlowNodeBlueprintCompilerContext(UBlueprint* InBlueprint, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions);

	// FKismetCompilerContext
	virtual void OnNewClassSet(UBlueprintGeneratedClass* ClassToUse) override;
	virtual void SpawnNewClass(const FString& NewClassName) override;
	virtual void EnsureProperGeneratedClass(UClass*& InTargetClass) override;
	virtual bool ValidateGeneratedClass(UBlueprintGeneratedClass* Class) override;
	virtual void PreCompile() override;
	// End FKismetCompilerContext
private:
	/** Process blueprint variables for Flow-specific metadata */
	void ProcessBlueprintVariables(class UFlowNodeBaseBlueprintGeneratedClass* FlowClass) const;
};

/**
 * Register the Flow blueprint compiler with the Kismet compiler system.
 */
void RegisterFlowNodeBlueprintCompiler();
