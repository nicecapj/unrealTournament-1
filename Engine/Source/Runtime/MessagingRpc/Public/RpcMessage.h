// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "RpcMessage.generated.h"


/**
 * Base type for RPC messages.
 */
USTRUCT()
struct FRpcMessage
{
	GENERATED_USTRUCT_BODY()

	/** Correlation identifier for the RPC call that this message refers to. */
	UPROPERTY()
	FGuid CallId;

	/** Default constructor. */
	FRpcMessage() { }
};


#define DECLARE_RPC(RpcType, ResultType) \
	struct RpcType \
	{ \
		typedef RpcType##Request FRequest; \
		typedef RpcType##Response FResponse; \
		typedef ResultType FResult; \
	};
