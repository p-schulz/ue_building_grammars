#include "Parcel/GrammarBlockExtraction.h"
#include "Parcel/GrammarParcelSubdivision.h"
#include "Geometry/GrammarGeometry2D.h"
#include "Algo/Reverse.h"
#include "Math/IntPoint.h"
#include "Math/Box2D.h"

namespace
{
	FVector2D LeftPerp(const FVector2D& Dir)
	{
		return FVector2D(-Dir.Y, Dir.X);
	}

	// Offsets every point of a tessellated road polyline sideways by Distance (positive = left of
	// the Points[0]->Points.Last() walking direction). Interior points use the averaged ("miter")
	// normal of their two adjacent segments; endpoints use their single adjacent segment's normal.
	// Not a true constant-distance miter offset (no 1/cos(halfAngle) length correction) -- an
	// accepted v1 simplification, safe here because FlexNetwork's ArcLengthTable tessellation
	// already keeps consecutive points close together and near-collinear except at real road
	// junctions, which this function never sees (it offsets one road's own interior shape; junction
	// corners are joined afterward, in ExtractBlocks, by point-merge tolerance).
	TArray<FVector2D> OffsetPolyline(const TArray<FVector2D>& Points, double Distance)
	{
		const int32 N = Points.Num();
		if (N < 2 || FMath::IsNearlyZero(Distance))
		{
			return Points;
		}

		TArray<FVector2D> Out;
		Out.Reserve(N);
		for (int32 i = 0; i < N; ++i)
		{
			FVector2D Normal;
			if (i == 0)
			{
				Normal = LeftPerp(FGrammarGeometry2D::Normalize2D(Points[1] - Points[0]));
			}
			else if (i == N - 1)
			{
				Normal = LeftPerp(FGrammarGeometry2D::Normalize2D(Points[N - 1] - Points[N - 2]));
			}
			else
			{
				const FVector2D PrevDir = FGrammarGeometry2D::Normalize2D(Points[i] - Points[i - 1]);
				const FVector2D NextDir = FGrammarGeometry2D::Normalize2D(Points[i + 1] - Points[i]);
				const FVector2D AvgDir = PrevDir + NextDir;
				Normal = AvgDir.IsNearlyZero() ? LeftPerp(NextDir) : LeftPerp(FGrammarGeometry2D::Normalize2D(AvgDir));
			}
			Out.Add(Points[i] + Normal * Distance);
		}
		return Out;
	}

	// Meters -- only needs to absorb floating-point drift between segments that share one real
	// FlexNetwork node (see SpatialNodeKey's comment below), not resolve genuine ambiguity between
	// nearby but distinct intersections. Shared at file scope (not just ExtractBlocks's old local
	// copy) because SplitRoadsAtIntersections needs the exact same tolerance -- see its own comment
	// on why.
	constexpr double NodeMergeTolerance = 0.05;

	// Groups road endpoints into graph nodes purely by spatial proximity (no explicit node-ID input
	// exists on FGrammarRoadPolyline by design -- see its header comment). Safe for FlexNetwork-
	// sourced input specifically because every segment touching one real FFlexRoadNode already
	// tessellates from that exact same shared Position, so genuinely-the-same node's raw (non-inset)
	// endpoints are bit-identical or near enough; this tolerance only needs to absorb floating-point
	// drift, not genuine ambiguity.
	FIntPoint SpatialNodeKey(const FVector2D& P, double CellSize)
	{
		return FIntPoint(FMath::RoundToInt(P.X / CellSize), FMath::RoundToInt(P.Y / CellSize));
	}

	// One directed traversal of one road's OWN INSET boundary (not the centerline) -- Points here is
	// already the offset polyline for whichever side this direction bounds. FromNode/ToNode are
	// canonical (pre-inset) node keys, so topology (which edges meet at which real node) stays
	// correct even though each edge's own geometry is independently inset per-road.
	struct FBlockHalfEdge
	{
		FIntPoint FromNode;
		FIntPoint ToNode;
		TArray<FVector2D> Points;
		double Angle = 0.0;
		int32 TwinIndex = -1;
		bool bVisited = false;
		FString RoadId;
	};

	// A point where two DIFFERENT roads' centerlines geometrically cross without either road actually
	// ending there -- the common FlexNetwork case this preprocessing pass exists for (two roads drawn
	// through the same point in the viewport with no shared FFlexRoadNode). SegmentIndex/LocalT locate
	// the cut along the road's OWN polyline (Points[SegmentIndex] -> Points[SegmentIndex+1], parameter
	// LocalT), so cuts from many other roads can all be resolved against one road independently.
	struct FRoadCut
	{
		int32 SegmentIndex = 0;
		double LocalT = 0.0;
		FVector2D Position;
	};

	// Strictly-interior segment intersection with its parameters, not just a bool -- SegmentsIntersect
	// (GrammarParcelSubdivision.h) only answers yes/no, which isn't enough here: this preprocessing
	// pass needs the actual crossing point (to insert as a new polyline vertex) and how far along each
	// segment it falls (to know where to cut). Excludes intersections within Epsilon of either
	// segment's own endpoints -- those are (at most) two roads already meeting at a shared node, which
	// the existing SpatialNodeKey merge already handles; re-cutting exactly on top of a real node would
	// just create a redundant duplicate.
	bool SegmentIntersectionParams(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, double& OutT, double& OutU)
	{
		const FVector2D R = B - A;
		const FVector2D S = D - C;
		const double RxS = R.X * S.Y - R.Y * S.X;
		if (FMath::IsNearlyZero(RxS))
		{
			return false; // Parallel or collinear -- not a crossing this pass needs to handle.
		}
		const FVector2D CmA = C - A;
		const double T = (CmA.X * S.Y - CmA.Y * S.X) / RxS;
		const double U = (CmA.X * R.Y - CmA.Y * R.X) / RxS;
		constexpr double Epsilon = 1e-4;
		if (T <= Epsilon || T >= 1.0 - Epsilon || U <= Epsilon || U >= 1.0 - Epsilon)
		{
			return false;
		}
		OutT = T;
		OutU = U;
		return true;
	}

	// Diagnostic-only (never affects the reject decision, which is IsSimplePolygon's own call at the
	// site below) -- finds the first pair of non-adjacent edges that actually cross and logs a
	// compact dump of the polygon's full point list. The per-road offset + single-point corner
	// choice (see OffsetPolyline's and ExtractBlocks's own comments) is known-imprecise, but exactly
	// which real junction geometry trips it needs to be seen on real data, not guessed from another
	// synthetic case.
	void LogNonSimpleBlockDiagnostics(const TArray<FVector2D>& Polygon)
	{
		const int32 N = Polygon.Num();
		int32 CrossI = INDEX_NONE;
		int32 CrossJ = INDEX_NONE;
		for (int32 I = 0; I < N && CrossI == INDEX_NONE; ++I)
		{
			for (int32 J = I + 1; J < N; ++J)
			{
				if (J == I + 1 || (I == 0 && J == N - 1))
				{
					continue; // Adjacent edges share a vertex -- not a crossing.
				}
				if (FGrammarParcelSubdivision::SegmentsIntersect(Polygon[I], Polygon[(I + 1) % N], Polygon[J], Polygon[(J + 1) % N]))
				{
					CrossI = I;
					CrossJ = J;
					break;
				}
			}
		}

		FString PointsList;
		for (const FVector2D& P : Polygon)
		{
			PointsList += FString::Printf(TEXT("(%.2f,%.2f) "), P.X, P.Y);
		}

		if (CrossI != INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("FGrammarBlockExtraction::ExtractBlocks: rejected non-simple block, %d point(s), edge [%d->%d] crosses edge [%d->%d]. Points (m): %s"),
				N, CrossI, (CrossI + 1) % N, CrossJ, (CrossJ + 1) % N, *PointsList);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("FGrammarBlockExtraction::ExtractBlocks: rejected non-simple block, %d point(s), but no crossing edge pair found by the same pairwise check (degenerate/duplicate-point case?). Points (m): %s"),
				N, *PointsList);
		}
	}

	// The corner where PrevEdge's tail direction and NextEdge's head direction (extended as infinite
	// lines) meet. Replaces the blunt "just use NextEdge's own first offset point" corner the face
	// tracer used to use unconditionally: since each road is offset independently of its neighbors
	// (OffsetPolyline's own comment), the arriving edge's first point generally does NOT lie on the
	// departing edge's own offset line, so blindly using it can overshoot past the other edge entirely
	// -- confirmed against real FlexNetwork data producing exactly this: a road offset ending at
	// (30.20,-6.69) followed immediately by the next road's own offset starting at (36.70,-7.51), a
	// 6.5m sideways jump versus an 0.82m step in the perpendicular direction, which then folds back
	// across the first road's own edge as the boundary continues -- a self-intersection with no
	// actual road geometry justifying it. The true corner here is the two offset lines' own
	// intersection, (30.20,-7.51). Returns false (caller keeps the old NextEdge.Points[0] fallback) if
	// the two lines are too close to parallel to solve reliably, or the solved point is implausibly
	// far from both edges' own near-corner points (an extremely acute junction angle can spike a true
	// miter arbitrarily far -- safer to fall back than to spike).
	bool ComputeCornerPoint(const FBlockHalfEdge& PrevEdge, const FBlockHalfEdge& NextEdge, FVector2D& OutCorner)
	{
		if (PrevEdge.Points.Num() < 2 || NextEdge.Points.Num() < 2)
		{
			return false;
		}
		const FVector2D& A = PrevEdge.Points[PrevEdge.Points.Num() - 2];
		const FVector2D& B = PrevEdge.Points.Last();
		const FVector2D& C = NextEdge.Points[0];
		const FVector2D& D = NextEdge.Points[1];

		const FVector2D R = B - A;
		const FVector2D S = D - C;
		const double RxS = R.X * S.Y - R.Y * S.X;
		if (FMath::IsNearlyZero(RxS))
		{
			return false; // Near-parallel -- barely any corner to fix anyway.
		}
		const FVector2D CmA = C - A;
		const double T = (CmA.X * S.Y - CmA.Y * S.X) / RxS;
		const FVector2D Corner = A + R * T;

		constexpr double MaxCornerJump = 50.0; // Meters -- generously larger than any real road width/inset.
		if (FVector2D::Distance(Corner, B) > MaxCornerJump || FVector2D::Distance(Corner, C) > MaxCornerJump)
		{
			return false;
		}

		OutCorner = Corner;
		return true;
	}

	FBox2D RoadBounds(const TArray<FVector2D>& Points)
	{
		FBox2D Bounds(ForceInit);
		for (const FVector2D& P : Points)
		{
			Bounds += P;
		}
		return Bounds;
	}

	// True if road I and road J already meet at a shared node (either endpoint of one within
	// NodeMergeTolerance of either endpoint of the other) -- the same tolerance/merge rule
	// ExtractBlocks itself uses for real nodes. Two roads that already connect don't need splitting
	// against each other at all; more importantly, EXCLUDING them from the crossing search below is
	// what keeps this pass safe against real (densely tessellated, curved) FlexNetwork data, not just
	// the straight 2-point synthetic roads the automation tests use: near a real shared node, two
	// roads' independent tessellations can pass close together at a shallow angle before actually
	// converging, which can register as a technically-valid interior crossing a hair away from the
	// real node -- SegmentIntersectionParams's own endpoint-fraction epsilon doesn't reliably catch
	// this because a densely-tessellated segment near a node can be short enough that even a tiny
	// fraction of it is still larger, in real distance, than the noise. Skipping any pair that
	// already shares a node removes the whole class of noise at the source instead of trying to
	// filter it after the fact.
	bool RoadsAlreadyShareNode(const FGrammarRoadPolyline& A, const FGrammarRoadPolyline& B)
	{
		if (A.Points.Num() < 2 || B.Points.Num() < 2)
		{
			return false; // Degenerate roads have no node to share; ExtractBlocks's own Num() < 2 check discards them later.
		}
		const FIntPoint AStart = SpatialNodeKey(A.Points[0], NodeMergeTolerance);
		const FIntPoint AEnd = SpatialNodeKey(A.Points.Last(), NodeMergeTolerance);
		const FIntPoint BStart = SpatialNodeKey(B.Points[0], NodeMergeTolerance);
		const FIntPoint BEnd = SpatialNodeKey(B.Points.Last(), NodeMergeTolerance);
		return AStart == BStart || AStart == BEnd || AEnd == BStart || AEnd == BEnd;
	}

	// Splits every road at every point where it geometrically crosses a DIFFERENT, NOT-ALREADY-
	// CONNECTED road's centerline -- see FRoadCut's and RoadsAlreadyShareNode's comments for why the
	// "not already connected" restriction matters. Runs before any offsetting/half-edge building, so
	// the rest of ExtractBlocks never has to know the difference between "roads that were drawn
	// meeting at a real node" and "roads that happen to cross" -- both look like ordinary shared
	// nodes to it afterward. O(NumRoads^2) road pairs (each with a cheap AABB reject first, then the
	// already-connected reject) times O(PointsPerRoad^2) segment pairs for the ones that survive --
	// fine for a single level's road network (tens to low hundreds of roads); would need a spatial
	// broad-phase if this ever ran on a road graph large enough for that product to matter.
	TArray<FGrammarRoadPolyline> SplitRoadsAtIntersections(const TArray<FGrammarRoadPolyline>& Roads, int32& OutCutCount)
	{
		OutCutCount = 0;
		const int32 NumRoads = Roads.Num();

		TArray<FBox2D> Bounds;
		Bounds.Reserve(NumRoads);
		for (const FGrammarRoadPolyline& Road : Roads)
		{
			Bounds.Add(RoadBounds(Road.Points));
		}

		TArray<TArray<FRoadCut>> CutsPerRoad;
		CutsPerRoad.SetNum(NumRoads);

		for (int32 I = 0; I < NumRoads; ++I)
		{
			const TArray<FVector2D>& PtsI = Roads[I].Points;
			for (int32 J = I + 1; J < NumRoads; ++J)
			{
				if (!Bounds[I].Intersect(Bounds[J]))
				{
					continue;
				}
				if (RoadsAlreadyShareNode(Roads[I], Roads[J]))
				{
					continue;
				}
				const TArray<FVector2D>& PtsJ = Roads[J].Points;
				for (int32 Si = 0; Si + 1 < PtsI.Num(); ++Si)
				{
					for (int32 Sj = 0; Sj + 1 < PtsJ.Num(); ++Sj)
					{
						double T = 0.0, U = 0.0;
						if (!SegmentIntersectionParams(PtsI[Si], PtsI[Si + 1], PtsJ[Sj], PtsJ[Sj + 1], T, U))
						{
							continue;
						}
						const FVector2D Point = PtsI[Si] + (PtsI[Si + 1] - PtsI[Si]) * T;
						CutsPerRoad[I].Add(FRoadCut{Si, T, Point});
						CutsPerRoad[J].Add(FRoadCut{Sj, U, Point});
						++OutCutCount;
					}
				}
			}
		}

		TArray<FGrammarRoadPolyline> Result;
		Result.Reserve(NumRoads);
		for (int32 I = 0; I < NumRoads; ++I)
		{
			TArray<FRoadCut>& Cuts = CutsPerRoad[I];
			if (Cuts.IsEmpty())
			{
				Result.Add(Roads[I]);
				continue;
			}

			Cuts.Sort([](const FRoadCut& A, const FRoadCut& B)
			{
				return A.SegmentIndex != B.SegmentIndex ? A.SegmentIndex < B.SegmentIndex : A.LocalT < B.LocalT;
			});

			const TArray<FVector2D>& SrcPoints = Roads[I].Points;
			TArray<FVector2D> Current;
			Current.Add(SrcPoints[0]);
			int32 CutIdx = 0;
			for (int32 Seg = 0; Seg + 1 < SrcPoints.Num(); ++Seg)
			{
				while (CutIdx < Cuts.Num() && Cuts[CutIdx].SegmentIndex == Seg)
				{
					Current.Add(Cuts[CutIdx].Position);
					FGrammarRoadPolyline Sub;
					Sub.Points = Current;
					Sub.InsetDistance = Roads[I].InsetDistance;
					Sub.RoadId = Roads[I].RoadId;
					Result.Add(MoveTemp(Sub));

					Current.Reset();
					Current.Add(Cuts[CutIdx].Position);
					++CutIdx;
				}
				Current.Add(SrcPoints[Seg + 1]);
			}
			FGrammarRoadPolyline Tail;
			Tail.Points = MoveTemp(Current);
			Tail.InsetDistance = Roads[I].InsetDistance;
			Tail.RoadId = Roads[I].RoadId;
			Result.Add(MoveTemp(Tail));
		}
		return Result;
	}
}

TArray<FGrammarBlock> FGrammarBlockExtraction::ExtractBlocks(const TArray<FGrammarRoadPolyline>& Roads)
{
	TArray<FGrammarBlock> Result;

	// 0. Split roads at points where they geometrically cross a different road without already
	// sharing a node there -- the common FlexNetwork case where two roads were drawn through the same
	// point in the viewport with no shared FFlexRoadNode. See SplitRoadsAtIntersections's own comment;
	// everything below this point works on SplitRoads and no longer needs to know the difference.
	int32 IntersectionCutCount = 0;
	const TArray<FGrammarRoadPolyline> SplitRoads = SplitRoadsAtIntersections(Roads, IntersectionCutCount);

	// 1. Build two half-edges per road, one per side, from that side's own inset polyline.
	TArray<FBlockHalfEdge> HalfEdges;
	HalfEdges.Reserve(SplitRoads.Num() * 2);
	TMap<FIntPoint, TArray<int32>> NodeOutgoing;

	for (const FGrammarRoadPolyline& Road : SplitRoads)
	{
		if (Road.Points.Num() < 2)
		{
			continue;
		}
		const FVector2D& Start = Road.Points[0];
		const FVector2D& End = Road.Points.Last();
		const FIntPoint FromKey = SpatialNodeKey(Start, NodeMergeTolerance);
		const FIntPoint ToKey = SpatialNodeKey(End, NodeMergeTolerance);
		if (FromKey == ToKey)
		{
			continue; // A self-closed loop has no distinct second node to bound a face against.
		}

		const TArray<FVector2D> LeftOffset = OffsetPolyline(Road.Points, Road.InsetDistance);
		TArray<FVector2D> RightOffset = OffsetPolyline(Road.Points, -Road.InsetDistance);
		Algo::Reverse(RightOffset);

		const int32 FwdIndex = HalfEdges.Num();
		FBlockHalfEdge& Fwd = HalfEdges.AddDefaulted_GetRef();
		Fwd.FromNode = FromKey;
		Fwd.ToNode = ToKey;
		Fwd.Points = LeftOffset;
		Fwd.RoadId = Road.RoadId;

		const int32 RevIndex = HalfEdges.Num();
		FBlockHalfEdge& Rev = HalfEdges.AddDefaulted_GetRef();
		Rev.FromNode = ToKey;
		Rev.ToNode = FromKey;
		Rev.Points = MoveTemp(RightOffset);
		Rev.RoadId = Road.RoadId;

		HalfEdges[FwdIndex].TwinIndex = RevIndex;
		HalfEdges[RevIndex].TwinIndex = FwdIndex;

		NodeOutgoing.FindOrAdd(FromKey).Add(FwdIndex);
		NodeOutgoing.FindOrAdd(ToKey).Add(RevIndex);
	}
	if (HalfEdges.IsEmpty())
	{
		// Roads.Num() > 0 here means every road was rejected by the FromKey == ToKey self-loop check
		// above (e.g. all input polylines degenerate to a single point at NodeMergeTolerance) --
		// distinguishes "no roads reached this function" from "roads reached it but none formed usable
		// edges" without needing a debugger.
		UE_LOG(LogTemp, Log, TEXT("FGrammarBlockExtraction::ExtractBlocks: %d input road(s) (%d after intersection-splitting, %d intersection(s) found), 0 usable half-edge(s) built -- returning zero blocks before face tracing even starts."),
			Roads.Num(), SplitRoads.Num(), IntersectionCutCount);
		return Result;
	}

	// 2. Sort each node's outgoing half-edges by angle -- needed to find "the next edge clockwise
	// from the twin" during face tracing. Each half-edge's own immediate tangent direction is a
	// well-defined, sufficient sort key here: unlike BuildingBlockDetection.cpp's compound-junction
	// case, a FlexNetwork node is always a single point, so there's no "which vantage point"
	// ambiguity to correct for.
	for (FBlockHalfEdge& Edge : HalfEdges)
	{
		if (Edge.Points.Num() < 2)
		{
			continue;
		}
		const FVector2D Direction = Edge.Points[1] - Edge.Points[0];
		Edge.Angle = FMath::Atan2(Direction.Y, Direction.X);
	}
	for (auto& Pair : NodeOutgoing)
	{
		TArray<int32>& Edges = Pair.Value;
		Edges.Sort([&HalfEdges](int32 A, int32 B) { return HalfEdges[A].Angle < HalfEdges[B].Angle; });
	}

	// 3. Trace every bounded face: from any unvisited half-edge, repeatedly move to "the next
	// half-edge CLOCKWISE from the reverse (twin) of the one just traversed" at the node just
	// arrived at, until the walk returns to the start. Standard planar-subdivision face tracing --
	// every edge is visited exactly once in each direction, and the traced cycles are exactly the
	// graph's faces, one of which per connected component is the unbounded "outside" face.
	//
	// Direction matters and was originally shipped backwards ("next CCW from twin"): at a plain
	// degree-2 node (an ordinary point mid-loop, only two half-edges present) CCW-next and CW-next
	// are the same single other edge, so a loop with no real junction traces correctly either way --
	// which is exactly why the original synthetic test (a single 4-road square, no interior
	// intersections) passed despite the bug. At any node with 3+ edges -- any real intersection,
	// with or without the road-splitting this file also does -- "next CCW from twin" walks straight
	// past the junction and merges every bounded face at that node into one oversized loop instead of
	// tracing them separately. Confirmed both ways with a synthetic square-with-both-diagonals-
	// crossing-at-the-center case (a degree-4 node): CCW-next produces one bogus ~9418 m^2 face plus
	// four wrongly-discarded negative-area triangles; CW-next produces the correct four ~2330 m^2
	// triangles plus one correctly-discarded unbounded face.
	//
	// Sign convention: under a standard (non-mirrored) 2D axis convention with angles increasing
	// counterclockwise (this function's plain UE X/Y, no north/east axis swap), this CW-next rule
	// traces each BOUNDED face counterclockwise -- positive signed area by the shoelace formula --
	// and the unbounded face(s) clockwise (negative), matching the existing Area <= 0.0 discard check
	// below unchanged. This is the OPPOSITE sign from BuildingBlockDetection.cpp, whose ToBlockSpace
	// (X=north, Y=east) axis swap mirrors handedness and flips it. If this project's block count ever
	// comes back suspiciously as "1 huge block, 0 small ones" again, re-check this rule's direction
	// before anything else.
	int32 InvalidCount = 0;
	int32 UnboundedCount = 0;
	int32 RejectedNonSimpleCount = 0;

	for (int32 StartIndex = 0; StartIndex < HalfEdges.Num(); ++StartIndex)
	{
		if (HalfEdges[StartIndex].bVisited)
		{
			continue;
		}

		TArray<FVector2D> FacePoints;
		TArray<FString> EdgeRoadIds;
		int32 Current = StartIndex;
		bool bValid = true;
		int32 SafetyCounter = 0;
		const int32 SafetyLimit = HalfEdges.Num() + 1; // A well-formed planar graph can never need more steps than it has half-edges.
		FBlockHalfEdge* PrevEdge = nullptr; // Null only for the very first edge in this trace -- its own leading corner is fixed up against the LAST edge after the loop closes, below.
		do
		{
			if (++SafetyCounter > SafetyLimit)
			{
				bValid = false;
				break;
			}
			FBlockHalfEdge& Edge = HalfEdges[Current];
			Edge.bVisited = true;
			EdgeRoadIds.Add(Edge.RoadId);

			// The first point of this edge is the corner it shares with PrevEdge -- replace it with
			// their two offset lines' actual intersection when that's well-conditioned (see
			// ComputeCornerPoint's own comment for why the raw point can overshoot past the other
			// edge and fold the polygon). Falls back to the raw point otherwise, same as before this
			// fix existed.
			int32 FirstPointIndex = 0;
			if (PrevEdge && Edge.Points.Num() >= 2)
			{
				FVector2D Corner;
				if (ComputeCornerPoint(*PrevEdge, Edge, Corner))
				{
					FacePoints.Add(Corner);
					FirstPointIndex = 1;
				}
			}
			// Skip the last point (== next edge's first point) except conceptually at the very end,
			// to avoid duplicating the shared vertex at every step -- CleanFootprint below handles
			// the final wrap-around duplicate.
			for (int32 p = FirstPointIndex; p + 1 < Edge.Points.Num(); ++p)
			{
				FacePoints.Add(Edge.Points[p]);
			}
			if (Edge.Points.Num() == 1)
			{
				FacePoints.Add(Edge.Points[0]);
			}

			const int32 Twin = Edge.TwinIndex;
			const TArray<int32>* Outgoing = NodeOutgoing.Find(Edge.ToNode);
			if (!Outgoing || Outgoing->IsEmpty())
			{
				bValid = false;
				break;
			}
			const int32 TwinPos = Outgoing->IndexOfByKey(Twin);
			if (TwinPos == INDEX_NONE)
			{
				bValid = false;
				break;
			}
			PrevEdge = &Edge;
			// Next CLOCKWISE from the twin -- see this function's own comment above on why "next
			// CCW" (the original, incorrect direction) only ever looked right for degree-2 nodes.
			Current = (*Outgoing)[(TwinPos - 1 + Outgoing->Num()) % Outgoing->Num()];
		} while (Current != StartIndex);

		if (!bValid || FacePoints.Num() < 3)
		{
			++InvalidCount;
			continue;
		}

		// Fix up the one corner the loop above couldn't: FacePoints[0], the very first edge's own
		// leading point, added back when PrevEdge was still null. Its true corner partner is the LAST
		// edge processed (PrevEdge now holds it, since the loop only just closed back to StartIndex).
		if (PrevEdge)
		{
			FVector2D Corner;
			if (ComputeCornerPoint(*PrevEdge, HalfEdges[StartIndex], Corner))
			{
				FacePoints[0] = Corner;
			}
		}

		FacePoints = FGrammarGeometry2D::CleanFootprint(FacePoints);
		if (FacePoints.Num() < 3)
		{
			++InvalidCount;
			continue;
		}

		const double Area = FGrammarGeometry2D::SignedPolygonArea(FacePoints);
		if (Area <= 0.0)
		{
			++UnboundedCount; // Unbounded/outer face for this component (or a degenerate dangling-edge spike).
			continue;
		}

		if (!FGrammarParcelSubdivision::IsSimplePolygon(FacePoints))
		{
			// The per-road offset + nearest-point corner merge (see OffsetPolyline's comment) isn't
			// a precise miter join -- occasionally produces a self-crossing near a junction where
			// very differently-sized roads meet. Skip rather than hand subdivision a polygon it
			// isn't prepared for; the block is simply not generated this pass.
			LogNonSimpleBlockDiagnostics(FacePoints);
			++RejectedNonSimpleCount;
			continue;
		}

		FGrammarBlock& Block = Result.AddDefaulted_GetRef();
		Block.Polygon = MoveTemp(FacePoints);
		Block.AreaM2 = Area;
		Block.BoundingRoadIds = MoveTemp(EdgeRoadIds);
	}

	UE_LOG(LogTemp, Log, TEXT("FGrammarBlockExtraction::ExtractBlocks: traced %d loop(s) from %d half-edge(s) (%d input road(s), %d after intersection-splitting, %d intersection(s) found): %d invalid/incomplete, %d unbounded (discarded), %d non-simple (discarded), %d block(s) kept."),
		InvalidCount + UnboundedCount + RejectedNonSimpleCount + Result.Num(), HalfEdges.Num(), Roads.Num(), SplitRoads.Num(), IntersectionCutCount,
		InvalidCount, UnboundedCount, RejectedNonSimpleCount, Result.Num());

	return Result;
}
