/**************************************************************************************
*                                                                                     *
* Copyright   2026 by Heathen Engineering Limited, an Irish registered company        *
* # 556277, VAT IE3394133CH, contact Heathen via support@heathen.group                *
*                                                                                     *
***************************************************************************************/

#pragma once

#include <cstdint>
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "DataLensColumn.h"
#include "DataLensView.h"
#include "DataLensSystem.h"
#include "DataLensSubsystem.generated.h"

struct dl_store;
struct dl_lens;

/**
 * Every column/tag identity in this API is a raw uint64 -- no wrapper type. Unreal's native
 * FGameplayTag stays the one real tag concept (hierarchy, picker, authoring); an FGameplayTag
 * overload exists wherever a dev would plausibly reach for one, and does nothing but hash it via
 * HashDataLensTag and forward to the uint64 version -- never store or wrap it. Nothing in this
 * class is UFUNCTION/Blueprint-exposed; a future UDataLensBlueprintLibrary (M5) is the only place
 * an int64<->uint64 reinterpret_cast for Blueprint's benefit belongs (UHT doesn't support uint64
 * in reflected signatures at all -- see ToolkitSteamworks' own int64-for-SteamID precedent).
 *
 * M3 scope: a Lens (owns a thread pool, runs Systems) plus scalar/cross-column/curved System
 * entry points, each with an optional per-row predicate gate. Still no replication primitives
 * (M4) or Blueprint exposure (M5) -- see docs/products/datalens/00-overview.md
 * (Unreal-DataLens-Foundation's consuming repo, SourceRepo/Unreal/ToolkitSource) for the full
 * milestone breakdown.
 *
 * Scope: UGameInstanceSubsystem -- one Lens/store set per play session, matching a Lens's
 * thread-pool+stores lifetime more closely than an EngineSubsystem (wrong granularity, would
 * outlive the session that created the stores) or a WorldSubsystem (recreated per level, too
 * narrow for cross-level state). Revisit if a project specifically wants per-streamed-level
 * DataLens state instead.
 *
 * NewObject<UDataLensSubsystem>() needs a real UGameInstance as its Outer --
 * UGameInstanceSubsystem is UCLASS(Abstract, Within = GameInstance), so a bare/transient-package
 * Outer trips an editor-only ensure (see the M1 result notes in the design doc).
 */
// BlueprintType only -- lets a Blueprint hold/pass a reference (e.g. from "Get Game Instance
// Subsystem") to feed into UDataLensBlueprintLibrary's static functions. No UFUNCTION on this
// class itself; all Blueprint-callable boundary conversion lives in that separate library (M5),
// per the class comment above.
UCLASS(BlueprintType)
class FOUNDATIONDATALENS_API UDataLensSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Builds the store from a schema. Fails if a store already exists, Columns is empty, any
	 * two columns resolve to the same id, or PreallocRows is zero. */
	bool CreateStore(const TArray<FDataLensColumn>& Columns, uint64 PreallocRows);

	/** Allocates a row. Returns UINT64_MAX if no store exists or it's at capacity. */
	uint64 AllocRow();

	void FreeRow(uint64 Row);

	bool SetFloat(uint64 ColumnId, uint64 Row, float Value);
	bool SetFloat(const FGameplayTag& ColumnTag, uint64 Row, float Value);

	bool GetFloat(uint64 ColumnId, uint64 Row, float& OutValue) const;
	bool GetFloat(const FGameplayTag& ColumnTag, uint64 Row, float& OutValue) const;

	bool SetInt32(uint64 ColumnId, uint64 Row, int32 Value);
	bool SetInt32(const FGameplayTag& ColumnTag, uint64 Row, int32 Value);

	bool GetInt32(uint64 ColumnId, uint64 Row, int32& OutValue) const;
	bool GetInt32(const FGameplayTag& ColumnTag, uint64 Row, int32& OutValue) const;

	void SetValid(uint64 Row, bool bValid);
	bool IsValid(uint64 Row) const;

	bool HasStore() const { return Store != nullptr; }

	/** True if ColumnId was declared in the schema passed to CreateStore. */
	bool HasColumn(uint64 ColumnId) const;
	bool HasColumn(const FGameplayTag& ColumnTag) const;

	/**
	 * Builds a read-only, row-major snapshot view over the given columns (in the given order) and
	 * refreshes it against the current store state. Returns an unset TOptional if no store
	 * exists or any column isn't part of the schema.
	 */
	TOptional<FDataLensView> CreateView(const TArray<uint64>& ColumnIds) const;
	TOptional<FDataLensView> CreateView(const TArray<FGameplayTag>& ColumnTags) const;

	/** Unconditional scalar System: TargetId = TargetId OP Operand, every live row. Returns rows
	 * affected, or 0 if no store exists / TargetId isn't in the schema. */
	uint64 RunSystemFloat(uint64 TargetId, EDataLensSystemOp Op, float Operand);
	uint64 RunSystemFloat(const FGameplayTag& TargetTag, EDataLensSystemOp Op, float Operand);

	/** As RunSystemFloat, but only applied to rows where (CompareId CMP Threshold) holds. Returns
	 * 0 (not an error, just zero rows affected) if CompareId isn't in the schema either. */
	uint64 RunSystemFloatIf(uint64 TargetId, EDataLensSystemOp Op, float Operand,
		uint64 CompareId, EDataLensCompareOp Compare, float Threshold);
	uint64 RunSystemFloatIf(const FGameplayTag& TargetTag, EDataLensSystemOp Op, float Operand,
		const FGameplayTag& CompareTag, EDataLensCompareOp Compare, float Threshold);

	// Int32 gets scalar Systems only for M3 (matching M2's Float/Int32 asymmetry) -- Core's C ABI
	// has cross-column and curved Int32 entry points too (dl_lens_run_col_i32/dl_lens_run_curved_i32),
	// straightforward to add later on real demand, just not duplicated here up front.
	uint64 RunSystemInt32(uint64 TargetId, EDataLensSystemOp Op, int32 Operand);
	uint64 RunSystemInt32(const FGameplayTag& TargetTag, EDataLensSystemOp Op, int32 Operand);

	uint64 RunSystemInt32If(uint64 TargetId, EDataLensSystemOp Op, int32 Operand,
		uint64 CompareId, EDataLensCompareOp Compare, int32 Threshold);
	uint64 RunSystemInt32If(const FGameplayTag& TargetTag, EDataLensSystemOp Op, int32 Operand,
		const FGameplayTag& CompareTag, EDataLensCompareOp Compare, int32 Threshold);

	/** Cross-column System: TargetId = TargetId OP (per-row value of OperandId). */
	uint64 RunSystemColumnFloat(uint64 TargetId, EDataLensSystemOp Op, uint64 OperandId);
	uint64 RunSystemColumnFloat(const FGameplayTag& TargetTag, EDataLensSystemOp Op, const FGameplayTag& OperandTag);

	uint64 RunSystemColumnFloatIf(uint64 TargetId, EDataLensSystemOp Op, uint64 OperandId,
		uint64 CompareId, EDataLensCompareOp Compare, float Threshold);
	uint64 RunSystemColumnFloatIf(const FGameplayTag& TargetTag, EDataLensSystemOp Op, const FGameplayTag& OperandTag,
		const FGameplayTag& CompareTag, EDataLensCompareOp Compare, float Threshold);

	/** Cross-column System where the per-row operand is passed through a response curve (normalised
	 * over [CurveMin, CurveMax], curved, optionally inverted) before the combine. */
	uint64 RunSystemCurvedFloat(uint64 TargetId, EDataLensSystemOp Op, uint64 OperandId,
		EDataLensCurveType CurveType, float CurveMin, float CurveMax, float CurveP0, float CurveP1, bool bInvertCurve);
	uint64 RunSystemCurvedFloat(const FGameplayTag& TargetTag, EDataLensSystemOp Op, const FGameplayTag& OperandTag,
		EDataLensCurveType CurveType, float CurveMin, float CurveMax, float CurveP0, float CurveP1, bool bInvertCurve);

	uint64 RunSystemCurvedFloatIf(uint64 TargetId, EDataLensSystemOp Op, uint64 OperandId,
		EDataLensCurveType CurveType, float CurveMin, float CurveMax, float CurveP0, float CurveP1, bool bInvertCurve,
		uint64 CompareId, EDataLensCompareOp Compare, float Threshold);
	uint64 RunSystemCurvedFloatIf(const FGameplayTag& TargetTag, EDataLensSystemOp Op, const FGameplayTag& OperandTag,
		EDataLensCurveType CurveType, float CurveMin, float CurveMax, float CurveP0, float CurveP1, bool bInvertCurve,
		const FGameplayTag& CompareTag, EDataLensCompareOp Compare, float Threshold);

	// ---- Replication primitives (M4). Engine-neutral state snapshot/delta (DataLens-Spec.md §10)
	// -- a provider, not a transport: this plugin opens no socket and picks no netcode stack. A
	// project's own replication layer (Iris, a custom RPC blob, whatever) is the thing that
	// actually moves these payloads between machines. Unscoped only for this slice (scope = a
	// row-index interest-management bitmask in Core's own terms) -- a real scoped variant is
	// deferred until there's a concrete interest-management consumer to design it against.

	/** The store's monotonic replication revision counter. 0 if no store exists. */
	uint64 GetRevision() const;
	void SetRevision(uint64 Revision);
	/** Advances the revision by one and returns the new value -- call once per network tick on the
	 * authoritative side, per Core's own doc comment. 0 if no store exists. */
	uint64 BumpRevision();

	/** Marks a column as changed since the last revision, so CollectDelta reports it. Every
	 * RunSystem, RunSystemColumn, and RunSystemCurved call already does this for its target column
	 * (Core's own Lens::RunSystem calls store.MarkColumnDirty internally) -- this is only needed
	 * for a column mutated some other way (e.g. a plain SetFloat/SetInt32 write). */
	void MarkColumnDirty(uint64 ColumnId);
	void MarkColumnDirty(const FGameplayTag& ColumnTag);

	/** A full scoped baseline of the current store state. Empty array if no store exists. */
	TArray<uint8> Snapshot();

	/** Changes since SinceRevision (falls back to a full snapshot if SinceRevision has lagged past
	 * Core's change-tracking horizon). Empty array if no store exists. */
	TArray<uint8> CollectDelta(uint64 SinceRevision);

	/** Applies a snapshot or delta payload (from Snapshot/CollectDelta, possibly on a different
	 * UDataLensSubsystem with an identical schema) as an authoritative commit. False on a malformed
	 * or schema-mismatched payload, or if no store exists -- never crashes on bad input. */
	bool ApplyPayload(const TArray<uint8>& Payload);

private:
	dl_store* Store = nullptr;
	dl_lens* Lens = nullptr;

	// Column id -> store column index (creation order from CreateStore).
	TMap<uint64, int32> ColumnIndexById;

	bool FindColumnIndex(uint64 ColumnId, uint64& OutIndex) const;

	/** Lazily creates Lens if it's still null -- see the .cpp for why this matters (Initialize()
	 * only runs when a real UGameInstance::Init() drives the subsystem collection, not when a
	 * UGameInstanceSubsystem is constructed directly via NewObject). */
	void EnsureLens();
};
