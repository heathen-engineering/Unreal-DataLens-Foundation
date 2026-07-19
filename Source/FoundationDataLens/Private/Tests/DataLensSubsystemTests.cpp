/**************************************************************************************
*                                                                                     *
* Copyright   2026 by Heathen Engineering Limited, an Irish registered company        *
* # 556277, VAT IE3394133CH, contact Heathen via support@heathen.group                *
*                                                                                     *
***************************************************************************************/

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "GameplayTagsManager.h"
#include "DataLensSubsystem.h"
#include "DataLensColumn.h"
#include "DataLensView.h"
#include "DataLensBlueprintLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

// M2 coverage target: schema (multiple typed columns), XXH3 tag-hash resolution, and read-only
// multi-column views -- on top of M1's store creation / typed round-trip / bounds rejection /
// validity toggling (still covered below, updated for the multi-column CreateStore signature).
//
// UGameInstanceSubsystem is UCLASS(Abstract, Within = GameInstance, ...), so every
// NewObject<UDataLensSubsystem>() needs a real UGameInstance as its Outer (see the M1 result notes
// in docs/products/datalens/00-overview.md) -- a bare NewObject<UGameInstance>() has no Within
// constraint of its own, so it's a valid, cheap stand-in Outer for these tests.
static UDataLensSubsystem* MakeTestSubsystem()
{
	UGameInstance* DummyGameInstance = NewObject<UGameInstance>();
	return NewObject<UDataLensSubsystem>(DummyGameInstance);
}

// FGameplayTag::RequestGameplayTag() only resolves tags already registered from config/native
// sources -- for an ad hoc test tag that isn't in this throwaway host project's tag table, it
// trips an ensureAlwaysMsgf ("Requested Gameplay Tag ... was not found") AND, critically, still
// returns an EMPTY FGameplayTag regardless of the ErrorIfNotFound flag. Every "different" test
// tag would silently collapse to the same empty tag, breaking every test that assumes two
// distinctly-named columns actually hash differently. AddNativeGameplayTag registers (idempotently
// -- safe to call with the same name repeatedly) and resolves in one step, which is the correct
// tool for tests that need real, distinct, ad hoc tags with no project config behind them.
static FGameplayTag MakeTestTag(const TCHAR* TagName)
{
	return UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TagName));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensStoreCreationTest,
	"Heathen.FoundationDataLens.StoreCreation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensStoreCreationTest::RunTest(const FString& Parameters)
{
	UDataLensSubsystem* Subsystem = MakeTestSubsystem();
	TestFalse(TEXT("HasStore is false before any store is created"), Subsystem->HasStore());

	const FGameplayTag HealthTag = MakeTestTag(TEXT("Test.Attribute.Health"));
	const TArray<FDataLensColumn> Columns = { FDataLensColumn::OfFloat(HealthTag) };

	TestTrue(TEXT("CreateStore succeeds with valid params"), Subsystem->CreateStore(Columns, /*PreallocRows*/ 4));
	TestTrue(TEXT("HasStore is true after a successful create"), Subsystem->HasStore());
	TestTrue(TEXT("HasColumn is true for a declared column"), Subsystem->HasColumn(HealthTag));

	// A second create on an already-populated subsystem must fail, not silently replace the store.
	TestFalse(TEXT("CreateStore fails when a store already exists"), Subsystem->CreateStore(Columns, 4));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensStoreCreationRejectsZeroRowsTest,
	"Heathen.FoundationDataLens.StoreCreationRejectsZeroRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensStoreCreationRejectsZeroRowsTest::RunTest(const FString& Parameters)
{
	UDataLensSubsystem* Subsystem = MakeTestSubsystem();
	const TArray<FDataLensColumn> Columns = { FDataLensColumn::OfFloat(MakeTestTag(TEXT("Test.Attribute.Health"))) };

	TestFalse(TEXT("CreateStore fails with zero preallocated rows"), Subsystem->CreateStore(Columns, /*PreallocRows*/ 0));
	TestFalse(TEXT("HasStore stays false after a rejected create"), Subsystem->HasStore());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensStoreCreationRejectsDuplicateColumnTest,
	"Heathen.FoundationDataLens.StoreCreationRejectsDuplicateColumn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensStoreCreationRejectsDuplicateColumnTest::RunTest(const FString& Parameters)
{
	UDataLensSubsystem* Subsystem = MakeTestSubsystem();
	const FGameplayTag HealthTag = MakeTestTag(TEXT("Test.Attribute.Health"));

	// Two columns resolving to the same tag hash must be rejected as an invalid schema, not
	// silently deduplicated or allowed to shadow each other.
	const TArray<FDataLensColumn> Columns = { FDataLensColumn::OfFloat(HealthTag), FDataLensColumn::OfInt32(HealthTag) };
	TestFalse(TEXT("CreateStore fails when two columns share a tag"), Subsystem->CreateStore(Columns, 4));
	TestFalse(TEXT("HasStore stays false after a rejected duplicate-column schema"), Subsystem->HasStore());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensTagHashParityTest,
	"Heathen.FoundationDataLens.TagHashParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensTagHashParityTest::RunTest(const FString& Parameters)
{
	const FGameplayTag TagA = MakeTestTag(TEXT("Test.Attribute.Health"));
	const FGameplayTag TagB = MakeTestTag(TEXT("Test.Attribute.Stamina"));

	TestEqual(TEXT("Hashing the same tag twice is deterministic"), HashDataLensTag(TagA), HashDataLensTag(TagA));
	TestNotEqual(TEXT("Different tags hash to different values"), HashDataLensTag(TagA), HashDataLensTag(TagB));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensTypedRoundTripTest,
	"Heathen.FoundationDataLens.TypedRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensTypedRoundTripTest::RunTest(const FString& Parameters)
{
	UDataLensSubsystem* Subsystem = MakeTestSubsystem();
	const FGameplayTag HealthTag = MakeTestTag(TEXT("Test.Attribute.Health"));
	const FGameplayTag LevelTag = MakeTestTag(TEXT("Test.Attribute.Level"));

	Subsystem->CreateStore({ FDataLensColumn::OfFloat(HealthTag), FDataLensColumn::OfInt32(LevelTag) }, 4);

	const uint64 Row = Subsystem->AllocRow();
	TestTrue(TEXT("AllocRow returns a valid row on a fresh store"), Row != UINT64_MAX);

	TestTrue(TEXT("SetFloat succeeds for an allocated row/known column"), Subsystem->SetFloat(HealthTag, Row, 3.5f));
	TestTrue(TEXT("SetInt32 succeeds for a second column on the same row"), Subsystem->SetInt32(LevelTag, Row, 7));

	float OutFloat = 0.0f;
	int32 OutInt = 0;
	TestTrue(TEXT("GetFloat succeeds for the same row/column"), Subsystem->GetFloat(HealthTag, Row, OutFloat));
	TestEqual(TEXT("GetFloat returns exactly what was written"), OutFloat, 3.5f);
	TestTrue(TEXT("GetInt32 succeeds for the same row/column"), Subsystem->GetInt32(LevelTag, Row, OutInt));
	TestEqual(TEXT("GetInt32 returns exactly what was written"), OutInt, 7);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensBoundsRejectionTest,
	"Heathen.FoundationDataLens.BoundsRejection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensBoundsRejectionTest::RunTest(const FString& Parameters)
{
	UDataLensSubsystem* Subsystem = MakeTestSubsystem();
	const FGameplayTag HealthTag = MakeTestTag(TEXT("Test.Attribute.Health"));
	Subsystem->CreateStore({ FDataLensColumn::OfFloat(HealthTag) }, /*PreallocRows*/ 2);
	const uint64 Row = Subsystem->AllocRow();

	// Out-of-range row: must fail cleanly, not crash.
	const uint64 OutOfRangeRow = Row + 1000;
	TestFalse(TEXT("SetFloat fails for an out-of-range row"), Subsystem->SetFloat(HealthTag, OutOfRangeRow, 1.0f));

	float OutValue = 0.0f;
	TestFalse(TEXT("GetFloat fails for an out-of-range row"), Subsystem->GetFloat(HealthTag, OutOfRangeRow, OutValue));

	// Unknown column tag: must fail cleanly.
	const FGameplayTag UnknownTag = MakeTestTag(TEXT("Test.Attribute.NotInSchema"));
	TestFalse(TEXT("SetFloat fails for a tag not in the schema"), Subsystem->SetFloat(UnknownTag, Row, 1.0f));
	TestFalse(TEXT("HasColumn is false for a tag not in the schema"), Subsystem->HasColumn(UnknownTag));

	// Operating on a subsystem with no store at all must also fail cleanly.
	UDataLensSubsystem* EmptySubsystem = MakeTestSubsystem();
	TestFalse(TEXT("SetFloat fails when no store exists"), EmptySubsystem->SetFloat(HealthTag, 0, 1.0f));
	TestFalse(TEXT("GetFloat fails when no store exists"), EmptySubsystem->GetFloat(HealthTag, 0, OutValue));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensValidityToggleTest,
	"Heathen.FoundationDataLens.ValidityToggle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensValidityToggleTest::RunTest(const FString& Parameters)
{
	UDataLensSubsystem* Subsystem = MakeTestSubsystem();
	Subsystem->CreateStore({ FDataLensColumn::OfFloat(MakeTestTag(TEXT("Test.Attribute.Health"))) }, 4);
	const uint64 Row = Subsystem->AllocRow();

	Subsystem->SetValid(Row, true);
	TestTrue(TEXT("IsValid reflects a true SetValid"), Subsystem->IsValid(Row));

	Subsystem->SetValid(Row, false);
	TestFalse(TEXT("IsValid reflects a false SetValid"), Subsystem->IsValid(Row));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensViewMultiColumnTest,
	"Heathen.FoundationDataLens.ViewMultiColumn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensViewMultiColumnTest::RunTest(const FString& Parameters)
{
	UDataLensSubsystem* Subsystem = MakeTestSubsystem();
	const FGameplayTag HealthTag = MakeTestTag(TEXT("Test.Attribute.Health"));
	const FGameplayTag LevelTag = MakeTestTag(TEXT("Test.Attribute.Level"));

	Subsystem->CreateStore({ FDataLensColumn::OfFloat(HealthTag), FDataLensColumn::OfInt32(LevelTag) }, 4);

	const uint64 RowA = Subsystem->AllocRow();
	Subsystem->SetValid(RowA, true);
	Subsystem->SetFloat(HealthTag, RowA, 10.0f);
	Subsystem->SetInt32(LevelTag, RowA, 1);

	const uint64 RowB = Subsystem->AllocRow();
	Subsystem->SetValid(RowB, true);
	Subsystem->SetFloat(HealthTag, RowB, 20.0f);
	Subsystem->SetInt32(LevelTag, RowB, 2);

	TOptional<FDataLensView> View = Subsystem->CreateView({ HealthTag, LevelTag });
	TestTrue(TEXT("CreateView succeeds for columns that are all in the schema"), View.IsSet());
	if (!View.IsSet())
	{
		return false;
	}

	TestEqual(TEXT("View gathers exactly the live rows"), View->RowCount(), (uint64)2);
	TestEqual(TEXT("View reports the requested column count"), View->ColumnCount(), (uint64)2);

	// Row order in the snapshot isn't guaranteed to match allocation order, so check both possible
	// live rows are present rather than assuming index 0 is RowA.
	float FirstHealth = 0.0f, SecondHealth = 0.0f;
	View->GetFloat(0, 0, FirstHealth);
	View->GetFloat(1, 0, SecondHealth);
	const bool bFoundBothHealthValues =
		(FMath::IsNearlyEqual(FirstHealth, 10.0f) && FMath::IsNearlyEqual(SecondHealth, 20.0f)) ||
		(FMath::IsNearlyEqual(FirstHealth, 20.0f) && FMath::IsNearlyEqual(SecondHealth, 10.0f));
	TestTrue(TEXT("View's health column contains both written values"), bFoundBothHealthValues);

	// Unknown column tag in the requested list must fail the whole CreateView call.
	const FGameplayTag UnknownTag = MakeTestTag(TEXT("Test.Attribute.NotInSchema"));
	TOptional<FDataLensView> BadView = Subsystem->CreateView({ HealthTag, UnknownTag });
	TestFalse(TEXT("CreateView fails if any requested column isn't in the schema"), BadView.IsSet());

	return true;
}

// M3 coverage target: unconditional scalar System, predicated scalar System (both the row that
// passes and the row that's gated out), cross-column System, curved System, and the
// unresolvable-tag failure paths (all of which should return 0 rows affected, not crash).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensRunSystemFloatTest,
	"Heathen.FoundationDataLens.RunSystemFloat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensRunSystemFloatTest::RunTest(const FString& Parameters)
{
	UDataLensSubsystem* Subsystem = MakeTestSubsystem();
	const FGameplayTag HealthTag = MakeTestTag(TEXT("Test.Attribute.Health"));
	Subsystem->CreateStore({ FDataLensColumn::OfFloat(HealthTag) }, 4);

	const uint64 RowA = Subsystem->AllocRow();
	Subsystem->SetValid(RowA, true);
	Subsystem->SetFloat(HealthTag, RowA, 10.0f);

	const uint64 RowB = Subsystem->AllocRow();
	Subsystem->SetValid(RowB, true);
	Subsystem->SetFloat(HealthTag, RowB, 5.0f);

	const uint64 Affected = Subsystem->RunSystemFloat(HealthTag, EDataLensSystemOp::Add, 1.0f);
	TestEqual(TEXT("Unconditional System applies to every live row"), Affected, (uint64)2);

	float RowAValue = 0.0f, RowBValue = 0.0f;
	Subsystem->GetFloat(HealthTag, RowA, RowAValue);
	Subsystem->GetFloat(HealthTag, RowB, RowBValue);
	TestEqual(TEXT("RowA got +1"), RowAValue, 11.0f);
	TestEqual(TEXT("RowB got +1"), RowBValue, 6.0f);

	// Unresolvable target tag must fail cleanly (0 rows affected), not crash.
	const FGameplayTag UnknownTag = MakeTestTag(TEXT("Test.Attribute.NotInSchema"));
	TestEqual(TEXT("RunSystemFloat on an unknown tag affects 0 rows"), Subsystem->RunSystemFloat(UnknownTag, EDataLensSystemOp::Add, 1.0f), (uint64)0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensRunSystemFloatPredicatedTest,
	"Heathen.FoundationDataLens.RunSystemFloatPredicated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensRunSystemFloatPredicatedTest::RunTest(const FString& Parameters)
{
	// dl_lens_run_f32's predicate reads compareCol as the same element type (f32) as the System
	// itself, so the gate column here is a float ("Threshold"), not an int32 one.
	const FGameplayTag HealthTag = MakeTestTag(TEXT("Test.Attribute.Health"));
	const FGameplayTag ThresholdTag = MakeTestTag(TEXT("Test.Attribute.Threshold"));

	UDataLensSubsystem* PredicateSubsystem = MakeTestSubsystem();
	PredicateSubsystem->CreateStore({ FDataLensColumn::OfFloat(HealthTag), FDataLensColumn::OfFloat(ThresholdTag) }, 4);

	const uint64 PassRow = PredicateSubsystem->AllocRow();
	PredicateSubsystem->SetValid(PassRow, true);
	PredicateSubsystem->SetFloat(HealthTag, PassRow, 10.0f);
	PredicateSubsystem->SetFloat(ThresholdTag, PassRow, 10.0f); // 10 > 5 -> gate passes

	const uint64 FailRow = PredicateSubsystem->AllocRow();
	PredicateSubsystem->SetValid(FailRow, true);
	PredicateSubsystem->SetFloat(HealthTag, FailRow, 10.0f);
	PredicateSubsystem->SetFloat(ThresholdTag, FailRow, 1.0f); // 1 > 5 is false -> gate blocks

	const uint64 Affected = PredicateSubsystem->RunSystemFloatIf(HealthTag, EDataLensSystemOp::Add, 1.0f,
		ThresholdTag, EDataLensCompareOp::Greater, 5.0f);
	TestEqual(TEXT("Predicated System affects only the row that passes the gate"), Affected, (uint64)1);

	float PassValue = 0.0f, FailValue = 0.0f;
	PredicateSubsystem->GetFloat(HealthTag, PassRow, PassValue);
	PredicateSubsystem->GetFloat(HealthTag, FailRow, FailValue);
	TestEqual(TEXT("The passing row got +1"), PassValue, 11.0f);
	TestEqual(TEXT("The gated-out row is untouched"), FailValue, 10.0f);

	// Unresolvable compare tag must fail cleanly (0 rows affected), not crash.
	const FGameplayTag UnknownTag = MakeTestTag(TEXT("Test.Attribute.NotInSchema"));
	TestEqual(TEXT("RunSystemFloatIf with an unknown compare tag affects 0 rows"),
		PredicateSubsystem->RunSystemFloatIf(HealthTag, EDataLensSystemOp::Add, 1.0f, UnknownTag, EDataLensCompareOp::Greater, 0.0f),
		(uint64)0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensRunSystemColumnFloatTest,
	"Heathen.FoundationDataLens.RunSystemColumnFloat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensRunSystemColumnFloatTest::RunTest(const FString& Parameters)
{
	UDataLensSubsystem* Subsystem = MakeTestSubsystem();
	const FGameplayTag HealthTag = MakeTestTag(TEXT("Test.Attribute.Health"));
	const FGameplayTag RegenTag = MakeTestTag(TEXT("Test.Attribute.Regen"));
	Subsystem->CreateStore({ FDataLensColumn::OfFloat(HealthTag), FDataLensColumn::OfFloat(RegenTag) }, 4);

	const uint64 Row = Subsystem->AllocRow();
	Subsystem->SetValid(Row, true);
	Subsystem->SetFloat(HealthTag, Row, 10.0f);
	Subsystem->SetFloat(RegenTag, Row, 2.5f);

	const uint64 Affected = Subsystem->RunSystemColumnFloat(HealthTag, EDataLensSystemOp::Add, RegenTag);
	TestEqual(TEXT("Cross-column System affects the one live row"), Affected, (uint64)1);

	float HealthValue = 0.0f;
	Subsystem->GetFloat(HealthTag, Row, HealthValue);
	TestEqual(TEXT("Health increased by the Regen column's per-row value"), HealthValue, 12.5f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensRunSystemCurvedFloatTest,
	"Heathen.FoundationDataLens.RunSystemCurvedFloat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensRunSystemCurvedFloatTest::RunTest(const FString& Parameters)
{
	UDataLensSubsystem* Subsystem = MakeTestSubsystem();
	const FGameplayTag HealthTag = MakeTestTag(TEXT("Test.Attribute.Health"));
	const FGameplayTag ThreatTag = MakeTestTag(TEXT("Test.Attribute.Threat"));
	Subsystem->CreateStore({ FDataLensColumn::OfFloat(HealthTag), FDataLensColumn::OfFloat(ThreatTag) }, 4);

	const uint64 Row = Subsystem->AllocRow();
	Subsystem->SetValid(Row, true);
	Subsystem->SetFloat(HealthTag, Row, 100.0f);
	Subsystem->SetFloat(ThreatTag, Row, 5.0f); // normalises to 0.5 over [0,10] with a Linear curve

	const uint64 Affected = Subsystem->RunSystemCurvedFloat(HealthTag, EDataLensSystemOp::Sub, ThreatTag,
		EDataLensCurveType::Linear, /*CurveMin*/ 0.0f, /*CurveMax*/ 10.0f, /*CurveP0*/ 1.0f, /*CurveP1*/ 0.0f,
		/*bInvertCurve*/ false);
	TestEqual(TEXT("Curved System affects the one live row"), Affected, (uint64)1);

	float HealthValue = 0.0f;
	Subsystem->GetFloat(HealthTag, Row, HealthValue);
	// Threat=5 normalised over [0,10] -> x=0.5; Linear curve y = p0*x + p1 = 1*0.5 + 0 = 0.5 -> Health -= 0.5
	TestEqual(TEXT("Health decreased by the curved, normalised Threat value"), HealthValue, 99.5f);

	return true;
}

// M4 coverage target: revision round-trip, a snapshot taken on one subsystem correctly landing on
// a second subsystem with an identical schema (the actual "replication replicates" proof), a delta
// round trip, and malformed-payload rejection (must fail cleanly, not crash).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensRevisionRoundTripTest,
	"Heathen.FoundationDataLens.RevisionRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensRevisionRoundTripTest::RunTest(const FString& Parameters)
{
	UDataLensSubsystem* Subsystem = MakeTestSubsystem();
	TestEqual(TEXT("GetRevision is 0 before any store exists"), Subsystem->GetRevision(), (uint64)0);

	Subsystem->CreateStore({ FDataLensColumn::OfFloat(MakeTestTag(TEXT("Test.Attribute.Health"))) }, 4);
	TestEqual(TEXT("GetRevision starts at 0 on a fresh store"), Subsystem->GetRevision(), (uint64)0);

	const uint64 Bumped = Subsystem->BumpRevision();
	TestEqual(TEXT("BumpRevision returns the new revision"), Bumped, (uint64)1);
	TestEqual(TEXT("GetRevision reflects the bump"), Subsystem->GetRevision(), (uint64)1);

	Subsystem->SetRevision(42);
	TestEqual(TEXT("SetRevision is read back exactly"), Subsystem->GetRevision(), (uint64)42);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensSnapshotAppliesToFreshStoreTest,
	"Heathen.FoundationDataLens.SnapshotAppliesToFreshStore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensSnapshotAppliesToFreshStoreTest::RunTest(const FString& Parameters)
{
	const FGameplayTag HealthTag = MakeTestTag(TEXT("Test.Attribute.Health"));
	const FGameplayTag LevelTag = MakeTestTag(TEXT("Test.Attribute.Level"));
	const TArray<FDataLensColumn> Schema = { FDataLensColumn::OfFloat(HealthTag), FDataLensColumn::OfInt32(LevelTag) };

	// Producer: a populated store.
	UDataLensSubsystem* Producer = MakeTestSubsystem();
	Producer->CreateStore(Schema, 4);
	const uint64 ProducerRow = Producer->AllocRow();
	Producer->SetValid(ProducerRow, true);
	Producer->SetFloat(HealthTag, ProducerRow, 75.0f);
	Producer->SetInt32(LevelTag, ProducerRow, 5);

	const TArray<uint8> Payload = Producer->Snapshot();
	TestTrue(TEXT("Snapshot of a populated store is non-empty"), Payload.Num() > 0);

	// Consumer: identical schema, no data of its own -- this is the actual replication proof.
	UDataLensSubsystem* Consumer = MakeTestSubsystem();
	Consumer->CreateStore(Schema, 4);
	TestTrue(TEXT("ApplyPayload succeeds against a schema-matching store"), Consumer->ApplyPayload(Payload));

	const uint64 ConsumerRow = 0; // ApplyPayload lands the snapshot at row 0 in a store this fresh.
	float ConsumerHealth = 0.0f;
	int32 ConsumerLevel = 0;
	Consumer->GetFloat(HealthTag, ConsumerRow, ConsumerHealth);
	Consumer->GetInt32(LevelTag, ConsumerRow, ConsumerLevel);
	TestEqual(TEXT("Consumer's Health matches what the producer snapshotted"), ConsumerHealth, 75.0f);
	TestEqual(TEXT("Consumer's Level matches what the producer snapshotted"), ConsumerLevel, 5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensDeltaRoundTripTest,
	"Heathen.FoundationDataLens.DeltaRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensDeltaRoundTripTest::RunTest(const FString& Parameters)
{
	const FGameplayTag HealthTag = MakeTestTag(TEXT("Test.Attribute.Health"));
	const TArray<FDataLensColumn> Schema = { FDataLensColumn::OfFloat(HealthTag) };

	UDataLensSubsystem* Producer = MakeTestSubsystem();
	Producer->CreateStore(Schema, 4);
	const uint64 Row = Producer->AllocRow();
	Producer->SetValid(Row, true);
	Producer->SetFloat(HealthTag, Row, 100.0f);

	// Baseline is captured BEFORE the tick's bump, not after: Core stamps a dirtied column with
	// whatever GetRevision() reads at the moment of the write (DataStore::MarkColumnDirty sets
	// mColumnRevision[c] = mRevision), and CollectDelta's gate is a STRICT `stamp > sinceRevision`
	// (DataStore::CollectDeltaSince). So the bump has to happen between "baseline captured" and
	// "mutation happens", or the mutation's stamp lands exactly equal to the baseline and gets
	// excluded (stamp > baseline is false when they're equal) -- caught by this test failing that
	// way on the first attempt.
	const uint64 BaselineRevision = Producer->GetRevision();

	// Bring a consumer up to the baseline via a full snapshot first, matching how a real observer
	// would join, then start collecting deltas after that point.
	UDataLensSubsystem* Consumer = MakeTestSubsystem();
	Consumer->CreateStore(Schema, 4);
	Consumer->ApplyPayload(Producer->Snapshot());

	Producer->BumpRevision();

	// RunSystemFloat marks its target column dirty internally (Core's own Lens::RunSystem does
	// this), so the mutation below is visible to CollectDelta without an explicit MarkColumnDirty.
	Producer->RunSystemFloat(HealthTag, EDataLensSystemOp::Sub, 25.0f);

	const TArray<uint8> Delta = Producer->CollectDelta(BaselineRevision);
	TestTrue(TEXT("CollectDelta after a mutation is non-empty"), Delta.Num() > 0);
	TestTrue(TEXT("ApplyPayload accepts the delta"), Consumer->ApplyPayload(Delta));

	float ConsumerHealth = 0.0f;
	Consumer->GetFloat(HealthTag, Row, ConsumerHealth);
	TestEqual(TEXT("Consumer reflects the delta-applied mutation"), ConsumerHealth, 75.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensApplyPayloadRejectsMalformedDataTest,
	"Heathen.FoundationDataLens.ApplyPayloadRejectsMalformedData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensApplyPayloadRejectsMalformedDataTest::RunTest(const FString& Parameters)
{
	UDataLensSubsystem* Subsystem = MakeTestSubsystem();
	Subsystem->CreateStore({ FDataLensColumn::OfFloat(MakeTestTag(TEXT("Test.Attribute.Health"))) }, 4);

	TArray<uint8> Garbage;
	Garbage.Init(0xFF, 16);
	TestFalse(TEXT("ApplyPayload rejects garbage bytes, not crash"), Subsystem->ApplyPayload(Garbage));

	TestFalse(TEXT("ApplyPayload rejects an empty payload"), Subsystem->ApplyPayload(TArray<uint8>()));

	// No store at all: must also fail cleanly.
	UDataLensSubsystem* EmptySubsystem = MakeTestSubsystem();
	TArray<uint8> SomeBytes;
	SomeBytes.Init(0, 8);
	TestFalse(TEXT("ApplyPayload fails when no store exists"), EmptySubsystem->ApplyPayload(SomeBytes));

	return true;
}

// M5 coverage target: UDataLensBlueprintLibrary's int64<->uint64 boundary casting is exercised
// end-to-end through the same Producer/Consumer-style scenarios above, but calling exclusively
// through the Blueprint-facing static functions (never the core UDataLensSubsystem API directly)
// so a regression in the boundary conversion itself would actually be caught, not just a
// regression in the core it wraps.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensBlueprintLibraryRoundTripTest,
	"Heathen.FoundationDataLens.BlueprintLibraryRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensBlueprintLibraryRoundTripTest::RunTest(const FString& Parameters)
{
	UDataLensSubsystem* Subsystem = MakeTestSubsystem();
	const FGameplayTag HealthTag = MakeTestTag(TEXT("Test.Attribute.Health"));
	const FGameplayTag LevelTag = MakeTestTag(TEXT("Test.Attribute.Level"));

	TArray<FDataLensColumnBP> Columns;
	FDataLensColumnBP HealthColumn;
	HealthColumn.Tag = HealthTag;
	HealthColumn.Type = EDataLensColumnTypeBP::Float;
	Columns.Add(HealthColumn);
	FDataLensColumnBP LevelColumn;
	LevelColumn.Tag = LevelTag;
	LevelColumn.Type = EDataLensColumnTypeBP::Int32;
	Columns.Add(LevelColumn);

	TestTrue(TEXT("BP CreateStore succeeds"), UDataLensBlueprintLibrary::CreateStore(Subsystem, Columns, 4));
	TestTrue(TEXT("BP HasStore reflects the create"), UDataLensBlueprintLibrary::HasStore(Subsystem));
	TestTrue(TEXT("BP HasColumn is true for a declared column"), UDataLensBlueprintLibrary::HasColumn(Subsystem, HealthTag));

	const int64 Row = UDataLensBlueprintLibrary::AllocRow(Subsystem);
	TestTrue(TEXT("BP AllocRow returns a real row, not the -1 sentinel"), Row >= 0);
	UDataLensBlueprintLibrary::SetValid(Subsystem, Row, true);
	TestTrue(TEXT("BP IsValid reflects SetValid"), UDataLensBlueprintLibrary::IsValid(Subsystem, Row));

	TestTrue(TEXT("BP SetFloat succeeds"), UDataLensBlueprintLibrary::SetFloat(Subsystem, HealthTag, Row, 100.0f));
	float HealthOut = 0.0f;
	TestTrue(TEXT("BP GetFloat succeeds"), UDataLensBlueprintLibrary::GetFloat(Subsystem, HealthTag, Row, HealthOut));
	TestEqual(TEXT("BP GetFloat round-trips the value set via BP SetFloat"), HealthOut, 100.0f);

	TestTrue(TEXT("BP SetInt32 succeeds"), UDataLensBlueprintLibrary::SetInt32(Subsystem, LevelTag, Row, 7));
	int32 LevelOut = 0;
	TestTrue(TEXT("BP GetInt32 succeeds"), UDataLensBlueprintLibrary::GetInt32(Subsystem, LevelTag, Row, LevelOut));
	TestEqual(TEXT("BP GetInt32 round-trips the value set via BP SetInt32"), LevelOut, 7);

	const int64 AffectedRows = UDataLensBlueprintLibrary::RunSystemFloat(Subsystem, HealthTag, EDataLensSystemOpBP::Sub, 25.0f);
	TestEqual(TEXT("BP RunSystemFloat affects exactly the one live row"), AffectedRows, (int64)1);
	UDataLensBlueprintLibrary::GetFloat(Subsystem, HealthTag, Row, HealthOut);
	TestEqual(TEXT("BP RunSystemFloat's EDataLensSystemOpBP::Sub maps to the correct core op"), HealthOut, 75.0f);

	UDataLensBlueprintLibrary::FreeRow(Subsystem, Row);
	TestFalse(TEXT("BP IsValid is false after BP FreeRow"), UDataLensBlueprintLibrary::IsValid(Subsystem, Row));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensBlueprintLibraryAllocRowSentinelTest,
	"Heathen.FoundationDataLens.BlueprintLibraryAllocRowSentinel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensBlueprintLibraryAllocRowSentinelTest::RunTest(const FString& Parameters)
{
	// No store at all: the core's UINT64_MAX "no room" sentinel must round-trip to exactly -1,
	// not some other bit pattern, through the BP int64 boundary.
	UDataLensSubsystem* Subsystem = MakeTestSubsystem();
	TestEqual(TEXT("BP AllocRow returns -1 when no store exists"), UDataLensBlueprintLibrary::AllocRow(Subsystem), (int64)-1);

	const FGameplayTag HealthTag = MakeTestTag(TEXT("Test.Attribute.Health"));
	TArray<FDataLensColumnBP> Columns;
	FDataLensColumnBP HealthColumn;
	HealthColumn.Tag = HealthTag;
	Columns.Add(HealthColumn);
	UDataLensBlueprintLibrary::CreateStore(Subsystem, Columns, /*PreallocRows*/ 1);

	TestTrue(TEXT("First BP AllocRow on a 1-row store succeeds"), UDataLensBlueprintLibrary::AllocRow(Subsystem) >= 0);
	TestEqual(TEXT("Second BP AllocRow on a full 1-row store returns -1"), UDataLensBlueprintLibrary::AllocRow(Subsystem), (int64)-1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataLensBlueprintLibraryReplicationRoundTripTest,
	"Heathen.FoundationDataLens.BlueprintLibraryReplicationRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDataLensBlueprintLibraryReplicationRoundTripTest::RunTest(const FString& Parameters)
{
	const FGameplayTag HealthTag = MakeTestTag(TEXT("Test.Attribute.Health"));
	TArray<FDataLensColumnBP> Columns;
	FDataLensColumnBP HealthColumn;
	HealthColumn.Tag = HealthTag;
	Columns.Add(HealthColumn);

	UDataLensSubsystem* Producer = MakeTestSubsystem();
	UDataLensBlueprintLibrary::CreateStore(Producer, Columns, 4);
	const int64 Row = UDataLensBlueprintLibrary::AllocRow(Producer);
	UDataLensBlueprintLibrary::SetValid(Producer, Row, true);
	UDataLensBlueprintLibrary::SetFloat(Producer, HealthTag, Row, 50.0f);

	TestEqual(TEXT("BP GetRevision starts at 0"), UDataLensBlueprintLibrary::GetRevision(Producer), (int64)0);
	const int64 Bumped = UDataLensBlueprintLibrary::BumpRevision(Producer);
	TestEqual(TEXT("BP BumpRevision returns the new revision"), Bumped, (int64)1);
	UDataLensBlueprintLibrary::SetRevision(Producer, 5);
	TestEqual(TEXT("BP SetRevision is read back exactly"), UDataLensBlueprintLibrary::GetRevision(Producer), (int64)5);

	const TArray<uint8> Payload = UDataLensBlueprintLibrary::Snapshot(Producer);
	TestTrue(TEXT("BP Snapshot of a populated store is non-empty"), Payload.Num() > 0);

	UDataLensSubsystem* Consumer = MakeTestSubsystem();
	UDataLensBlueprintLibrary::CreateStore(Consumer, Columns, 4);
	TestTrue(TEXT("BP ApplyPayload succeeds against a schema-matching store"), UDataLensBlueprintLibrary::ApplyPayload(Consumer, Payload));

	float ConsumerHealth = 0.0f;
	UDataLensBlueprintLibrary::GetFloat(Consumer, HealthTag, 0, ConsumerHealth);
	TestEqual(TEXT("Consumer's Health matches what the BP-driven producer snapshotted"), ConsumerHealth, 50.0f);

	TestFalse(TEXT("BP ApplyPayload rejects garbage bytes, not crash"), UDataLensBlueprintLibrary::ApplyPayload(Consumer, TArray<uint8>({ 0xFF, 0xFF, 0xFF, 0xFF })));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
