/*
 * vcacheopt.h - Vertex Cache Optimizer
 * Copyright 2009 Michael Georgoulpoulos <mgeorgoulopoulos at gmail>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _VCACHEOPT_H_
#define _VCACHEOPT_H_

#include <vector>
#include "cwMath.h"
#include <assert.h>

class VertexCacheData
{
public:
	int position_in_cache;
	float current_score;
	int total_valence; // toatl number of triangles using this vertex
	int remaining_valence; // number of triangles using it but not yet rendered
	std::vector<int> tri_indices; // indices to the indices that use this vertex
	bool calculated; // was the score calculated during this iteration?


	int FindTriangle(int tri)
	{
		for (int i=0; i<(int)tri_indices.size(); i++)
		{
			if (tri_indices[i] == tri) return i;
		}

		return -1;
	}

	void MoveTriangleToEnd(int tri)
	{
		int t_ind = FindTriangle(tri);

		assert(t_ind >= 0);

		tri_indices.erase(tri_indices.begin() + t_ind,
			tri_indices.begin() + t_ind + 1);

		tri_indices.push_back(tri);
	}

	VertexCacheData()
	{
		position_in_cache = -1;
		current_score = 0.0f;
		total_valence = 0;
		remaining_valence = 0;
	}
};

class TriangleCacheData
{
public:
	bool rendered; // has the triangle been added to the draw list yet?
	float current_score; // sum of the score of its vertices
	int verts[3]; // indices to the triangle's vertices
	bool calculated; // was the score calculated during this iteration?

	TriangleCacheData()
	{
		rendered = false;
		current_score = 0.0f;
		verts[0] = verts[1] = verts[2] = -1;
		calculated = false;
	}
};

class VertexCache
{
protected:
	int cache[40];
	int misses; // cache miss count

	int FindVertex(int v)
	{
		for (int i=0; i<32; i++)
		{
			if (cache[i] == v) return i;
		}

		return -1;
	}

	void RemoveVertex(int stack_index)
	{
		for (int i=stack_index; i<38; i++)
		{
			cache[i] = cache[i+1];
		}
	}

public:
	// the vertex will be placed on top
	// if the vertex didn't exist previewsly in
	// the cache, then miss count is incermented
	void AddVertex(int v)
	{
		int w = FindVertex(v);
		if (w >= 0)
		{
			// remove the vertex from the cache (to reinsert it later on the top)
			RemoveVertex(w);
		}
		else
		{
			// the vertex was not found in the cache - increment misses
			misses++;
		}

		// shift all vertices down (to make room for the new top vertex)
		for (int i=39; i>0; i--)
		{
			cache[i] = cache[i-1];
		}

		// add the new vertex on top
		cache[0] = v;
	}

	void Clear()
	{
		for (int i=0; i<40; i++) cache[i] = -1;
		misses = 0;
	}

	VertexCache()
	{
		Clear();
	}

	int GetCacheMissCount()
	{
		return misses;
	}

	int GetCachedVertex(int which)
	{
		return cache[which];
	}

	int GetCacheMissCount(int *inds, int tri_count)
	{
		Clear();

		for (int i=0; i<3*tri_count; i++)
		{
			AddVertex(inds[i]);
		}

		return misses;
	}
};

class VertexCacheOptimizer
{
public:
	// CalculateVertexScore constants
	float CacheDecayPower;
	float LastTriScore;
	float ValenceBoostScale;
	float ValenceBoostPower;

	enum Result
	{
		Success = 0,
		Fail_BadIndex,
		Fail_NoVerts
	};

	static bool Failed(Result r)
	{
		return r != Success;
	}

protected:
	std::vector<VertexCacheData> verts;
	std::vector<TriangleCacheData> tris;
	std::vector<int> inds;
	int best_tri; // the next triangle to add to the render list
	VertexCache vertex_cache;
	std::vector<int> draw_list;

	float CalculateVertexScore(int vertex)
	{
		VertexCacheData *v = &verts[vertex];
		if (v->remaining_valence <= 0)
		{
			// No tri needs this vertex!
			return -1.0f;
		}
	 
		float ret = 0.0f;
		if (v->position_in_cache < 0)
		{
			// Vertex is not in FIFO cache - no score.
		}
		else
		{
			if (v->position_in_cache < 3)
			{
				// This vertex was used in the last triangle,
				// so it has a fixed score, whichever of the three
				// it's in. Otherwise, you can get very different
				// answers depending on whether you add
				// the triangle 1,2,3 or 3,1,2 - which is silly.
				ret = LastTriScore;
			}
			else
			{
				// Points for being high in the cache.
				const float Scaler = 1.0f / (32  - 3);
				ret = 1.0f - (v->position_in_cache - 3) * Scaler;
				ret = powf(ret, CacheDecayPower);
			}
		}
	 
		// Bonus points for having a low number of tris still to
		// use the vert, so we get rid of lone verts quickly.
		float valence_boost = powf((float)v->remaining_valence, -ValenceBoostPower);
		ret += ValenceBoostScale * valence_boost;
	 
		return ret;
	}

	// returns the index of the triangle with the highest score
	// (or -1, if there aren't any active triangles)
	int FullScoreRecalculation()
	{
		// calculate score for all vertices
		for (int i=0; i<(int)verts.size(); i++)
		{
			verts[i].current_score = CalculateVertexScore(i);
		}

		// calculate scores for all active triangles
		float max_score;
		int max_score_tri = -1;
		bool first_time = true;
		for (int i=0; i<(int)tris.size(); i++)
		{
			if (tris[i].rendered) continue;
			// sum the score of all the triangle's vertices
			float sc = verts[tris[i].verts[0]].current_score +
				verts[tris[i].verts[1]].current_score +
				verts[tris[i].verts[2]].current_score;
			
			tris[i].current_score = sc;
	
			if (first_time || sc > max_score)
			{
				first_time = false;
				max_score = sc;
				max_score_tri = i;
			}
		}

		return max_score_tri;
	}

	Result InitialPass()
	{
		for (int i=0; i<(int)inds.size(); i++)
		{
			int index = inds[i];
			if (index < 0 || index >= (int)verts.size()) return Fail_BadIndex;

			verts[index].total_valence++;
			verts[index].remaining_valence++;
			
			verts[index].tri_indices.push_back(i/3);
		}

		best_tri = FullScoreRecalculation();

		return Success;
	}

	Result Init(int *inds, int tri_count, int vertex_count)
	{
		// clear the draw list
		draw_list.clear();

		// allocate and initialize vertices and triangles
		verts.clear();
		for (int i=0; i<vertex_count; i++) verts.push_back(VertexCacheData());
		
		tris.clear();
		for (int i=0; i<tri_count; i++)
		{
			TriangleCacheData dat;
			for (int j=0; j<3; j++)
			{
				dat.verts[j] = inds[i * 3 + j];
			}
			tris.push_back(dat);
		}

		// copy the indices
		this->inds.clear();
		for (int i=0; i<tri_count * 3; i++) this->inds.push_back(inds[i]);

		vertex_cache.Clear();
		best_tri = -1;

		return InitialPass();
	}

	void AddTriangleToDrawList(int tri)
	{
		// reset all cache positions
		for (int i=0; i<32; i++)
		{
			int ind = vertex_cache.GetCachedVertex(i);
			if (ind < 0) continue;
			verts[ind].position_in_cache = -1;
		}

		TriangleCacheData *t = &tris[tri];
		if (t->rendered) return; // triangle is already in the draw list
	
		for (int i=0; i<3; i++)
		{
			// add all triangle vertices to the cache
			vertex_cache.AddVertex(t->verts[i]);

			VertexCacheData *v = &verts[t->verts[i]];

			// decrease remaining velence
			v->remaining_valence--;

			// move the added triangle to the end of the vertex's
			// triangle index list, so that the first 'remaining_valence'
			// triangles in the list are the active ones
			v->MoveTriangleToEnd(tri);
		}

		draw_list.push_back(tri);

		t->rendered = true;

		// update all vertex cache positions
		for (int i=0; i<32; i++)
		{
			int ind = vertex_cache.GetCachedVertex(i);
			if (ind < 0) continue;
			verts[ind].position_in_cache = i;
		}

	}

	// Optimization: to avoid duplicate calculations durind the same iteration,
	// both vertices and triangles have a 'calculated' flag. This flag
	// must be cleared at the beginning of the iteration to all *active* triangles
	// that have one or more of their vertices currently cached, and all their
	// other vertices.
	// If there aren't any active triangles in the cache, the function returns
	// false and full recalculation is performed.
	bool CleanCalculationFlags()
	{
		bool ret = false;
		for (int i=0; i<32; i++)
		{
			int vert = vertex_cache.GetCachedVertex(i);
			if (vert < 0) continue;

			VertexCacheData *v = &verts[vert];

			for (int j=0; j<v->remaining_valence; j++)
			{
				TriangleCacheData *t = &tris[v->tri_indices[j]];

				// we actually found a triangle to process
				ret = true;

				// clear triangle flag
				t->calculated = false;

				// clear vertex flags
				for (int tri_vert=0; tri_vert<3; tri_vert++)
				{
					verts[t->verts[tri_vert]].calculated = false;
				}
			}
		}

		return ret;
	}

	void TriangleScoreRecalculation(int tri)
	{
		TriangleCacheData *t = &tris[tri];

		// calculate vertex scores
		float sum = 0.0f;
		for (int i=0; i<3; i++)
		{
			VertexCacheData *v = &verts[t->verts[i]];
			float sc = v->current_score;
			if (!v->calculated)
			{
				sc = CalculateVertexScore(t->verts[i]);
			}
			v->current_score = sc;
			v->calculated = true;
			sum += sc;
		}

		t->current_score = sum;
		t->calculated = true;
	}

	int PartialScoreRecalculation()
	{
		// iterate through all the vertices of the cache
		bool first_time = true;
		float max_score;
		int max_score_tri = -1;
		for (int i=0; i<32; i++)
		{
			int vert = vertex_cache.GetCachedVertex(i);
			if (vert < 0) continue;

			VertexCacheData *v = &verts[vert];

			// iterate through all *active* triangles of this vertex
			for (int j=0; j<v->remaining_valence; j++)
			{
				int tri = v->tri_indices[j];
				TriangleCacheData *t = &tris[tri];
				if (!t->calculated)
				{
					// calculate triangle score
					TriangleScoreRecalculation(tri);
				}
				
				float sc = t->current_score;

				// we actually found a triangle to process
				if (first_time || sc > max_score)
				{
					first_time = false;
					max_score = sc;
					max_score_tri = tri;
				}
			}
		}

		return max_score_tri;
	}

	// returns true while there are more steps to take
	// false when optimization is complete
	bool Iterate()
	{
		if (draw_list.size() == tris.size()) return false;

		// add the selected triangle to the draw list
		AddTriangleToDrawList(best_tri);

		// recalculate vertex and triangle scores and
		// select the best triangle for the next iteration
		if (CleanCalculationFlags())
		{
			best_tri = PartialScoreRecalculation();
		}
		else
		{
			best_tri = FullScoreRecalculation();
		}

		return true;
	}

public:
	VertexCacheOptimizer()
	{
		// initialize constants
		CacheDecayPower = 1.5f;
		LastTriScore = 0.75f;
		ValenceBoostScale = 2.0f;
		ValenceBoostPower = 0.5f;

		best_tri = 0;
	}
	
	// stores new indices in place
	Result Optimize(int *inds, int tri_count)
	{
		// find vertex count
		int max_vert = -1;
		for (int i=0; i<tri_count * 3; i++)
		{
			if (inds[i] > max_vert) max_vert = inds[i];
		}

		if (max_vert == -1) return Fail_NoVerts;

		Result res = Init(inds, tri_count, max_vert + 1);
		if (res) return res;

		// iterate until Iterate returns false
		while (Iterate());

		// rewrite optimized index list
		for (int i=0; i<(int)draw_list.size(); i++)
		{
			inds[3 * i + 0] = tris[draw_list[i]].verts[0];
			inds[3 * i + 1] = tris[draw_list[i]].verts[1];
			inds[3 * i + 2] = tris[draw_list[i]].verts[2];
		}

		return Success;
	}
};

#endif // ndef _VCACHEOPT_H_
