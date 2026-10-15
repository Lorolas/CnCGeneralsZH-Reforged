/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// FILE: ParallelSteering.cpp //////////////////////////////////////////////////////////////////////
// Desc:   Deterministic fork-join parallel steering evaluation and zero-allocation query helpers.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Common/GameCommon.h"
#include "Common/JobSystem.h"
#include "GameLogic/ParallelSteering.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/GameLogic.h"
#include <cmath>

Int gatherNearbySteeringNeighbors( const Coord3D *pos, Real maxDist, ParallelSteeringNeighborBuffer *buf )
{
	if( !buf )
		return 0;

	buf->clear();

	if( !ThePartitionManager || !pos )
		return 0;

	Int cellCenterX = 0;
	Int cellCenterY = 0;
	ThePartitionManager->worldToCell( pos->x, pos->y, &cellCenterX, &cellCenterY );

	Int cellRadius = ThePartitionManager->worldToCellDist( maxDist ) + 1;
	if( cellRadius < 1 )
		cellRadius = 1;

	for( Int cy = cellCenterY - cellRadius; cy <= cellCenterY + cellRadius; ++cy )
	{
		for( Int cx = cellCenterX - cellRadius; cx <= cellCenterX + cellRadius; ++cx )
		{
			PartitionCell *cell = ThePartitionManager->getCellAt( cx, cy );
			if( !cell )
				continue;

			for( CellAndObjectIntersection *coi = cell->getFirstCoiInCell(); coi; coi = coi->getNextCoi() )
			{
				PartitionData *data = coi->getModule();
				if( !data )
					continue;

				Object *obj = data->getObject();
				if( !obj || obj->isEffectivelyDead() )
					continue;

				buf->add( obj );
				if( buf->count >= MAX_PARALLEL_STEERING_NEIGHBORS )
					return buf->count;
			}
		}
	}

	return buf->count;
}

void evaluateParallelSteering( Int index, void *context )
{
	ParallelSteeringContext *ctx = static_cast<ParallelSteeringContext *>( context );
	if( !ctx || index < 0 || index >= ctx->count )
		return;

	const ParallelSteeringItem &in = ctx->items[ index ];
	ParallelSteeringOutput &out = ctx->outputs[ index ];

	out.objectID = in.objectID;
	out.adjustedGoalPos.zero();
	out.adjustedSpeed = in.desiredSpeed;
	out.onPathDistToGoal = 0.0f;
	out.bendDist = 0.0f;
	out.bendCos = 1.0f;
	out.layer = in.layer;
	out.blocked = in.blocked;
	out.valid = FALSE;

	if( in.path != NULL && in.object != NULL && in.locomotorSet != NULL )
	{
		if( !in.isGroundMovement )
		{
			out.onPathDistToGoal = in.path->computeFlightDistToGoal( &in.currentPos, out.adjustedGoalPos );
			out.bendDist = 0.0f;
			out.bendCos = 1.0f;
			out.layer = in.layer;
		}
		else
		{
			ClosestPointOnPathInfo info;
			in.path->computePointOnPath( in.object, *in.locomotorSet, in.currentPos, info );
			out.adjustedGoalPos = info.posOnPath;
			out.onPathDistToGoal = info.distAlongPath;
			out.bendDist = info.bendDist;
			out.bendCos = info.bendCos;
			out.layer = info.layer;
		}
		out.valid = TRUE;
	}
	else
	{
		out.adjustedGoalPos = in.initialGoalPos;
		out.onPathDistToGoal = in.onPathDistToGoal;
		out.bendDist = in.bendDist;
		out.bendCos = in.bendCos;
		out.layer = in.layer;
		out.valid = TRUE;
	}
}

void dispatchParallelSteering( ParallelSteeringItem *items, ParallelSteeringOutput *outputs, Int count )
{
	if( !items || !outputs || count <= 0 )
		return;

	ParallelSteeringContext ctx;
	ctx.items = items;
	ctx.outputs = outputs;
	ctx.count = count;

	if( count < PARALLEL_STEERING_MIN_UNITS || JobSystem::workerCount() == 0 )
	{
		for( Int i = 0; i < count; ++i )
			evaluateParallelSteering( i, &ctx );
		return;
	}

	JobSystem::parallel_for( count, PARALLEL_STEERING_GRANULARITY, evaluateParallelSteering, &ctx );
}
