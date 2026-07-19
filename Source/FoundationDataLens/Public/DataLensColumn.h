/**************************************************************************************
*                                                                                     *
* Copyright   2026 by Heathen Engineering Limited, an Irish registered company        *
* # 556277, VAT IE3394133CH, contact Heathen via support@heathen.group                *
*                                                                                     *
***************************************************************************************/

#pragma once

#include <cstdint>
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

enum class EDataLensColumnType : uint8
{
	Float,
	Int32,
};

/**
 * DataLens (and every other Heathen DOA tool) works entirely in raw uint64 column/tag identity --
 * no wrapper type. Unreal's native FGameplayTag stays the one real tag concept (hierarchy,
 * picker, authoring); this is a pure, stateless derivation from it (XXH3_64bits over the tag's
 * UTF-8 string, seed 0 -- the same convention every other engine's GameplayTags/Lexicon already
 * share, verified bit-identical against UE's own native FXxHash64::HashBuffer). Call it once at
 * authoring/schema-declaration time and keep the uint64; never call it on a hot path.
 */
FOUNDATIONDATALENS_API uint64 HashDataLensTag(const FGameplayTag& GameplayTag);

/** A single column in a DataLens schema: its identity (a raw uint64 id) and byte width. */
struct FOUNDATIONDATALENS_API FDataLensColumn
{
	uint64 Id = 0;
	EDataLensColumnType Type = EDataLensColumnType::Float;
	uint32 Stride = 0;

	static FDataLensColumn OfFloat(uint64 ColumnId);
	static FDataLensColumn OfInt32(uint64 ColumnId);

	/** Convenience: hashes GameplayTag via HashDataLensTag and forwards. Fine for schema
	 * declaration (a one-time/setup-time operation); not for any hot path. */
	static FDataLensColumn OfFloat(const FGameplayTag& GameplayTag);
	static FDataLensColumn OfInt32(const FGameplayTag& GameplayTag);
};
