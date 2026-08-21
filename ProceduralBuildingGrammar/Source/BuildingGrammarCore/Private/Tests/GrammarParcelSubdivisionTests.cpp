#include "Misc/AutomationTest.h"
#include "Parcel/GrammarParcelSubdivision.h"
#include "Geometry/GrammarGeometry2D.h"

#if WITH_DEV_AUTOMATION_TESTS

// Port of the user-supplied test_parcel_subdivision.cpp regression test -- same three block shapes,
// chosen to stress the exact failure modes the reference implementation's own hardening pass
// addresses (a long rectangle exposes centroid-fan wedges; an L-shape's area centroid falls outside
// the polygon; a "comb" block with deep notches pressure-tests the fallback-to-OBB contract when the
// coverage sanity check fails), and the same two assertions per (shape, method) pair: every produced
// parcel is a validated simple polygon, and the total parcel area covers the block within tolerance
// (no gaps, no overlap).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrammarParcelSubdivisionTest, "ProceduralBuildingGrammar.Parcel.Subdivision", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	bool AllParcelsSimple(const FGrammarParcelSubdivisionResult& Result)
	{
		for (const FGrammarParcel& Parcel : Result.Parcels)
		{
			if (!FGrammarParcelSubdivision::IsSimplePolygon(Parcel.Polygon))
			{
				return false;
			}
		}
		return true;
	}

	double TotalParcelArea(const FGrammarParcelSubdivisionResult& Result)
	{
		double Sum = 0.0;
		for (const FGrammarParcel& Parcel : Result.Parcels)
		{
			Sum += FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Parcel.Polygon));
		}
		return Sum;
	}
}

bool FGrammarParcelSubdivisionTest::RunTest(const FString& Parameters)
{
	FGrammarParcelConfig Config;
	Config.MinArea = 1250.0;
	Config.MaxArea = 4400.0;
	Config.MinWidth = 28.0;
	Config.MaxWidth = 58.0;
	Config.PerimeterDepth = 88.0;
	Config.Irregularity = 0.24;
	Config.StreetAccessPreference = 0.72;
	Config.OffsetRatio = 0.35;
	Config.Seed = 4127;

	struct FCase
	{
		const TCHAR* Name;
		TArray<FVector2D> Block;
	};

	TArray<FCase> Cases;
	// A long, narrow rectangular block -- the shape that most exposed the old centroid-fan wedge problem.
	Cases.Add(FCase{TEXT("long rectangle"), {FVector2D(0, 0), FVector2D(480, 0), FVector2D(480, 120), FVector2D(0, 120)}});
	// An L-shaped concave block whose area centroid falls in the notch.
	Cases.Add(FCase{TEXT("L-shaped block"), {FVector2D(0, 0), FVector2D(300, 0), FVector2D(300, 100), FVector2D(120, 100), FVector2D(120, 260), FVector2D(0, 260)}});
	// A deliberately pathological "comb" block with deep, closely-spaced concave notches.
	Cases.Add(FCase{TEXT("comb block"), {
		FVector2D(0, 0), FVector2D(40, 0), FVector2D(40, 150), FVector2D(80, 150), FVector2D(80, 0), FVector2D(120, 0),
		FVector2D(120, 150), FVector2D(160, 150), FVector2D(160, 0), FVector2D(200, 0), FVector2D(200, 220), FVector2D(0, 220)
	}});

	const EGrammarParcelSubdivisionMethod Methods[] = {
		EGrammarParcelSubdivisionMethod::Obb,
		EGrammarParcelSubdivisionMethod::SkeletonNoOffset,
		EGrammarParcelSubdivisionMethod::SkeletonWithOffset,
		EGrammarParcelSubdivisionMethod::Hybrid
	};
	const TCHAR* MethodNames[] = {TEXT("obb"), TEXT("skeleton-no-offset"), TEXT("skeleton-with-offset"), TEXT("hybrid")};

	for (const FCase& Case : Cases)
	{
		const double BlockArea = FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Case.Block));

		for (int32 MethodIndex = 0; MethodIndex < 4; ++MethodIndex)
		{
			// Matches the reference test's own convention: one fixed base Config, a distinct BlockId
			// (0..3) per method so each gets an independent seed derivation (Config.Seed + BlockId *
			// 9973 internally) without needing to hand-vary Config.Seed itself.
			const FGrammarParcelSubdivisionResult Result = FGrammarParcelSubdivision::Subdivide(Case.Block, Config, Methods[MethodIndex], MethodIndex);

			const FString Label = FString::Printf(TEXT("%s / %s"), Case.Name, MethodNames[MethodIndex]);
			TestTrue(FString::Printf(TEXT("%s: produced at least one parcel"), *Label), Result.Parcels.Num() > 0);
			TestTrue(FString::Printf(TEXT("%s: every parcel is a valid simple polygon"), *Label), AllParcelsSimple(Result));

			const double Coverage = TotalParcelArea(Result) / FMath::Max(1.0, BlockArea);
			AddInfo(FString::Printf(TEXT("%s: %d parcel(s), coverage ratio %.4f"), *Label, Result.Parcels.Num(), Coverage));
			// Same 15% tolerance FGrammarParcelSubdivision's own internal RingCoverageIsPlausible check
			// uses -- a silent regression toward gaps or overlap shows up here even if nothing else trips.
			TestTrue(FString::Printf(TEXT("%s: coverage ratio is plausible (0.85..1.15)"), *Label), Coverage > 0.85 && Coverage < 1.15);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
