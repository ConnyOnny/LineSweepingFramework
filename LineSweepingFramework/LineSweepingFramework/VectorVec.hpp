#pragma once

#include <algorithm>
#include <assert.h>

/**
 * This class exists only because a std::vector<Num_t[dimension]> is not allowed in all contexts in C++.
 */
template <typename Num_t, size_t dimension> class VectorVec
{
private:
	VectorVec(const VectorVec&);
protected:
	Num_t * _data;
	size_t len;
	size_t allocd;
public:
	VectorVec() {
		_data = NULL;
		allocd = 0;
		len = 0;
	}
	~VectorVec() {
		if (_data) {
			free(_data);
		}
		_data = 0;
		allocd = 0;
		len = 0;
	}
	void fill_zero(size_t elements) {
		reserve(elements);
		assert(allocd >= elements);
		len = elements;
		memset(_data, 0, len * dimension * sizeof(Num_t));
	}
	void reserve(size_t min_capacity) {
		size_t newsize = std::max<size_t>(std::max<size_t>(allocd * 2, 4), min_capacity);
		_data = (Num_t*)realloc(_data, newsize * dimension * sizeof(Num_t));
		assert(_data);
		allocd = newsize;
	}
	void push_back(const Num_t* v) {
		if (len == allocd) {
			reserve(len+1);
		}
		assert(len < allocd);
		memcpy(_data + (dimension*len), v, sizeof(Num_t)*dimension);
		len++;
	}
	size_t size() const {
		return len;
	}
	size_t capacity() const {
		return allocd;
	}
	Num_t* at(size_t i) const {
		assert(i < len);
		return _data + (i*dimension);
	}
	const Num_t* operator[](size_t i) const {
		return _data + (i*dimension);
	}
	Num_t* operator[](size_t i) {
		return _data + (i*dimension);
	}
	Num_t* data() const {
		return _data;
	}
	VectorVec& operator*=(const Num_t scalar) {
		for (int i = 0; i < len*dimension; i++) {
			_data[i] *= scalar;
		}
		return *this;
	}
};

