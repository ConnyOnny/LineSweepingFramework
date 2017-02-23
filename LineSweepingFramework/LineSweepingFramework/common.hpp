#pragma once

#include "CLConfig.hpp"
#include "JSONBuilder.hpp"
#include "VectorVec.hpp"
#include "typenames.hpp"
#include <assert.h>
#include <stdint.h>
#include <signal.h>

//#define LOADPHPOUT

struct GetUsefulVectorTypeReturnType {
	std::string cl_type_name;
	size_t arity;
	size_t single_element_size;
	std::string to_string() const {
		if (arity == 1) {
			return cl_type_name;
		}
		else {
			std::stringstream ss;
			ss << cl_type_name << '[' << arity << ']';
			return ss.str();
		}
	}
};
template <typename Num_t, size_t dimension> GetUsefulVectorTypeReturnType getUsefulVectorType() {
	std::stringstream rets;
	size_t retn = 1;
	size_t ssize;
	rets << getTypeName<Num_t>();
	switch (dimension) {
	case 0:
	case 1:
		assert(false && "This framework is not built for zero- and one-dimensional line sweeping.");
		raise(SIGABRT);
		break; // unreachable
	case 2:
		rets << '2';
		ssize = 2 * sizeof(Num_t);
		break;
	case 4:
		rets << '4';
		ssize = 4 * sizeof(Num_t);
		break;
	case 8:
		rets << '8';
		ssize = 8 * sizeof(Num_t);
		break;
	case 16:
		rets << '16';
		ssize = 16 * sizeof(Num_t);
		break;
	default:
		retn = dimension;
		ssize = sizeof(Num_t);
	}
	GetUsefulVectorTypeReturnType ret;
	ret.arity = retn;
	ret.single_element_size = ssize;
	ret.cl_type_name = rets.str();
	return ret;
}

std::string exec(const char* cmd);
std::string dataFilePath(const char* fname = NULL);
void writeStringToFile(std::string what, std::string fname);
std::string readStringFromFile(std::string fname);

// phpFileName must be relative to the data directory
std::string executePhp(const char* phpFileName, std::string json = "{}", bool absolutePath = false);

// convenience function to compile a CL source file after passing it through php
cl::Program getProgram(cl::Context context, const char* phpFileName, std::string json = "{}", std::string buildOptions="", bool absolutePath = false);

void panic();

void write_buffer_to_file(void* data, size_t size, const char* fname);

void read_rgba8(cl_uchar4* data, int width, int height, const char* fname);
void write_rgba8(cl_uchar4* data, int pxcount, const char* fname);

void read_gray8(cl_uchar* data, int width, int height, const char* fname);
void write_gray8(cl_uchar* data, int pxcount, const char* fname);

void read_dpt(cl_float** data, uint32_t* width, uint32_t* height, const char* fname);

bool dimension_needs_array(size_t dimension);

template <typename Num_t, size_t dimension> Num_t vectorLength(Num_t const * vector) {
	Num_t accu = 0;
	for (int i = 0; i < dimension; i++) {
		accu += vector[i] * vector[i];
	}
	return sqrt(accu);
}

template <typename Num_t, size_t dimension> void normalise(Num_t *vector) {
	Num_t len = vectorLength<Num_t, dimension>(vector);
	assert(len != 0);
	for (int i = 0; i < dimension; i++) {
		vector[i] /= len;
	}
}

template <typename Num_t, size_t dimension> Num_t scalarProduct(Num_t const *a, Num_t const *b) {
	Num_t ret = 0;
	for (int i = 0; i < dimension; i++) {
		ret += a[i] * b[i];
	}
	return ret;
}

template <typename Num_t, size_t dimension> void vectorMinus(Num_t *dst, Num_t const *a, Num_t const *b) {
	for (int i = 0; i < dimension; i++) {
		dst[i] = a[i] - b[i];
	}
}

template <typename Num_t, size_t dimension> void vectorMulAdd(Num_t *dst, Num_t const *a, Num_t factor, Num_t const *b) {
	for (int i = 0; i < dimension; i++) {
		dst[i] = a[i] + factor * b[i];
	}
}

template <typename Num_t, size_t dimension> void scaleVector(Num_t *dst, Num_t factor, Num_t const *vec) {
	for (int i = 0; i < dimension; i++) {
		dst[i] = factor * vec[i];
	}
}

template <typename Num_t, size_t dimension> void gramSchmidt(VectorVec<Num_t, dimension> const & currentBase, Num_t *newVector) {
	normalise<Num_t, dimension>(newVector);
	for (int i = 0; i < currentBase.size(); i++) {
		Num_t* bvec = currentBase.at(i);
		Num_t sp = scalarProduct<Num_t, dimension>(newVector, bvec);
		for (int j = 0; j < dimension; j++) {
			newVector[j] -= sp * bvec[j];
		}
	}
}

template<typename Num_t, size_t dimension> void randomiseVector(Num_t* v) {
	bool allzero = true;
	for (int i = 0; i < dimension; i++) {
		v[i] = static_cast<Num_t>(rand()) / (RAND_MAX / 2) - 1;
		if (v[i] != 0)
			allzero = false;
	}
	if (allzero)
		randomiseVector<Num_t, dimension>(v);
}

template<typename Num_t> const char* precisionString();

enum LSOperationMode {
	SINGLE_PASS,
	TWO_PASS,
};