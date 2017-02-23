//#define _USE_MATH_DEFINES
//#include <math.h>
#include <iostream>
#include <cstring>
#include <assert.h>
#include <chrono>
#include "PointSpreader.hpp"
#include "RTND.hpp"
#include "CLConfig.hpp"
#include "LSProgram.hpp"

int lsdo() {
	const char* imgname = "C:\\linesweeping\\sintel_depth_source\\MPI-Sintel-depth-training-20150305\\training\\depth\\temple_2\\frame_0041.dpt";
	const char* outname = "C:\\linesweeping\\lsdoout.rgba";
	const char* horizon_filename = "C:\\linesweeping\\sky_colorful_low.rgba";
	uint32_t horizon_angles_around = 1024;
	uint32_t horizon_angles_up = 256;
	uint32_t imgwidth;
	uint32_t imgheight;
	cl_float *imgdata_cpu;
	cl_uchar4 *horizon_cpu = new cl_uchar4[horizon_angles_around * horizon_angles_up];
	read_dpt(&imgdata_cpu, &imgwidth, &imgheight, imgname);
	read_rgba8(horizon_cpu, horizon_angles_around, horizon_angles_up, horizon_filename);
	const int imgarea = imgwidth * imgheight;
	cl_uchar4 *outdata_cpu = new cl_uchar4[imgarea];
	const int direction_count = 8;
	const cl_float grid_size = 1.8;
	PointSpreader<cl_float, 2> direction_finder(direction_count);
	direction_finder.optimise(0.001f);
	direction_finder.jitter();
	cl_float problem_space_dimensions[] = { imgwidth, imgheight };
	RectangleND<cl_float, 2> problem_space(problem_space_dimensions);
	RayFactory::RayData *ray_data = RayFactory::default_rays<cl_float, 2>(problem_space, direction_finder.points, grid_size, TWO_PASS);
	CLConfig *clconfig = CLConfig::default_config();
	RayFactory::SplitRayDataGpu ray_data_gpu = RayFactory::SplitRayDataGpu(*ray_data, clconfig->context, 4);
	JSONBuilder constants;
	constants.add("image_width", (long)imgwidth)
		->add("image_height", (long)imgheight)
		->add("horizon_angles_around", (long)horizon_angles_around)
		->add("horizon_angles_up", (long)horizon_angles_up)
		->add("hull_buffer_size", 256)
		->add("falloff_radius", 60);
	LSProgram<cl_float, 2> lsprogram(*clconfig, *ray_data, dataFilePath("lsdo.cl.php"), &constants);
	cl::Kernel lsdo_kernel = lsprogram.get_kernel("lsdo");
	size_t input_image_buffer_size = imgarea * sizeof(cl_float);
	size_t output_image_buffer_size = imgarea * sizeof(cl_uchar4);
	cl::Buffer imgdata_gpu = cl::Buffer(clconfig->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, input_image_buffer_size, imgdata_cpu);
	cl::Buffer horizon_gpu = cl::Buffer(clconfig->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, horizon_angles_around * horizon_angles_up * sizeof(cl_uchar4), horizon_cpu);
	size_t intermediate_storage_byte_size = ray_data_gpu.intermediate_storage_slots * sizeof(cl_uchar4);
	cl::Buffer intermediate_storage_gpu = cl::Buffer(clconfig->context, CL_MEM_READ_WRITE, intermediate_storage_byte_size);
	cl_uint argctr = 0;
	argctr = ray_data_gpu.set_constant_sweep_parameters(argctr, lsdo_kernel);
	lsdo_kernel.setArg<cl::Buffer>(argctr++, intermediate_storage_gpu);
	lsdo_kernel.setArg<cl::Buffer>(argctr++, imgdata_gpu);
	lsdo_kernel.setArg<cl::Buffer>(argctr++, horizon_gpu);
	const cl_uint lsdo_ray_interval_arg_index = argctr;
	cl::Buffer outimg_gpu = cl::Buffer(clconfig->context, CL_MEM_READ_WRITE, output_image_buffer_size);
	clconfig->queue.enqueueFillBuffer<cl_uint>(outimg_gpu, 0, 0, output_image_buffer_size);
	cl::Kernel gather_results_kernel = lsprogram.get_kernel("gather_results");
	argctr = 0;
	argctr = ray_data_gpu.set_constant_sweep_parameters(argctr, gather_results_kernel);
	gather_results_kernel.setArg<cl::Buffer>(argctr++, intermediate_storage_gpu);
	gather_results_kernel.setArg<cl::Buffer>(argctr++, outimg_gpu);
	const cl_uint grk_ray_interval_arg_index = argctr;
	clconfig->queue.finish();
	auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < ray_data_gpu.parts(); i++) {
		ray_data_gpu.set_variable_sweep_parameters(lsdo_ray_interval_arg_index, lsdo_kernel, i);
		clconfig->queue.enqueueFillBuffer<cl_uint>(intermediate_storage_gpu, 0, 0, intermediate_storage_byte_size); // PERF needed?
		//clconfig->queue.finish();
		cl_uint2 rays_this_time = ray_data_gpu.get_ray_interval(i);
		clconfig->queue.enqueueNDRangeKernel(lsdo_kernel, cl::NullRange, cl::NDRange(rays_this_time.s1-rays_this_time.s0));
		//clconfig->queue.finish();

		ray_data_gpu.set_variable_sweep_parameters(grk_ray_interval_arg_index, gather_results_kernel, i);
		clconfig->queue.enqueueNDRangeKernel(gather_results_kernel, cl::NullRange, cl::NDRange(imgwidth, imgheight));
		//clconfig->queue.finish();
	}
	clconfig->queue.finish();
	auto end = std::chrono::steady_clock::now();
	std::cout << "time needed (micro seconds): " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << std::endl;
	clconfig->queue.enqueueReadBuffer(outimg_gpu, true, 0, output_image_buffer_size, outdata_cpu);
	clconfig->queue.finish();
	write_rgba8(outdata_cpu, imgarea, outname);
	std::cin.get();
	// memset the intermediate storage to 0; PERF may be superfluous
	
	/*
	// to debug the intermediate storage
	cl_uchar *intermediate = new cl_uchar[framework.getIntermediateStorageSize() * sizeof(cl_uchar)];
	framework.queue.enqueueReadBuffer(*framework.getIntermediateStorageBuffer(), true, 0, framework.getIntermediateStorageSize() * sizeof(cl_uchar), intermediate);
	write_gray8(intermediate, framework.getIntermediateStorageSize(), "C:\\linesweeping\\intermediate.gray");
	*/
	return 0;
}

template<typename Num_t>
Num_t* spaced_octahedron(Num_t space, Num_t scale=1) {
	Num_t *out = new Num_t[8 * 3 * 3];
	space = space / sqrt(3); // because the `directions` are not normalized
	Num_t directions[] = {
		 1, 1, 1,
		 1, 1,-1,
		 1,-1, 1,
		 1,-1,-1,
		-1, 1, 1,
		-1, 1,-1,
		-1,-1, 1,
		-1,-1,-1,
	};
	assert(sizeof(directions) == 8 * 3 * sizeof(Num_t));
	for (int i = 0; i < sizeof(directions) / sizeof(Num_t); i++) {
		directions[i] *= scale;
	}
	int ctr = 0;
	// push point: PP
#define PP(a,b,c) do{out[ctr++]=(a);out[ctr++]=(b);out[ctr++]=(c);}while(0);
	for (int direction = 0; direction < 8; direction++) {
		assert(direction * 3 * 3 == ctr);
		PP(directions[direction * 3 + 0], space*directions[direction * 3 + 1], space*directions[direction * 3 + 2]);
		PP(space*directions[direction * 3 + 0], directions[direction * 3 + 1], space*directions[direction * 3 + 2]);
		PP(space*directions[direction * 3 + 0], space*directions[direction * 3 + 1], directions[direction * 3 + 2]);
	}
#undef PP
	assert(ctr == 8 * 3 * 3);
	return out;
}

#define TRIANGLES_IN_BOX_WITH_SPACE (14*2)
template<typename Num_t>
Num_t* box_with_space() {
	Num_t *out = new Num_t[TRIANGLES_IN_BOX_WITH_SPACE * 3 * 3];
	int ctr = 0;
	// push point: PP
#define PP(a,b,c) do{out[ctr++]=(a);out[ctr++]=(b);out[ctr++]=(c);}while(0);
#define W(a,b,c,d,e,f,g,h,i,j,k,l) do{PP(a,b,c);PP(d,e,f);PP(g,h,i);PP(g,h,i);PP(j,k,l);PP(a,b,c);}while(0);
	const Num_t l = 0.8;
	const Num_t d = l / 3;
	// wall at -l,x,x
	W(-l, -l, -l, -l, -l, l, -l, l, l, -l, l, -l);
	// wall at x,-l,x
	PP(-l, -l, -l);
	PP(-l, -l, l);
	PP(l, -l, -l);
	PP(l, -l, l);
	PP(-l, -l, l);
	PP(l, -l, -l);
	// wall at x,+l,x
	PP(-l, l, -l);
	PP(-l, l, l);
	PP(l, l, -l);
	PP(l, l, l);
	PP(-l, l, l);
	PP(l, l, -l);
	// wall at x,x,-l
	PP(-l, -l, -l);
	PP(-l, l, -l);
	PP(l, -l, -l);
	PP(l, l, -l);
	PP(-l, l, -l);
	PP(l, -l, -l);
	// wall at x,x,l
	PP(-l, -l, l);
	PP(-l, l, l);
	PP(l, -l, l);
	PP(l, l, l);
	PP(-l, l, l);
	PP(l, -l, l);
	/* front wall:
	ABC
	A C
	ADC
	*/
	// wall A
	W(l, -l, -l, l, -l, l, l, -d, l, l, -d, -l);
	// wall C
	W(l, l, -l, l, l, l, l, d, l, l, d, -l);
	// wall B
	W(l, -d, l, l, d, l, l, d, d, l, -d, d);
	// wall D
	W(l, -d, -l, l, d, -l, l, d, -d, l, -d, -d);
	// inner left wall
	W(l, -d, -d, l, -d, d, -d, -d, d, -d, -d, -d);
	// inner right wall
	W(l, d, -d, l, d, d, -d, d, d, -d, d, -d);
	// inner upper and lower wall
	W(l, -d, -d, l, d, -d, -d, d, -d, -d, -d, -d);
	W(l, -d, d, l, d, d, -d, d, d, -d, -d, d);
	// inner back wall
	const Num_t x = 0;
	W(-d - x, -d, -d, -d - x, -d, d, -d - x, d, d, -d - x, d, -d);
#undef W
#undef PP
	assert(ctr == TRIANGLES_IN_BOX_WITH_SPACE * 3 * 3);
	return out;
}

int inside() {
	bool load_rays = false;
	bool store_rays = true;
	const cl_double spacing = 1.0/32;
	const cl_double* triangles = spaced_octahedron<cl_double>(spacing, 0.8);//box_with_space<cl_double>();//
	const cl_uint num_triangles = 8;// TRIANGLES_IN_BOX_WITH_SPACE;
	const cl_uint triangle_buffer_size = num_triangles * 3 * 3 * sizeof(cl_double);
	/* //show triangles
	for (int i = 0; i < num_triangles; i++) {
		std::cout << "triangle:" << std::endl;
		std::cout << " " << triangles[i * 3 * 3 + 0] << " " << triangles[i * 3 * 3 + 1] << " " << triangles[i * 3 * 3 + 2] << std::endl;
		std::cout << " " << triangles[i * 3 * 3 + 3] << " " << triangles[i * 3 * 3 + 4] << " " << triangles[i * 3 * 3 + 5] << std::endl;
		std::cout << " " << triangles[i * 3 * 3 + 6] << " " << triangles[i * 3 * 3 + 7] << " " << triangles[i * 3 * 3 + 8] << std::endl;
	}
	std::cin.get();*/
	const int direction_count = 64;
	const size_t cube_size = 256;
	const size_t cube_buffer_size = cube_size*cube_size*cube_size*sizeof(cl_int);
	PointSpreader<cl_double, 3> direction_finder(direction_count);
	direction_finder.optimise(0.001f);
	direction_finder.jitter();
	/*direction_finder.points.at(0)[0] = 1;
	direction_finder.points.at(0)[1] = 0;
	direction_finder.points.at(0)[2] = 0;*/
	const cl_double problem_diameter = 2;
	const cl_double problem_space_dimensions[] = { problem_diameter, problem_diameter, problem_diameter };
	const RectangleND<cl_double, 3> problem_space(problem_space_dimensions);
	const cl_double grid_size = problem_diameter / cube_size / sqrt(2);
	RayFactory::RayData *ray_data;
	if (load_rays) {
		ray_data = new RayFactory::RayData(dataFilePath("raydata.bin"), RayFactory::rayDataStructure<cl_double, 3>(SINGLE_PASS)->finalize(), RayFactory::directionDataStructure<cl_double, 3>(SINGLE_PASS)->finalize());
	}
	else {
		auto start = std::chrono::steady_clock::now();
		ray_data = RayFactory::default_rays<cl_double, 3>(problem_space, direction_finder.points, grid_size, SINGLE_PASS);
		auto end = std::chrono::steady_clock::now();
		std::cout << "time needed (micro seconds): " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << std::endl;
	}
	if (store_rays) {
		ray_data->save_to_file(dataFilePath("raydata.bin"));
	}
	CLConfig *clconfig = CLConfig::default_config();
	RayFactory::RayDataGpu ray_data_gpu = RayFactory::RayDataGpu(*ray_data, clconfig->context);
	cl::Buffer triangles_gpu = cl::Buffer(clconfig->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, triangle_buffer_size, (void*)triangles);
	cl::Buffer out_volume_gpu = cl::Buffer(clconfig->context, CL_MEM_READ_WRITE, cube_buffer_size);
	clconfig->queue.enqueueFillBuffer(out_volume_gpu, 0, 0, cube_buffer_size);
	JSONBuilder constants;
	constants.add("num_triangles", (long)num_triangles)
		->add("outcubesize", (long)cube_size);
	LSProgram<cl_double, 3> lsprogram(*clconfig, *ray_data, dataFilePath("inside.cl.php"), &constants);
	cl::Kernel kernel = lsprogram.get_kernel("inside");
	int argctr = 0;
	argctr = ray_data_gpu.set_constant_sweep_parameters(argctr, kernel);
	assert(argctr == 2);
	argctr = ray_data_gpu.set_variable_sweep_parameters(argctr, kernel, 0);
	assert(argctr == 3);
	kernel.setArg<cl::Buffer>(argctr++, triangles_gpu);
	kernel.setArg<cl::Buffer>(argctr++, out_volume_gpu);
	std::cout << "shooting " << ray_data_gpu.rays_size << " rays." << std::endl;
	clconfig->queue.finish();
	auto start = std::chrono::steady_clock::now();
	clconfig->queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(ray_data_gpu.rays_size));
	clconfig->queue.finish();
	auto end = std::chrono::steady_clock::now();
	std::cout << "time needed (micro seconds): " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << std::endl;
	cl_int *out_volume = (cl_int*)malloc(cube_buffer_size);
	clconfig->queue.enqueueReadBuffer(out_volume_gpu, true, 0, cube_buffer_size, out_volume);
	write_buffer_to_file(out_volume, cube_buffer_size, dataFilePath("inside_output.bin").c_str());
	std::cout << "done" << std::endl;
	std::cin.get();
	return 0;
}

int main_switcher(int argc, char** argv) {
	int ret;
	char* cmd = argv[1];
	if (strcmp(cmd, "lsdo") == 0) {
		ret = lsdo();
	}
	else if (strcmp(cmd, "inside") == 0) {
		ret = inside();
	}
	else {
		ret = 1;
		std::cerr << "Unknown command: " << cmd << std::endl;
	}
	return ret;
}

int main(int argc, char** argv) {
#ifdef NDEBUG
	std::cout << "release mode" << std::endl;
#else
	std::cout << "debug mode" << std::endl;
#endif
	if (argc < 2) {
		std::cerr << "No args given. First argument must be the command to call." << std::endl;
		return 1;
	}
	int ret;
#ifdef NDEBUG
	try {
#endif
		ret = main_switcher(argc, argv);
#ifdef NDEBUG
	}
	catch (cl::Error error) {
		ret = 2;
		std::cerr << "CL Error: " << error.what() << "(" << error.err() << ")" << std::endl;
	}
	catch (...) {
		ret = 3;
		std::cerr << "Caught an unknown exception." << std::endl;
	}
#endif
	if (ret != 0) {
		std::cin.get();
	}
	return ret;
}
