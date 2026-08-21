#include "Misc/AutomationTest.h"
#include "Parcel/GrammarBlockExtraction.h"

#if WITH_DEV_AUTOMATION_TESTS

// Synthetic road-graph test, deliberately independent of FlexNetwork (FGrammarBlockExtraction takes
// plain FGrammarRoadPolyline input precisely so this is possible without a UWorld). Locks in two
// separate things ExtractBlocks depends on getting right, both documented in that function's own
// comments: the signed-area sign convention (a single closed loop should trace 1 bounded block, not
// 0 -- if the sign were backwards it'd be the opposite), and, at any node with 3+ edges, the face-
// tracing traversal DIRECTION (next clockwise from the twin, not counterclockwise -- a plain single
// loop's degree-2 nodes can't actually distinguish the two directions, which is exactly how that
// direction bug shipped unnoticed the first time; the crossroads case below is what catches it).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrammarBlockExtractionTest, "ProceduralBuildingGrammar.Parcel.BlockExtraction", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGrammarBlockExtractionTest::RunTest(const FString& Parameters)
{
	auto MakeRoad = [](const FVector2D& A, const FVector2D& B, double Inset) -> FGrammarRoadPolyline
	{
		FGrammarRoadPolyline Road;
		Road.Points = {A, B};
		Road.InsetDistance = Inset;
		return Road;
	};

	// A single square loop of 4 roads (100m x 100m centerlines), each inset 5m.
	constexpr double Inset = 5.0;
	TArray<FGrammarRoadPolyline> Roads;
	Roads.Add(MakeRoad(FVector2D(0, 0), FVector2D(100, 0), Inset));
	Roads.Add(MakeRoad(FVector2D(100, 0), FVector2D(100, 100), Inset));
	Roads.Add(MakeRoad(FVector2D(100, 100), FVector2D(0, 100), Inset));
	Roads.Add(MakeRoad(FVector2D(0, 100), FVector2D(0, 0), Inset));

	const TArray<FGrammarBlock> Blocks = FGrammarBlockExtraction::ExtractBlocks(Roads);

	TestEqual(TEXT("Exactly one bounded block is traced from a single closed road loop"), Blocks.Num(), 1);
	if (Blocks.Num() == 1)
	{
		// Expected inset square is roughly (100 - 2*5) x (100 - 2*5) = 8100 m^2 -- generous tolerance
		// for the per-road offset's nearest-point corner-join simplification (see
		// GrammarBlockExtraction.cpp's OffsetPolyline comment for why this isn't a precise miter).
		TestTrue(TEXT("Traced block area is close to the expected inset square"), FMath::IsNearlyEqual(Blocks[0].AreaM2, 8100.0, 400.0));
		TestTrue(TEXT("Traced block polygon has at least 4 corners"), Blocks[0].Polygon.Num() >= 4);
		AddInfo(FString::Printf(TEXT("Traced block area: %.1f m^2 (expected ~8100)"), Blocks[0].AreaM2));
	}

	// A completely open (non-closed) chain of roads has no bounded face at all -- must return zero
	// blocks, not spuriously trace the unbounded/outer face as if it were one.
	TArray<FGrammarRoadPolyline> OpenChain;
	OpenChain.Add(MakeRoad(FVector2D(0, 0), FVector2D(100, 0), Inset));
	OpenChain.Add(MakeRoad(FVector2D(100, 0), FVector2D(100, 100), Inset));
	const TArray<FGrammarBlock> NoBlocks = FGrammarBlockExtraction::ExtractBlocks(OpenChain);
	TestEqual(TEXT("An open (non-closed) road chain traces zero bounded blocks"), NoBlocks.Num(), 0);

	// The same square perimeter PLUS both diagonals, crossing each other at the center with no
	// shared node there -- exercises two things the single-loop case above can't: (1) the
	// intersection-splitting preprocessing (SplitRoadsAtIntersections) that turns the crossing into
	// a real degree-4 node, and (2) the face-tracing direction at a degree-4 node itself. This is the
	// case that caught ExtractBlocks's traversal rule originally being backwards ("next CCW from
	// twin" instead of "next CW from twin") -- a single loop's degree-2 nodes can't distinguish the
	// two directions (only one *other* edge exists either way), so that bug shipped silently until
	// exercised against a real intersection. Expect exactly 4 small triangular blocks, not 1 bogus
	// block covering the whole square.
	TArray<FGrammarRoadPolyline> CrossroadsSquare;
	CrossroadsSquare.Add(MakeRoad(FVector2D(0, 0), FVector2D(100, 0), Inset));
	CrossroadsSquare.Add(MakeRoad(FVector2D(100, 0), FVector2D(100, 100), Inset));
	CrossroadsSquare.Add(MakeRoad(FVector2D(100, 100), FVector2D(0, 100), Inset));
	CrossroadsSquare.Add(MakeRoad(FVector2D(0, 100), FVector2D(0, 0), Inset));
	CrossroadsSquare.Add(MakeRoad(FVector2D(0, 0), FVector2D(100, 100), Inset));
	CrossroadsSquare.Add(MakeRoad(FVector2D(100, 0), FVector2D(0, 100), Inset));
	const TArray<FGrammarBlock> CrossroadsBlocks = FGrammarBlockExtraction::ExtractBlocks(CrossroadsSquare);
	TestEqual(TEXT("A square with both diagonals crossing at the center traces exactly 4 triangular blocks"), CrossroadsBlocks.Num(), 4);
	for (const FGrammarBlock& Block : CrossroadsBlocks)
	{
		// Each triangle is roughly 1/4 of the inset square (~8100 m^2 total, see the case above) --
		// generous tolerance for the same corner-join simplification as that case, plus the extra
		// asymmetry the center-crossing offset introduces.
		TestTrue(TEXT("Each crossroads triangle's area is plausible (not near-zero, not near the whole square)"), Block.AreaM2 > 500.0 && Block.AreaM2 < 6000.0);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
