#pragma once

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>
#include "OpenCLUtilities/openCLUtilities.hpp"
#include "common.hpp"

struct CLConfig
{
public:
	cl::Context context;
	VECTOR_CLASS<cl::Device> devices;
	cl::CommandQueue queue;
	/**
	 * Use this if there is only one GPU available.
	 * Otherwise build the struct members manually.
	 * Using this in a system with multiple GPUs will print a warning to stderr.
	 */
	static CLConfig* default_config();
};

