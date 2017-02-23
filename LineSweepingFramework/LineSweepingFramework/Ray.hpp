#pragma once

#include <math.h>

template <typename Num_t, size_t dimension> class Ray {
public:
	Num_t origin[dimension];
	Num_t direction[dimension];
	void walk(Num_t lambda) {
		for (int i=0; i<dimension; i++) {
			origin[i] += direction[i]*lambda;
		}
	}
	void accelerate(Num_t factor) {
		for (int i=0; i<dimension; i++) {
			direction[i] *= factor;
		}
	}
	void moveBy(const cl_float* vector) {
		for (int i=0; i<dimension; i++) {
			origin[i] += vector[i];
		}
	}
	Ray() {
		memset(origin, 0, sizeof(Num_t) * dimension);
		memset(direction, 0, sizeof(Num_t) * dimension);
	}
};
