#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include <vector>
#include <assert.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <sstream>
#include <signal.h>
#include "VectorVec.hpp"
#include "Ray.hpp"
#include "RTND.hpp"
#include "PointSpreader.hpp"
#include "CLStructDef.hpp"

class RayFactory {
private:
	RayFactory(){}
	~RayFactory(){}
public:
	struct RayData;
	struct RayDataGpu;
	struct SplitRayDataGpu;
	template <typename Num_t, size_t dimension> static CLStructDef::Builder * rayDataStructure(LSOperationMode mode, CLStructDef::Builder * builder = NULL);
	template <typename Num_t, size_t dimension> static CLStructDef::Builder * directionDataStructure(LSOperationMode mode, CLStructDef::Builder * builder = NULL);
	template <typename Num_t, size_t dimension, typename Hit_t> static RayData* default_rays(Hit_t interesting_space, const VectorVec<Num_t, dimension> &directions, Num_t grid_size, LSOperationMode mode,
		CLStructDef *ray_data_description = NULL, CLStructDef *direction_data_description = NULL);
};

struct RayFactory::RayData {
	CLStructDef::Vector rays;
	CLStructDef::Vector directions;
	// the following will be empty, respectively 0 in the single-pass mode
	std::vector<cl_uint> ray_lookup_table;
	size_t intermediate_storage_slots;
	size_t max_rays_per_dimension;
	RayData() {
		intermediate_storage_slots = 0;
		max_rays_per_dimension = 0;
	}
	RayData(std::string fname, CLStructDef* raydesc, CLStructDef* directiondesc) {
		std::ifstream in;
		in.open(fname, std::ios::binary);
		rays.definition = raydesc;
		directions.definition = directiondesc;
		if (in.fail()) {
			std::cerr << "Could not open file to read: " << fname << std::endl;
			panic();
		}
		size_t rays_size, directions_size, rlt_elements;
#define R(x) do { in.read((char*)&(x), sizeof(size_t)); assert(!in.fail()); } while(0)
		R(rays_size);
		R(directions_size);
		R(rlt_elements);
		R(intermediate_storage_slots);
		R(max_rays_per_dimension);
#undef R
		assert(rays_size % raydesc->byte_size() == 0);
		assert(directions_size % directiondesc->byte_size() == 0);
		assert(sizeof(char) == 1);
		rays.data.resize(rays_size, 0);
		in.read((char*)rays.raw_pointer(), rays_size);
		assert(!in.fail());
		directions.data.resize(directions_size, 0);
		in.read((char*)directions.raw_pointer(), directions_size);
		assert(!in.fail());
		if (rlt_elements > 0) {
			ray_lookup_table.resize(rlt_elements, 0);
			in.read((char*)ray_lookup_table.data(), rlt_elements * sizeof(cl_uint));
			assert(!in.fail());
		}
		in.close();
	}
	void save_to_file(std::string fname) {
		std::ofstream out;
		out.open(fname, std::ios::binary);
		if (out.fail()) {
			std::cerr << "Could not open file to write: " << fname << std::endl;
			panic();
		}
		size_t tmp;
#define W out.write((char*)&tmp, sizeof(tmp))
		tmp = rays.byte_size();
		W;
		tmp = directions.byte_size();
		W;
		tmp = ray_lookup_table.size();
		W;
		tmp = intermediate_storage_slots;
		W;
		tmp = max_rays_per_dimension;
		W;
#undef W
		out.write((char*)rays.raw_pointer(), rays.byte_size());
		out.write((char*)directions.raw_pointer(), directions.byte_size());
		out.write((char*)ray_lookup_table.data(), ray_lookup_table.size() * sizeof(cl_uint));
		out.close();
	}
	LSOperationMode mode() const {
		if (intermediate_storage_slots > 0) {
			return TWO_PASS;
		}
		else {
			return SINGLE_PASS;
		}
	}
};

struct RayFactory::RayDataGpu {
	cl::Buffer rays_gpu;
	cl::Buffer directions_gpu;
	cl::Buffer ray_lookup_table_gpu;
	cl_uint intermediate_storage_slots;
	cl_uint rays_size;
	const LSOperationMode _mode;
	virtual LSOperationMode mode() const {
		return _mode;
	}
	RayDataGpu(const RayData& data, const cl::Context& context) : _mode(data.mode()) {
		rays_gpu = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, data.rays.byte_size(), data.rays.raw_pointer());
		directions_gpu = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, data.directions.byte_size(), data.directions.raw_pointer());
		if (data.mode() == TWO_PASS) {
			ray_lookup_table_gpu = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, data.ray_lookup_table.size() * sizeof(cl_uint), (void*)data.ray_lookup_table.data());
			intermediate_storage_slots = data.intermediate_storage_slots;
		}
		else {
			intermediate_storage_slots = 0;
		}
		rays_size = data.rays.size();
	}
	/*
	* Sets the required parameters: ray and direction descriptors, and if in TWO_PASS mode also the ray lookup table
	* returns the arg_offset where the next argument after this should be placed.
	*/
	virtual cl_uint set_constant_sweep_parameters(cl_uint arg_offset, cl::Kernel kernel) const {
		kernel.setArg<cl::Buffer>(arg_offset++, rays_gpu);
		kernel.setArg<cl::Buffer>(arg_offset++, directions_gpu);
		if (mode() == TWO_PASS) {
			kernel.setArg<cl::Buffer>(arg_offset++, ray_lookup_table_gpu);
		}
		return arg_offset;
	}
	virtual cl_uint2 get_ray_interval(int iteration) const {
		assert(iteration == 0 && "If you want to iterate, please use SplitRayDataGpu");
		cl_uint2 ret;
		ret.x = 0;
		ret.y = rays_size;
		return ret;
	}
	virtual cl_uint set_variable_sweep_parameters(cl_uint arg_offset, cl::Kernel kernel, int iteration) const {
		cl_uint2 ray_interval = get_ray_interval(iteration);
		kernel.setArg<cl_uint2>(arg_offset++, ray_interval);
		return arg_offset;
	}
};

struct RayFactory::SplitRayDataGpu : RayFactory::RayDataGpu {
	virtual LSOperationMode mode() const {
		return TWO_PASS;
	}
	std::vector<cl_uint> split_indices_rays;
	SplitRayDataGpu(const RayData& data, const cl::Context& context, size_t parts) : RayFactory::RayDataGpu(data, context) {
		assert(data.mode() == TWO_PASS && "Splitting is only useful in TWO_PASS mode");
		assert(parts <= data.directions.size());
		size_t part_size = data.directions.size() / parts + ((data.directions.size() % parts) != 0 ? 1 : 0);
		size_t ray_did_offs = data.rays.definition->getMemberOffset("direction_idx");
		split_indices_rays.push_back(0); // first section alsways starts at 0
		for (int part = 1; part < parts; part++) {
			size_t did_start = part*part_size;
			for (cl_uint ray_idx = split_indices_rays.back();; ray_idx++) {
				cl_uint did = *((cl_uint*)(data.rays.definition->getPtr(data.rays.raw_pointer(), ray_did_offs, ray_idx)));
				if (did == did_start) {
					split_indices_rays.push_back(ray_idx);
					break;
				}
			}
		}
		split_indices_rays.push_back(data.rays.size());
		assert(split_indices_rays.size() == parts + 1);
	}
	virtual cl_uint2 get_ray_interval(int iteration) const {
		assert(iteration < parts());
		assert(iteration >= 0);
		cl_uint2 ret;
		ret.x = split_indices_rays[iteration];
		ret.y = split_indices_rays[iteration+1];
		return ret;
	}
	int parts() const {
		return split_indices_rays.size() - 1;
	}
};

template <typename Num_t, size_t dimension> struct RayBaseData {
	Num_t origin[dimension];
	cl_uint direction_idx;
	cl_int steps;
	cl_uint memory_begin; // only used in two-pass mode
};

template<size_t dimension> size_t calc_ray_lookup_table_size(size_t directions, size_t max_rays_per_dimension) {
	size_t per_side = 1;
	for (size_t i = 0; i < dimension - 1; i++) {
		per_side *= max_rays_per_dimension;
	}
	return directions * per_side;
}

template<typename int_t> int_t intpow(int_t base, int_t exponent) {
	int_t result = 1;
	for (int_t i = 0; i < exponent; i++) {
		result *= base;
	}
	return result;
}

template<size_t dimension> size_t calc_ray_lookup_table_idx(int direction_idx, int* loop_indices, int max_rays_per_dimension) {
	size_t ret = direction_idx * intpow<size_t>(max_rays_per_dimension, dimension - 1);
	size_t mul_accu = 1;
	for (int i = 0; i < dimension-1; i++) {
		int this_idx = loop_indices[i] + (max_rays_per_dimension / 2); // make positive
		assert(this_idx >= 0);
		ret += this_idx * mul_accu;
		mul_accu *= max_rays_per_dimension;
	}
	return ret;
}

template <typename Num_t, size_t dimension> static CLStructDef::Builder * RayFactory::rayDataStructure(LSOperationMode mode, CLStructDef::Builder * builder) {
	CLStructDef::Builder *ret;
	if (builder) {
		ret = builder;
	}
	else {
		ret = new CLStructDef::Builder("RayDescriptor");
	}
	assert(ret->getStructName() == "RayDescriptor" && "The struct for the ray descriptors must be named 'RayDescriptor'");
	GetUsefulVectorTypeReturnType vt = getUsefulVectorType<Num_t, dimension>();
	ret->manualAddMember("origin", vt.cl_type_name.c_str(), vt.single_element_size, vt.arity)
		->addMember<cl_int>("num_steps")
		->addMember<cl_uint>("direction_idx");
	if (mode == TWO_PASS) {
		ret->addMember<cl_uint>("memory_begin");
	}
	return ret;
}
template <typename Num_t, size_t dimension> static CLStructDef::Builder * RayFactory::directionDataStructure(LSOperationMode mode, CLStructDef::Builder * builder) {
	CLStructDef::Builder *ret;
	if (builder) {
		ret = builder;
	}
	else {
		ret = new CLStructDef::Builder("DirectionDescriptor");
	}
	assert(ret->getStructName() == "DirectionDescriptor" && "The struct for the direction descriptors must be named 'DirectionDescriptor'");
	GetUsefulVectorTypeReturnType vt = getUsefulVectorType<Num_t, dimension>();
	ret->manualAddMember("step_vec", vt.cl_type_name.c_str(), vt.single_element_size, vt.arity);
	if (mode == TWO_PASS) {
		// need span vectors to project on
		for (int i = 0; i < dimension - 1; i++) {
			// dimension-1 because step_vec is the missing vector for the orthogonal base
			std::stringstream vname;
			vname << "span_vec_" << i;
			ret->manualAddMember(vname.str().c_str(), vt.cl_type_name.c_str(), vt.single_element_size, vt.arity);
		}
	}
	return ret;
}

template <typename Num_t, size_t dimension, typename Hit_t> RayFactory::RayData* RayFactory::default_rays(Hit_t interesting_space, const VectorVec<Num_t, dimension> &directions, Num_t grid_size, LSOperationMode mode,
	CLStructDef *ray_data_description, CLStructDef *direction_data_description) {
	RayData *ret = new RayData();
	if (!ray_data_description) {
		ray_data_description = rayDataStructure<Num_t, dimension>(mode)->finalize();
	}
	if (!direction_data_description) {
		direction_data_description = directionDataStructure<Num_t, dimension>(mode)->finalize();
	}
	size_t num_directions = directions.size();
	ret->rays.definition = ray_data_description;
	ret->directions.definition = direction_data_description;
	ret->directions.fill_zero(directions.size());
	cl_uint ray_out_memory_ctr = 0;
	// verify RayDescriptor
	assert(ray_data_description->getStructName() == "RayDescriptor");
	assert(ray_data_description->getSizeByName("origin") >= sizeof(Num_t)*dimension && "storage space for 'origin' in RayDescriptor too small");
	assert(ray_data_description->getSizeByName("num_steps") == sizeof(cl_int) && "num_steps must be an int in RayDescriptor");
	assert(ray_data_description->getSizeByName("direction_idx") == sizeof(cl_uint) && "direction_idx must be a uint in RayDescriptor");
	assert(mode == SINGLE_PASS || ray_data_description->getSizeByName("memory_begin") == sizeof(cl_uint) && "memory_begin must be a uint in RayDescriptor when using the two-pass mode");
	// verify DirectionDescriptor
	assert(direction_data_description->getStructName() == "DirectionDescriptor");
	assert(direction_data_description->getSizeByName("step_vec") >= sizeof(Num_t)*dimension && "storage space for 'step_vec' in DirectionDescriptor too small");
	int max_rays_per_dimension = (int)roundf((Num_t)interesting_space.bounding_diameter() / grid_size);
	size_t ray_lookup_table_size = 0;
	if (mode == TWO_PASS) {
		ray_lookup_table_size = calc_ray_lookup_table_size<dimension>(num_directions, max_rays_per_dimension);
		ret->ray_lookup_table.resize(ray_lookup_table_size, 0);
		ret->max_rays_per_dimension = max_rays_per_dimension;
		for (int i = 0; i < dimension-1; i++) {
			// dimension-1 because step_vec is the missing vector for the orthogonal base
			std::stringstream vname;
			vname << "span_vec_" << i;
			assert(direction_data_description->getSizeByName(vname.str().c_str()) >= sizeof(Num_t)*dimension && "storage space for a span_vec in DirectionDescriptor too small");
		}
	}
	else {
		assert(mode == SINGLE_PASS);
	}
	// *out_direction_data = malloc((*out_direction_data_description)->byte_size() * num_directions); // PERF reserve space
	std::fstream logf;
	logf.open(dataFilePath("generated_directions.svg"), std::ios_base::out | std::ios_base::trunc);
	logf << "<svg>" << std::endl;
	for (int d_idx = 0; d_idx < num_directions; d_idx++) {
		std::cout << "generating rays: direction " << d_idx << '/' << num_directions << std::endl;
		const Num_t *ray_direction = directions[d_idx];
		logf << "<path style=\"stroke:#000000;stroke-width:0.01px\" d=\"M 0,0 ";
		for (int j = 0; j < dimension; j++) {
			logf << ray_direction[j];
			if (j < dimension - 1)
				logf << ',';
		}
		logf << "\" />" << std::endl;
		VectorVec<Num_t, dimension> base;
		base.push_back(ray_direction);
		while (base.size() < dimension) {
			Num_t v[dimension];
			randomiseVector<Num_t, dimension>(v);
			gramSchmidt<Num_t, dimension>(base, v);
			normalise<Num_t, dimension>(v);
			base.push_back(v);
		}
		// now we have a orthonormal basis of the problem space where the first vector is our marching direction
		// save the vectors into out_direction_data, except for single pass, then we only need the step_vec
		for (int basevec_idx = 0; basevec_idx < (mode == TWO_PASS ? base.size() : 1); basevec_idx++) {
			std::stringstream vname;
			if (basevec_idx == 0) {
				vname << "step_vec";
			}
			else {
				vname << "span_vec_" << basevec_idx - 1;
			}
			size_t member_offs = direction_data_description->getMemberOffset(vname.str().c_str());
			Num_t v[dimension];
			memcpy(v, base.at(basevec_idx), sizeof(Num_t)*dimension);
			for (int i = 0; i < dimension; i++) {
				v[i] *= grid_size;
			}
			memcpy(direction_data_description->getPtr(ret->directions.at(d_idx), member_offs), v, sizeof(Num_t)*dimension);
		}
		int loop_indices[dimension - 1];
		for (int i = 0; i < dimension - 1; i++) {
			loop_indices[i] = -max_rays_per_dimension / 2;
		}
		RayBaseData<Num_t, dimension> ray;
		ray.direction_idx = d_idx;
		std::stringstream dbg;
		dbg << "<svg>" << std::endl;
		while (loop_indices[0] <= max_rays_per_dimension / 2) {
			for (int i = 0; i < dimension; i++) {
				ray.origin[i] = 0;
				for (int j = 0; j < dimension - 1; j++) {
					ray.origin[i] += grid_size * base.at(j + 1)[i] * loop_indices[j];
				}
			}
			size_t stepvecoffs = direction_data_description->getMemberOffset("step_vec");
			Num_t lambda_entry, lambda_exit;
			assert(fabs(vectorLength<Num_t, dimension>(ray_direction)-1) < 0.01); // ray_direction is normalised
			interesting_space.intersect(ray.origin, ray_direction, &lambda_entry, &lambda_exit);
			if (!isnan(lambda_entry)) {
				// to align the rays to each other, make them all have a stop at 0 (in step direction)
				lambda_entry = ceil(lambda_entry / grid_size) * grid_size;
				Num_t p_entry[dimension];
				vectorMulAdd<Num_t, dimension>(p_entry, ray.origin, lambda_entry, ray_direction);
				if (!isnan(lambda_exit)) {
					Num_t interesting_length = lambda_exit - lambda_entry;
					ray.steps = std::max<cl_int>((cl_int)floorf(interesting_length / grid_size),0);
				}
				else {
					// it is uncertain how many steps will be needed
					ray.steps = -1;
				}
				if (ray.steps == -1 || ray.steps > 1) {
					// rays with only one step are not interesting => don't send them to the gpu
					memcpy(ray.origin, p_entry, sizeof(Num_t)*dimension);
					size_t ray_idx = ret->rays.size();
					if (mode == TWO_PASS) {
						assert(ray.steps != -1 && "uncertain ray lengths can only be used in single-pass mode");
						size_t idx = calc_ray_lookup_table_idx<dimension>(d_idx, loop_indices, max_rays_per_dimension);
						assert(idx < ray_lookup_table_size);
						ray.memory_begin = ray_out_memory_ctr;
						ray_out_memory_ctr += ray.steps;
						ret->ray_lookup_table[idx] = (cl_uint)(ray_idx+1); // is one more than the actual index because 0 means no ray at all
					}
					void * clray = malloc(ray_data_description->byte_size());
					size_t ray_origin_offs = ray_data_description->getMemberOffset("origin");
					memcpy(ray_data_description->getPtr(clray, ray_origin_offs), ray.origin, sizeof(Num_t)*dimension);
					size_t ray_num_steps_offs = ray_data_description->getMemberOffset("num_steps");
					memcpy(ray_data_description->getPtr(clray, ray_num_steps_offs), &ray.steps, sizeof(cl_int));
					size_t ray_direction_idx_offs = ray_data_description->getMemberOffset("direction_idx");
					memcpy(ray_data_description->getPtr(clray, ray_direction_idx_offs), &ray.direction_idx, sizeof(cl_uint));
					if (mode == TWO_PASS) {
						size_t ray_memory_begin_offs = ray_data_description->getMemberOffset("memory_begin");
						memcpy(ray_data_description->getPtr(clray, ray_memory_begin_offs), &ray.memory_begin, sizeof(cl_uint));
					}
					ret->rays.push_back(clray);
					if (d_idx == 0) {
						dbg << "<circle cx=\"" << ray.origin[1] << "\" cy=\"" << ray.origin[2] << "\" r=\"0.02\" />" << std::endl;
					}
				}
			}
			// iterate indices
			for (int i = dimension - 2; i >= 0; i--) {
				if (++loop_indices[i] <= max_rays_per_dimension / 2) {
					break;
				}
				else {
					if (i > 0) { // for the last index there is no place to overflow. The loop is then over
						loop_indices[i] = -max_rays_per_dimension / 2;
					}
				}
			}
		}
		dbg << "</svg>" << std::endl;
		writeStringToFile(dbg.str(), dataFilePath("startpos.svg"));
	}
	if (mode == TWO_PASS) {
		ret->intermediate_storage_slots = ray_out_memory_ctr;
	}
	logf << "</svg>" << std::endl;
	logf.close();
	return ret;
}

