#pragma once

#include "CLConfig.hpp"
#include "JSONBuilder.hpp"
#include "RayFactory.hpp"

template <typename Num_t, size_t dimension>
class LSProgram
{
protected:
	LSOperationMode _mode;
	LSOperationMode mode() const;
public:
	cl::Program program;
	LSProgram(const CLConfig&, const RayFactory::RayData&, const std::string & program_file_name, JSONBuilder* phpargs = NULL, std::string build_options = "");
	cl::Kernel get_kernel(const char* name, cl_int* err = 0);
	~LSProgram();
};

template <typename Num_t, size_t dimension>
LSOperationMode LSProgram<Num_t, dimension>::mode() const {
	return _mode;
}

template <typename Num_t, size_t dimension>
LSProgram<Num_t, dimension>::LSProgram(const CLConfig& clconfig, const RayFactory::RayData& ray_data, const std::string & program_file_name, JSONBuilder* phpargs, std::string build_options) {
	_mode = ray_data.mode();
	JSONBuilder backupjson; // this will be destructed at the end of this function
	if (phpargs == NULL) {
		phpargs = &backupjson;
	}
	auto vt = getUsefulVectorType<Num_t, dimension>();
	phpargs->add("dimension", (int)dimension)
		->add("VT", vt.to_string())
		->add("RayDescriptor", ray_data.rays.definition->generateCLCode())
		->add("DirectionDescriptor", ray_data.directions.definition->generateCLCode())
		->add("two_pass", mode() == TWO_PASS)
		->add("sweep_directions", (long)(ray_data.directions.size()))
		->add("max_rays_per_dimension", (long)(ray_data.max_rays_per_dimension));
	this->program = cl::Program(getProgram(clconfig.context, program_file_name.c_str(), phpargs->done(), build_options, true));
}

template <typename Num_t, size_t dimension>
cl::Kernel LSProgram<Num_t, dimension>::get_kernel(const char* name, cl_int* err) {
	cl::Kernel ret(this->program, name, err);
	return ret;
}

template <typename Num_t, size_t dimension>
LSProgram<Num_t,dimension>::~LSProgram() {
}