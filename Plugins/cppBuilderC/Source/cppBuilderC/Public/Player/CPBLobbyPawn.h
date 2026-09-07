#pragma once

#include "CoreMinimal.h"
#include "Player/CPBMainPawn.h"
#include "CPBLobbyPawn.generated.h"

UCLASS(Blueprintable)
class CPPBUILDERC_API ACPBLobbyPawn : public ACPBMainPawn
{
	GENERATED_BODY()

public:
	ACPBLobbyPawn();
};
