#pragma once

#include <math.h>

template <typename Num_t, size_t dimension> class RectangleND {
private:
	Num_t dimensions[dimension];
	void calcLambdas(Num_t origin, Num_t target, Num_t speed, Num_t* out_entry, Num_t* out_exit);
public:
	RectangleND(Num_t const * dimensions);
	void intersect(const Num_t* origin, const Num_t* direction, Num_t* out_lambda_entry, Num_t* out_lambda_exit);
	Num_t bounding_diameter();
};

template <typename Num_t> Num_t calcLambda(Num_t origin, Num_t target, Num_t speed) {
	if (speed == 0) {
		if (target >= origin)
			return INFINITY;
		else
			return -INFINITY;
	}
	return (target - origin) / speed;
}

template <typename Num_t, size_t dimension> void RectangleND<Num_t,dimension>::calcLambdas(Num_t origin, Num_t target, Num_t speed, Num_t* out_entry, Num_t* out_exit) {
	Num_t l1 = calcLambda(origin,  target, speed);
	Num_t l2 = calcLambda(origin, -target, speed);
	if (l1 < l2) {
		*out_entry = l1;
		*out_exit = l2;
	} else {
		*out_entry = l2;
		*out_exit = l1;
	}
}


template <typename Num_t, size_t dimension> void RectangleND<Num_t, dimension>::intersect(const Num_t* origin, const Num_t* direction, Num_t* out_lambda_entry, Num_t* out_lambda_exit) {
	Num_t max_entry = -INFINITY, min_exit = INFINITY, l_entry, l_exit;
	for (int i=0; i<dimension; i++) {
	    calcLambdas(origin[i], dimensions[i]/2, direction[i], &l_entry, &l_exit);
	    if (l_entry > max_entry) {
	    	max_entry = l_entry;
	    }
	    if (l_exit < min_exit) {
	    	min_exit = l_exit;
	    }
	}
	if (min_exit < max_entry) {
		// We exit in some dimension before all dimensions are even in.
		// => no hit
		*out_lambda_entry = NAN;
		*out_lambda_exit = NAN;
	}
	else {
		// the entry line which is hit last
		*out_lambda_entry = max_entry;
		// the exit line which is hit first
		*out_lambda_exit = min_exit;
	}
}

template <typename Num_t, size_t dimension> RectangleND<Num_t, dimension>::RectangleND(Num_t const* dimensions) {
	memcpy(this->dimensions, dimensions, sizeof(Num_t)*dimension);
}

template <typename Num_t, size_t dimension> Num_t RectangleND<Num_t, dimension>::bounding_diameter() {
	// calculate the length of the room diagonal
	Num_t accu = 0;
	for (int i=0; i<dimension; i++) {
		accu += dimensions[i] * dimensions[i];
	}
	return sqrt(accu);
}
