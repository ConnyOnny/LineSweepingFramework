#pragma once

#include <vector>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <assert.h>
#include "common.hpp"

template <typename Num_t, size_t dimension>
class PointSpreader
{
protected:
	VectorVec<Num_t,dimension> shadow_points;
	static Num_t dst(const Num_t *a, const Num_t *b) {
		Num_t result = 0;
		for (int i = 0; i < dimension; i++) {
			Num_t d = a[i] - b[i];
			result += d*d;
		}
		return sqrt(result);
	}
	static Num_t length(Num_t* x) {
		Num_t result = 0;
		for (int i = 0; i < dimension; i++) {
			Num_t d = x[i];
			result += d*d;
		}
		return sqrt(result);
	}
	void normalise() {
		for (int i = 0; i < n; i++) {
			Num_t len = length(points[i]);
			for (int j = 0; j < dimension; j++) {
				points[i][j] /= len;
			}
		}
	}
public:
	VectorVec<Num_t,dimension> points;
	size_t n;
	PointSpreader(size_t n) : n(n) {
		points.fill_zero(n);
		shadow_points.fill_zero(n);
		srand(static_cast<unsigned> (time(0)));
		assert(points.size() == n);
		for (size_t i = 0; i < n; i++) {
			randomiseVector<Num_t,dimension>(points[i]);
		}
		normalise();
	}
	~PointSpreader() {}
	bool iterate(Num_t repulsion_constant = 0.5) {
		memcpy(shadow_points.data(), points.data(), sizeof(Num_t)*dimension*n);
		for (int i = 0; i < n - 1; i++) {
			for (int j = i+1; j < n; j++) {
				Num_t d = dst(points[i], points[j]);
				if (d == 0)
					continue;
				Num_t repulsion = repulsion_constant / (d*d);
				for (int k = 0; k < dimension; k++) {
					Num_t repulsion_k = (points[i][k] - points[j][k]) * repulsion;
					shadow_points[i][k] += repulsion_k;
					shadow_points[j][k] -= repulsion_k;
				}
			}
		}
		memcpy(points.data(), shadow_points.data(), sizeof(Num_t)*dimension*n);
		normalise();
		return true;
	}
	Num_t mindst() const {
		Num_t mindst = 1;
		for (int i = 0; i < n; i++) {
			for (int j = 0; j < n; j++) {
				if (i == j)
					continue;
				Num_t d = dst(points[i], points[j]);
				if (d < mindst)
					mindst = d;
			}
		}
		return mindst;
	}
	Num_t mindst_diff() const {
		Num_t largest_mindst = 0;
		Num_t smallest_mindst = 1;
		for (int i = 0; i < n; i++) {
			Num_t mindst = 1;
			for (int j = 0; j < n; j++) {
				if (i == j)
					continue;
				Num_t d = dst(points[i], points[j]);
				if (d < mindst)
					mindst = d;
			}
			if (mindst < smallest_mindst)
				smallest_mindst = mindst;
			if (mindst > largest_mindst)
				largest_mindst = mindst;
		}
		return largest_mindst - smallest_mindst;
	}
	Num_t optimise(Num_t allowed_error) {
		Num_t step_size = 1.0;
		Num_t last_error = 1;
		size_t steps_made = 0;
		while (true) {
			for (int i = 0; i < 20; i++)
				iterate(step_size);
			Num_t error = mindst_diff();
			if (error >= last_error) {
				step_size /= 8.0;
			}
			if (error < allowed_error || step_size <= (allowed_error/16))
				return error;
			last_error = error;
			steps_made += 20;
		}
	}
	void jitter() {
		Num_t maxamount = mindst() / 2;
		Num_t randvec[dimension];
		for (int i = 0; i < n; i++) {
			randomiseVector<Num_t, dimension>(randvec);
			VectorVec<Num_t,dimension> vv;
			vv.push_back(points[i]);
			gramSchmidt<Num_t, dimension>(vv, randvec);
			Num_t amount = (Num_t)rand() * maxamount / RAND_MAX;
			vectorMulAdd<Num_t, dimension>(points[i], points[i], amount / vectorLength<Num_t, dimension>(randvec), randvec);
		}
		normalise();
	}
};