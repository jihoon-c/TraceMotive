#include "Interaction/CPBDissolveInteractionComponent.h"

#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

UCPBDissolveInteractionComponent::UCPBDissolveInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCPBDissolveInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshDynamicMaterials();
}

void UCPBDissolveInteractionComponent::RefreshDynamicMaterials()
{
	DynamicMaterials.Reset();

	if (TargetMeshComponents.Num() == 0)
	{
		if (AActor* Owner = GetOwner())
		{
			TArray<UMeshComponent*> MeshComponents;
			Owner->GetComponents<UMeshComponent>(MeshComponents);
			for (UMeshComponent* MeshComponent : MeshComponents)
			{
				TargetMeshComponents.Add(MeshComponent);
			}
		}
	}

	for (UMeshComponent* MeshComponent : TargetMeshComponents)
	{
		if (!MeshComponent)
		{
			continue;
		}

		const int32 MaterialCount = MeshComponent->GetNumMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			UMaterialInstanceDynamic* DynamicMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamic(MaterialIndex);
			if (DynamicMaterial)
			{
				DynamicMaterials.Add(DynamicMaterial);
			}
		}
	}
}

void UCPBDissolveInteractionComponent::SetDissolveAmount(float DissolveAmount)
{
	for (UMaterialInstanceDynamic* DynamicMaterial : DynamicMaterials)
	{
		if (DynamicMaterial)
		{
			DynamicMaterial->SetScalarParameterValue(DissolveParameterName, DissolveAmount);
		}
	}
}

void UCPBDissolveInteractionComponent::BeginDissolve(float TargetDissolveAmount)
{
	SetDissolveAmount(TargetDissolveAmount);
}
