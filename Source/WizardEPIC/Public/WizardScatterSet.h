// WizardScatterSet.h - instanced meshes placed by the forge (grass clumps, bushes, flowers,
// small stones, planks). A plain container without gameplay behaviour.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WizardScatterSet.generated.h"

class UInstancedStaticMeshComponent;

UCLASS()
class WIZARDEPIC_API AWizardScatterSet : public AActor
{
	GENERATED_BODY()

public:
	AWizardScatterSet();

	/** Up to 8 instanced layers. The forge sets a static mesh on a layer and adds instances to it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Scatter)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> InstanceLayers;

	/** Adds an instance in actor space. Returns its index or -1. */
	UFUNCTION(BlueprintCallable, Category = Wizard)
	int32 AddInstance(int32 LayerIndex, const FTransform& Transform);

	UFUNCTION(BlueprintCallable, Category = Wizard)
	int32 AddInstances(int32 LayerIndex, const TArray<FTransform>& Transforms);

	UFUNCTION(BlueprintCallable, Category = Wizard)
	void ClearLayer(int32 LayerIndex);

	UFUNCTION(BlueprintPure, Category = Wizard)
	int32 GetLayerCount() const;
};
