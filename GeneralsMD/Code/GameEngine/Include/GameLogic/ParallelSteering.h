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

#pragma once

#ifndef __PARALLEL_STEERING_H__
#define __PARALLEL_STEERING_H__

#include "Lib/BaseType.h"
#include "Common/GameCommon.h"
#include "Common/GameType.h"
#include "Common/GameMemory.h"
#include "GameLogic/Object.h"

enum LocomotorSetType : Int;
class AIUpdateInterface;
class Locomotor;
class LocomotorSet;
class Path;

enum ParallelSteeringLimits
{
	MAX_PARALLEL_STEERING_ITEMS = 2048,
	PARALLEL_STEERING_MIN_UNITS = 32,
	PARALLEL_STEERING_GRANULARITY = 16,
	MAX_PARALLEL_STEERING_NEIGHBORS = 64
};

struct ParallelSteeringNeighborBuffer
{
	Object *neighbors[MAX_PARALLEL_STEERING_NEIGHBORS];
	Int count;

	void clear() { count = 0; }
	Bool add( Object *obj )
	{
		if ( !obj || count >= MAX_PARALLEL_STEERING_NEIGHBORS )
			return FALSE;
		for ( Int i = 0; i < count; ++i )
		{
			if ( neighbors[i] == obj )
				return FALSE;
		}
		neighbors[count++] = obj;
		return TRUE;
	}
};

/**
 * Immutable snapshot of unit movement state gathered on the main simulation thread
 * for parallel steering evaluation. Workers read this struct without modifying simulation state.
 */
struct ParallelSteeringItem
{
	ObjectID objectID;
	Object *object;
	AIUpdateInterface *ai;
	Locomotor *locomotor;

	Coord3D currentPos;
	Coord3D initialGoalPos;
	Coord3D faceTargetPos;
	Bool hasFaceTarget;
	Bool fighting;

	Real desiredSpeed;
	Real maxSpeed;
	Real bumpSpeedLimit;
	Real curMaxBlockedSpeed;

	Real onPathDistToGoal;
	Real bendDist;
	Real bendCos;
	Real pathExtraDist;

	PathfindLayerEnum layer;
	const LocomotorSet *locomotorSet;
	Path *path;

	Bool isGroundMovement;
	Bool foot;
	Bool sideStep;
	Coord2D sideAway;
	Bool blocked;
	Real boundingRadius;
};

/**
 * Output results produced by parallel steering worker jobs.
 * Committed sequentially on the main thread in strict ObjectID order.
 */
struct ParallelSteeringOutput
{
	ObjectID objectID;
	Coord3D adjustedGoalPos;
	Real adjustedSpeed;
	Real onPathDistToGoal;
	Real bendDist;
	Real bendCos;
	PathfindLayerEnum layer;
	Bool blocked;
	Bool valid;
};

struct ParallelSteeringContext
{
	ParallelSteeringItem *items;
	ParallelSteeringOutput *outputs;
	Int count;
};

/** Zero-allocation spatial neighbor gathering for worker threads. */
Int gatherNearbySteeringNeighbors( const Coord3D *pos, Real maxDist, ParallelSteeringNeighborBuffer *buf );

/** Pure parallel evaluation worker entry point conforming to JobSystem::JobFunc. */
void evaluateParallelSteering( Int index, void *context );

/** Fork-join dispatcher calling JobSystem::parallel_for or running inline based on threshold. */
void dispatchParallelSteering( ParallelSteeringItem *items, ParallelSteeringOutput *outputs, Int count );

#endif // __PARALLEL_STEERING_H__
