#include "Geometry/GrammarRoofSkeleton.h"
#include "Geometry/GrammarGeometry2D.h"

// ---------------------------------------------------------------------------------------------
// Implementation notes (read this before touching anything below):
//
// Every wavefront node's velocity depends only on its two CURRENTLY ADJACENT original edges'
// (fixed, precomputed once) outward normals -- edges never rotate, only translate inward at unit
// speed, so a node's velocity never needs recomputing once its adjacency is known (see
// MiterVelocity). A node's adjacency is captured by just three ints: Prev, Next, and
// NextSourceEdge (the original Footprint edge index that the node's OWN forward/Next wavefront
// edge derives from) -- the node's Prev-side source edge is simply Nodes[Prev].NextSourceEdge, so
// it doesn't need its own field.
//
// Each original Footprint edge maps to one or more "face fragments" (FFaceBuilder): a fragment
// tracks ONE currently-active wavefront edge via two growing chains, LeftChain (starting at the
// fragment's base-left vertex, appended-to whenever its current left endpoint gets replaced) and
// RightChain (same, base-right). A fragment finishes -- becomes one output FFace -- only via an
// edge event (its own two endpoints finally converge) or by being frozen at the MaxDistance cap.
// A split event does NOT finish the fragment whose edge was crashed into; instead it is replaced
// by two NEW fragments (see ProcessSplitEvent), each inheriting one of the old fragment's chains
// unchanged and starting fresh on the other side -- worked out in detail in ProcessSplitEvent's
// own comment, since this is the one place a naive implementation most easily goes wrong.
//
// The three reflex-vertex fragments touched by a split (the crashed-into edge itself, plus the
// reflex vertex's own two neighboring fragments) are the only fragments a single split event ever
// modifies; every other active fragment elsewhere in the polygon is untouched.
//
// Finished fragments read out as [LeftChain[0]] ++ RightChain ++ reverse(LeftChain[1:]) -- i.e.
// base-left, straight across to base-right, up the right side in chronological order, across
// whatever the top currently is (a single apex, or two frozen points for a capped fragment), then
// back down the left side in reverse-chronological order. This exact order (not the more obvious
// LeftChain ++ reverse(RightChain)) is required to match GabledRoofMesh's own
// {Base[i], Base[i+1], Ridge[i+1], Ridge[i]} winding convention -- verified by hand against a
// degenerate single-apex triangle case; get this backwards and every hip face's normal points
// into the roof instead of out of it.
// ---------------------------------------------------------------------------------------------

namespace
{
	constexpr double SkelEpsilon = 1e-4;
	constexpr double SkelNoEvent = TNumericLimits<double>::Max();

	struct FSkelNode
	{
		FVector2D Origin = FVector2D::ZeroVector;
		FVector2D Velocity = FVector2D::ZeroVector;
		double Time = 0.0;
		int32 Prev = INDEX_NONE;
		int32 Next = INDEX_NONE;
		int32 NextSourceEdge = INDEX_NONE;
		int32 PrevFaceBuilder = INDEX_NONE;
		int32 NextFaceBuilder = INDEX_NONE;
		int32 LoopId = INDEX_NONE;
		bool bReflex = false;
		bool bActive = true;

		FVector2D PositionAt(double T) const { return Origin + Velocity * (T - Time); }
	};

	struct FFaceBuilder
	{
		int32 SourceEdgeIndex = INDEX_NONE;
		TArray<int32> LeftChain;
		TArray<int32> RightChain;
		bool bFinished = false;
	};

	// Velocity of a vertex whose two adjacent (fixed-direction) edges have outward normals
	// NormalPrev/NormalNext -- the standard polygon-offset miter-join construction, but signed for
	// INWARD recession (Dot(Velocity, NormalPrev) = Dot(Velocity, NormalNext) = -1, so that a point
	// on each edge's own inward-offset line, Dot(X-Q0,N) = -t, is satisfied for all t -- see this
	// file's own derivation), i.e. the NEGATIVE of the outward-pointing bisector direction. Unlike
	// FGrammarRoofFrameMath::RoofBaseVertices' own one-shot OUTWARD overhang offset (whose formula
	// this was first adapted from -- do not reuse its sign here), this must also NOT clamp the miter
	// scale: the whole event simulation depends on every node moving at its true velocity so that
	// event times are computed correctly. A very sharp corner genuinely does move fast inward in a
	// correct straight skeleton -- that's expected, not a bug, and the event simulation resolves it
	// (such a node triggers an edge/split event almost immediately).
	FVector2D MiterVelocity(const FVector2D& NormalPrev, const FVector2D& NormalNext)
	{
		const FVector2D Sum = NormalPrev + NormalNext;
		const double SumLenSq = Sum.SizeSquared();
		if (SumLenSq <= SkelEpsilon * SkelEpsilon)
		{
			// Anti-parallel adjacent edges (a zero-width spike vertex) -- pathological input for a
			// building footprint. Fall back to a large-but-finite perpendicular velocity, oriented
			// consistently (and still inward, matching the main case below), rather than a true
			// infinite-speed spike.
			const FVector2D Perp(-NormalPrev.Y, NormalPrev.X);
			const double Sign = (FVector2D::DotProduct(Perp, NormalNext) >= 0.0) ? 1.0 : -1.0;
			return Perp * Sign * -8.0;
		}
		const FVector2D Bisector = Sum / FMath::Sqrt(SumLenSq);
		// Mathematically guaranteed in (0,1] once the near-zero Sum case above is excluded (cosine
		// of half the angle between two unit vectors, which spans [0,180) here) -- the Max() is a
		// numerical-noise safety net only, not a behavior-changing clamp.
		const double CosHalfAngle = FMath::Max(FVector2D::DotProduct(Bisector, NormalPrev), SkelEpsilon);
		return Bisector * (-1.0 / CosHalfAngle);
	}

	bool ComputeIsReflex(int32 PrevSourceEdge, int32 NextSourceEdge, const TArray<FVector2D>& EdgeDirections)
	{
		const FVector2D& DirIn = EdgeDirections[PrevSourceEdge];
		const FVector2D& DirOut = EdgeDirections[NextSourceEdge];
		const double Cross = DirIn.X * DirOut.Y - DirIn.Y * DirOut.X;
		return Cross < -SkelEpsilon;
	}

	// Time at which A and B (each moving at constant velocity) reach the same position, or
	// SkelNoEvent if never in the future (parallel/lockstep motion, or the crossing is in the
	// past). A and B are always constrained (by construction) to lie on the same moving line at
	// all times, so solving via a single (the numerically larger) component is exact, not an
	// approximation.
	double EdgeEventTime(const FSkelNode& A, const FSkelNode& B, double Now)
	{
		const FVector2D C = (A.Origin - A.Velocity * A.Time) - (B.Origin - B.Velocity * B.Time);
		const FVector2D D = B.Velocity - A.Velocity;
		const double DLenSq = D.SizeSquared();
		if (DLenSq <= SkelEpsilon * SkelEpsilon)
		{
			return SkelNoEvent;
		}
		const double T = (FMath::Abs(D.X) >= FMath::Abs(D.Y)) ? (C.X / D.X) : (C.Y / D.Y);
		return (T > Now + SkelEpsilon) ? T : SkelNoEvent;
	}

	// Time at which reflex node R's swept path crosses the infinite line of the (fixed-direction,
	// receding) edge with outward normal EdgeNormal passing through EdgeBasePoint at time 0, or
	// SkelNoEvent if never in the future.
	double SplitLineTime(const FSkelNode& R, const FVector2D& EdgeNormal, const FVector2D& EdgeBasePoint, double Now)
	{
		const double A = FVector2D::DotProduct(R.Origin - EdgeBasePoint, EdgeNormal);
		const double B = FVector2D::DotProduct(R.Velocity, EdgeNormal);
		const double Denom = B + 1.0;
		if (FMath::Abs(Denom) <= SkelEpsilon)
		{
			return SkelNoEvent;
		}
		const double T = (B * R.Time - A) / Denom;
		return (T > Now + SkelEpsilon) ? T : SkelNoEvent;
	}
}

bool FGrammarRoofSkeleton::Build(const TArray<FVector2D>& Footprint, double MaxDistance, FResult& OutResult)
{
	OutResult = FResult();
	const int32 N = Footprint.Num();
	if (N < 3)
	{
		return false;
	}

	TArray<FVector2D> EdgeDirections;
	TArray<FVector2D> EdgeNormals;
	EdgeDirections.Reserve(N);
	EdgeNormals.Reserve(N);
	for (int32 Index = 0; Index < N; ++Index)
	{
		const FVector2D& Start = Footprint[Index];
		const FVector2D& End = Footprint[(Index + 1) % N];
		EdgeDirections.Add(FGrammarGeometry2D::Tangent(Start, End));
		EdgeNormals.Add(FGrammarGeometry2D::OutwardNormal(Start, End, /*bCCW=*/true));
	}

	TArray<FSkelNode> Nodes;
	Nodes.Reserve(N * 4);
	Nodes.SetNum(N);
	for (int32 Index = 0; Index < N; ++Index)
	{
		FSkelNode& Node = Nodes[Index];
		Node.Origin = Footprint[Index];
		Node.Time = 0.0;
		Node.Prev = (Index - 1 + N) % N;
		Node.Next = (Index + 1) % N;
		Node.NextSourceEdge = Index;
		Node.LoopId = 0;
	}
	for (int32 Index = 0; Index < N; ++Index)
	{
		FSkelNode& Node = Nodes[Index];
		const int32 PrevSource = Nodes[Node.Prev].NextSourceEdge;
		Node.Velocity = MiterVelocity(EdgeNormals[PrevSource], EdgeNormals[Node.NextSourceEdge]);
		Node.bReflex = ComputeIsReflex(PrevSource, Node.NextSourceEdge, EdgeDirections);
	}

	TArray<FFaceBuilder> FaceBuilders;
	FaceBuilders.Reserve(N * 3);
	FaceBuilders.SetNum(N);
	for (int32 Index = 0; Index < N; ++Index)
	{
		FFaceBuilder& FB = FaceBuilders[Index];
		FB.SourceEdgeIndex = Index;
		FB.LeftChain = { Index };
		FB.RightChain = { (Index + 1) % N };
		Nodes[Index].NextFaceBuilder = Index;
		Nodes[(Index + 1) % N].PrevFaceBuilder = Index;
	}

	int32 NextLoopId = 1;

	// ---- event processing ----

	auto ProcessEdgeEvent = [&](int32 X, int32 Y, double T)
	{
		const int32 W = Nodes[X].Prev;
		const int32 Z = Nodes[Y].Next;
		// A loop reduced to exactly 3 nodes always finishes with all 3 remaining fragments closing
		// at the SAME point simultaneously (the offset-incenter of the remaining triangle -- every
		// triangle's 3 sides reach a common inradius-distance point at the same time by
		// definition). Detected here as W==Z (the "third" node is both X's Prev and Y's Next).
		const bool bFinalTriangle = (W == Z);

		const int32 M = Nodes.Add(FSkelNode());
		Nodes[M].Origin = Nodes[X].PositionAt(T);
		Nodes[M].Time = T;
		Nodes[M].LoopId = Nodes[X].LoopId;
		Nodes[M].bActive = !bFinalTriangle;

		const int32 FBxyIndex = Nodes[X].NextFaceBuilder;
		FaceBuilders[FBxyIndex].LeftChain.Add(M);
		FaceBuilders[FBxyIndex].bFinished = true;

		Nodes[X].bActive = false;
		Nodes[Y].bActive = false;

		if (bFinalTriangle)
		{
			const int32 FBwxIndex = Nodes[W].NextFaceBuilder; // W -> X
			FaceBuilders[FBwxIndex].RightChain.Add(M);
			FaceBuilders[FBwxIndex].bFinished = true;

			const int32 FByzIndex = Nodes[Y].NextFaceBuilder; // Y -> W(==Z)
			FaceBuilders[FByzIndex].LeftChain.Add(M);
			FaceBuilders[FByzIndex].bFinished = true;

			Nodes[W].bActive = false;
			return;
		}

		Nodes[M].Prev = W;
		Nodes[M].Next = Z;
		Nodes[M].NextSourceEdge = Nodes[Y].NextSourceEdge;
		Nodes[W].Next = M;
		Nodes[Z].Prev = M;

		const int32 FBwxIndex = Nodes[W].NextFaceBuilder; // W's forward edge, currently ending at X
		FaceBuilders[FBwxIndex].RightChain.Add(M);
		Nodes[M].PrevFaceBuilder = FBwxIndex;

		const int32 FByzIndex = Nodes[Y].NextFaceBuilder; // Y's forward edge, now starting from M
		FaceBuilders[FByzIndex].LeftChain.Add(M);
		Nodes[M].NextFaceBuilder = FByzIndex;

		const int32 MPrevSource = Nodes[W].NextSourceEdge;
		Nodes[M].Velocity = MiterVelocity(EdgeNormals[MPrevSource], EdgeNormals[Nodes[M].NextSourceEdge]);
		Nodes[M].bReflex = ComputeIsReflex(MPrevSource, Nodes[M].NextSourceEdge, EdgeDirections);
	};

	// R (reflex) crashes into the middle of wavefront edge (X, Y=X.Next) at time T. Splits the
	// loop containing them into two: one running X -> (new)PA -> RNext -> ... -> W(loop A's own
	// far side) -> back to X, the other running (new)PB -> Y -> ... -> RPrev -> back to PB. See
	// this file's own top-of-file comment for the full derivation -- this is the one place a
	// naive implementation most easily gets the bookkeeping wrong.
	auto ProcessSplitEvent = [&](int32 R, int32 X, double T)
	{
		const int32 Y = Nodes[X].Next;
		const int32 RPrev = Nodes[R].Prev;
		const int32 RNext = Nodes[R].Next;

		const int32 FBxyIndex = Nodes[X].NextFaceBuilder; // == Nodes[Y].PrevFaceBuilder
		const int32 FBfIndex = Nodes[RPrev].NextFaceBuilder; // R's Prev-edge fragment (RPrev -> R)
		const int32 FBgIndex = Nodes[R].NextFaceBuilder; // R's Next-edge fragment (R -> RNext)

		const int32 Sab = Nodes[X].NextSourceEdge;
		const int32 Sg = Nodes[R].NextSourceEdge;
		const int32 Sf = Nodes[RPrev].NextSourceEdge;

		const FVector2D CrashPos = Nodes[R].PositionAt(T);

		const int32 PA = Nodes.Add(FSkelNode()); // continues towards RNext (takes over Sg)
		const int32 PB = Nodes.Add(FSkelNode()); // continues towards RPrev's side (takes over Sf)

		Nodes[PA].Origin = CrashPos;
		Nodes[PA].Time = T;
		Nodes[PA].Prev = X;
		Nodes[PA].Next = RNext;
		Nodes[PA].NextSourceEdge = Sg;
		Nodes[PA].Velocity = MiterVelocity(EdgeNormals[Sab], EdgeNormals[Sg]);
		Nodes[PA].bReflex = ComputeIsReflex(Sab, Sg, EdgeDirections);

		Nodes[PB].Origin = CrashPos;
		Nodes[PB].Time = T;
		Nodes[PB].Prev = RPrev;
		Nodes[PB].Next = Y;
		Nodes[PB].NextSourceEdge = Sab;
		Nodes[PB].Velocity = MiterVelocity(EdgeNormals[Sf], EdgeNormals[Sab]);
		Nodes[PB].bReflex = ComputeIsReflex(Sf, Sab, EdgeDirections);

		Nodes[X].Next = PA;
		Nodes[RPrev].Next = PB;
		Nodes[RNext].Prev = PA;
		Nodes[Y].Prev = PB;
		Nodes[R].bActive = false;

		// FBxy splits into two continuing fragments: one keeps FBxy's existing left history
		// (unchanged) and starts a fresh right side at PA; the other starts fresh on the left at
		// PB and keeps FBxy's existing right history (unchanged). FBxy itself is discarded --
		// bFinished is deliberately left false, so it's never emitted as an output face.
		const int32 FBxyA = FaceBuilders.Add(FFaceBuilder());
		FaceBuilders[FBxyA].SourceEdgeIndex = Sab;
		FaceBuilders[FBxyA].LeftChain = FaceBuilders[FBxyIndex].LeftChain;
		FaceBuilders[FBxyA].RightChain = { PA };

		const int32 FBxyB = FaceBuilders.Add(FFaceBuilder());
		FaceBuilders[FBxyB].SourceEdgeIndex = Sab;
		FaceBuilders[FBxyB].LeftChain = { PB };
		FaceBuilders[FBxyB].RightChain = FaceBuilders[FBxyIndex].RightChain;

		// FBf and FBg continue (do not finish), each gaining one appended point -- R remains as a
		// genuine historical boundary point in both, PA/PB appended after it.
		FaceBuilders[FBfIndex].RightChain.Add(PB);
		FaceBuilders[FBgIndex].LeftChain.Add(PA);

		Nodes[X].NextFaceBuilder = FBxyA;
		Nodes[PA].PrevFaceBuilder = FBxyA;
		Nodes[PA].NextFaceBuilder = FBgIndex;
		Nodes[RNext].PrevFaceBuilder = FBgIndex;
		Nodes[RPrev].NextFaceBuilder = FBfIndex;
		Nodes[PB].PrevFaceBuilder = FBfIndex;
		Nodes[PB].NextFaceBuilder = FBxyB;
		Nodes[Y].PrevFaceBuilder = FBxyB;

		// The old loop is replaced by two new ones -- relabel every node reachable from each side.
		const int32 LoopIdA = NextLoopId++;
		const int32 LoopIdB = NextLoopId++;
		auto RelabelLoop = [&Nodes](int32 Start, int32 NewLoopId)
		{
			int32 Cur = Start;
			int32 Safety = 0;
			do
			{
				Nodes[Cur].LoopId = NewLoopId;
				Cur = Nodes[Cur].Next;
			} while (Cur != Start && ++Safety < 100000);
		};
		RelabelLoop(X, LoopIdA);
		RelabelLoop(PB, LoopIdB);
	};

	// ---- main simulation loop ----

	double Now = 0.0;
	// True once the sweep stops for a LEGITIMATE reason: nothing left to simulate, or an event
	// genuinely landed at/past MaxDistance. False if the `for` loop instead runs out of
	// MaxIterations -- a safety net for pathological inputs that never cleanly converge (e.g. a
	// numerically imperfect polygon, such as a straight-skeleton TopRing fed back into a second
	// Build() call, stuck re-finding near-epsilon events without real progress). See the freeze
	// step below for why this distinction matters.
	bool bStoppedLegitimately = false;
	const int32 MaxIterations = FMath::Max(N * 8 + 64, 256);
	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
	{
		TMap<int32, TArray<int32>> Loops;
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if (Nodes[Index].bActive)
			{
				Loops.FindOrAdd(Nodes[Index].LoopId).Add(Index);
			}
		}
		if (Loops.Num() == 0)
		{
			bStoppedLegitimately = true;
			break;
		}

		double BestTime = SkelNoEvent;
		bool bBestIsSplit = false;
		int32 BestA = INDEX_NONE;
		int32 BestB = INDEX_NONE;

		for (const TPair<int32, TArray<int32>>& LoopPair : Loops)
		{
			const TArray<int32>& LoopNodes = LoopPair.Value;
			if (LoopNodes.Num() < 3)
			{
				continue; // shouldn't persist (see ProcessEdgeEvent's final-triangle case), but skip defensively
			}
			for (int32 X : LoopNodes)
			{
				const double T = EdgeEventTime(Nodes[X], Nodes[Nodes[X].Next], Now);
				if (T < BestTime)
				{
					BestTime = T;
					bBestIsSplit = false;
					BestA = X;
					BestB = Nodes[X].Next;
				}
			}
			for (int32 R : LoopNodes)
			{
				if (!Nodes[R].bReflex)
				{
					continue;
				}
				for (int32 X : LoopNodes)
				{
					if (X == R || X == Nodes[R].Prev)
					{
						continue; // R's own two adjacent edges can't be genuine split targets
					}
					const int32 SrcEdge = Nodes[X].NextSourceEdge;
					const double T = SplitLineTime(Nodes[R], EdgeNormals[SrcEdge], Footprint[SrcEdge], Now);
					if (T >= BestTime)
					{
						continue;
					}
					const int32 Y = Nodes[X].Next;
					const FVector2D P = Nodes[R].PositionAt(T);
					const FVector2D XPos = Nodes[X].PositionAt(T);
					const FVector2D YPos = Nodes[Y].PositionAt(T);
					const FVector2D EdgeVec = YPos - XPos;
					const double EdgeLenSq = EdgeVec.SizeSquared();
					if (EdgeLenSq <= SkelEpsilon * SkelEpsilon)
					{
						continue; // this edge is itself about to vanish -- let its own edge event handle it
					}
					const double Param = FVector2D::DotProduct(P - XPos, EdgeVec) / EdgeLenSq;
					if (Param < SkelEpsilon || Param > 1.0 - SkelEpsilon)
					{
						continue; // too close to either endpoint -- edge-event territory, not a genuine split
					}
					BestTime = T;
					bBestIsSplit = true;
					BestA = R;
					BestB = X;
				}
			}
		}

		if (BestTime == SkelNoEvent || BestTime >= MaxDistance)
		{
			bStoppedLegitimately = true;
			break;
		}
		Now = BestTime;

		if (bBestIsSplit)
		{
			ProcessSplitEvent(BestA, BestB, Now);
		}
		else
		{
			ProcessEdgeEvent(BestA, BestB, Now);
		}
	}

	// ---- freeze whatever's still active at MaxDistance into flat-top rings, closing every
	//      still-open fragment with its frozen (2-point) top ----
	{
		// Freezing at MaxDistance is only correct when the sweep legitimately reached it (or fully
		// converged, in which case no loops remain active and this value is never used below).
		// If the MaxIterations safety net is what actually stopped the loop above, MaxDistance may
		// still be TNumericLimits<double>::Max() (the uncapped-skeleton convention -- see this
		// class's header comment) -- PositionAt(DBL_MAX) with any nonzero velocity would then place
		// the frozen node astronomically far from the building, in whichever direction that node
		// happens to be receding (the "roof extrudes toward infinity" failure). Freeze at the last
		// time actually reached instead, which is always finite and never worse than what the
		// simulation had already legitimately computed.
		const double FreezeDistance = bStoppedLegitimately ? MaxDistance : Now;

		TMap<int32, TArray<int32>> RemainingLoops;
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if (Nodes[Index].bActive)
			{
				RemainingLoops.FindOrAdd(Nodes[Index].LoopId).Add(Index);
			}
		}

		TMap<int32, int32> FrozenNodeFor;
		for (const TPair<int32, TArray<int32>>& LoopPair : RemainingLoops)
		{
			if (LoopPair.Value.Num() == 0)
			{
				continue;
			}
			TArray<int32> OrderedLoopNodes;
			{
				const int32 Start = LoopPair.Value[0];
				int32 Cur = Start;
				int32 Safety = 0;
				do
				{
					OrderedLoopNodes.Add(Cur);
					Cur = Nodes[Cur].Next;
				} while (Cur != Start && ++Safety < LoopPair.Value.Num() + 1);
			}

			TArray<int32> Ring;
			Ring.Reserve(OrderedLoopNodes.Num());
			for (int32 NodeIndex : OrderedLoopNodes)
			{
				int32 FrozenIndex;
				if (const int32* Existing = FrozenNodeFor.Find(NodeIndex))
				{
					FrozenIndex = *Existing;
				}
				else
				{
					FSkelNode Frozen;
					Frozen.Origin = Nodes[NodeIndex].PositionAt(FreezeDistance);
					Frozen.Time = FreezeDistance;
					FrozenIndex = Nodes.Add(Frozen);
					FrozenNodeFor.Add(NodeIndex, FrozenIndex);
				}
				Ring.Add(FrozenIndex);
			}
			OutResult.TopRings.Add(Ring);

			for (int32 NodeIndex : OrderedLoopNodes)
			{
				const int32 FBIndex = Nodes[NodeIndex].NextFaceBuilder;
				FFaceBuilder& FB = FaceBuilders[FBIndex];
				if (FB.bFinished)
				{
					continue;
				}
				const int32 NextNode = Nodes[NodeIndex].Next;
				FB.LeftChain.Add(FrozenNodeFor[NodeIndex]);
				FB.RightChain.Add(FrozenNodeFor[NextNode]);
				FB.bFinished = true;
			}
		}
	}

	// ---- emit output ----

	OutResult.Nodes.Reserve(Nodes.Num());
	for (const FSkelNode& Node : Nodes)
	{
		OutResult.Nodes.Add(FNode{ Node.Origin, Node.Time });
	}

	for (const FFaceBuilder& FB : FaceBuilders)
	{
		if (!FB.bFinished || FB.LeftChain.Num() == 0 || FB.RightChain.Num() == 0)
		{
			continue;
		}
		FFace Face;
		Face.SourceEdgeIndex = FB.SourceEdgeIndex;
		Face.NodeIndices.Reserve(FB.LeftChain.Num() + FB.RightChain.Num());
		Face.NodeIndices.Add(FB.LeftChain[0]);
		Face.NodeIndices.Append(FB.RightChain);
		for (int32 Index = FB.LeftChain.Num() - 1; Index >= 1; --Index)
		{
			Face.NodeIndices.Add(FB.LeftChain[Index]);
		}
		OutResult.Faces.Add(MoveTemp(Face));
	}

	return true;
}
