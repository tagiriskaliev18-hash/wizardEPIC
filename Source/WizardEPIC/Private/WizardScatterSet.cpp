#include "WizardScatterSet.h"

#include "Components/InstancedStaticMeshComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardScatterSet)

AWizardScatterSet::AWizardScatterSet()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	for (int32 Index = 0; Index < 8; ++Index)
	{
		UInstancedStaticMeshComponent* Layer = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("Layer%d"), Index));
		Layer->SetupAttachment(Root);
		Layer->SetMobility(EComponentMobility::Movable);
		InstanceLayers.Add(Layer);
	}
}

int32 AWizardScatterSet::AddInstance(int32 LayerIndex, const FTransform& Transform)
{
	if (!InstanceLayers.IsValidIndex(LayerIndex))
	{
		return -1;
	}
	return InstanceLayers[LayerIndex]->AddInstance(Transform, false);
}

int32 AWizardScatterSet::AddInstances(int32 LayerIndex, const TArray<FTransform>& Transforms)
{
	if (!InstanceLayers.IsValidIndex(LayerIndex) || Transforms.IsEmpty())
	{
		return -1;
	}
	const int32 Before = InstanceLayers[LayerIndex]->GetInstanceCount();
	InstanceLayers[LayerIndex]->AddInstances(Transforms, false, false);
	return Before;
}

void AWizardScatterSet::ClearLayer(int32 LayerIndex)
{
	if (InstanceLayers.IsValidIndex(LayerIndex))
	{
		InstanceLayers[LayerIndex]->ClearInstances();
	}
}

int32 AWizardScatterSet::GetLayerCount() const
{
	return InstanceLayers.Num();
}
