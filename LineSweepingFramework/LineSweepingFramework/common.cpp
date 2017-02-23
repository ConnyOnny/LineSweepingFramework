#include "common.hpp"

#include <iostream>
#include <fstream>
#include <csignal>
#include <string>
#include <cstdio>
#include <memory>
#include <cstdlib>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#define SEPARATOR '\\'
#else
#define POPEN popen
#define PCLOSE pclose
#define SEPARATOR '/'
#endif

std::string exec(const char* cmd) {
	const size_t bufsize = 4096;
	std::shared_ptr<FILE> pipe(POPEN(cmd, "r"), PCLOSE);
	if (!pipe) return "ERROR";
	char buffer[bufsize];
	std::string result = "";
	while (!feof(pipe.get())) {
		if (fgets(buffer, bufsize, pipe.get()) != NULL)
			result += buffer;
	}
	return result;
}

std::string dataFilePath(const char* fname) {
	char* ddir = getenv("DATA_DIR");
	if (!ddir) {
		ddir = "data";
	}
	std::string ret(ddir);
	if (fname) {
		ret += SEPARATOR;
		ret += fname;
	}
	return ret;
}

void writeStringToFile(std::string what, std::string fname) {
	std::ofstream writer(fname);
	writer << what;
	writer.close();
}

std::string readStringFromFile(std::string fname) {
	std::ifstream file(fname);
	std::stringstream ss;
	ss << file.rdbuf();
	file.close();
	return ss.str();
}

std::string executePhp(const char* phpFileName, std::string json, bool absolutePath) {
	char jsonfname[L_tmpnam+16];
	std::tmpnam(jsonfname);
	strncat(jsonfname, ".tmp.json", 15);
	std::string jsonfpath = dataFilePath(jsonfname);
	std::ofstream writer(jsonfpath);
	writer << json;
	writer.close();
	std::string phpFilePath = absolutePath ? phpFileName : dataFilePath(phpFileName);
	std::string cmd = "php " + phpFilePath + " " + jsonfpath;
	std::cout << "executing: " << cmd << std::endl;
	return exec(cmd.c_str());
}

cl::Program getProgram(cl::Context context, const char* phpFileName, std::string json, std::string buildOptions, bool absolutePath) {
#ifdef LOADPHPOUT
	std::string source = readStringFromFile("phpoutput.cl");
#else
	std::string source = executePhp(phpFileName, json, absolutePath);
#ifndef NDEBUG
	writeStringToFile(source, "phpoutput.cl");
#endif
#endif
	return buildProgramFromSourceString(context, source, buildOptions);
}

void panic() {
	std::cerr.flush();
	std::cin.get();
	raise(SIGTERM);
}

void read_rgba8(cl_uchar4* data, int width, int height, const char* fname) {
	std::ifstream is;
	is.open(fname, std::ios::binary);
	if (is.fail()) {
		std::cerr << "Could not open file to read: " << fname << std::endl;
		panic();
	}
	size_t toread = width * height * 4;
	is.seekg(0, is.end);
	size_t actual_file_size = is.tellg();
	is.seekg(0, is.beg);
	if (toread != actual_file_size) {
		std::cerr << "Input file '" << fname << "' has length " << actual_file_size << " but should have length " << toread << std::endl;
		panic();
	}
	is.read((char*)data, toread);
}

void write_buffer_to_file(void* data, size_t size, const char* fname) {
	std::ofstream out;
	out.open(fname, std::ios::binary);
	if (out.fail()) {
		std::cerr << "Could not open file to write: " << fname << std::endl;
		panic();
	}
	out.write((char*)data, size);
}

void write_rgba8(cl_uchar4* data, int pxcount, const char* fname) {
	std::ofstream out;
	out.open(fname, std::ios::binary);
	if (out.fail()) {
		std::cerr << "Could not open file to write: " << fname << std::endl;
		panic();
	}
	out.write((char*)data, pxcount * 4);
}

void read_gray8(cl_uchar* data, int width, int height, const char* fname) {
	std::ifstream is;
	is.open(fname, std::ios::binary);
	if (is.fail()) {
		std::cerr << "Could not open file to read: " << fname << std::endl;
		panic();
	}
	size_t toread = width * height;
	is.seekg(0, is.end);
	size_t actual_file_size = is.tellg();
	is.seekg(0, is.beg);
	if (toread != actual_file_size) {
		std::cerr << "Input file '" << fname << "' has length " << actual_file_size << " but should have length " << toread << std::endl;
		panic();
	}
	is.read((char*)data, toread);
}

void write_gray8(cl_uchar* data, int pxcount, const char* fname) {
	std::ofstream out;
	out.open(fname, std::ios::binary);
	if (out.fail()) {
		std::cerr << "Could not open file to write: " << fname << std::endl;
		panic();
	}
	out.write((char*)data, pxcount);
}

void read_dpt(cl_float** data, uint32_t* width, uint32_t* height, const char* fname) {
	static_assert(sizeof(cl_float) == 4, "cl_float must be 4 bytes, otherwise this code doesn't work");
	const int HEADER_LENGTH = 12;
	std::ifstream is;
	is.open(fname, std::ios::binary);
	if (is.fail()) {
		std::cerr << "Could not open file to read: " << fname << std::endl;
		panic();
	}
	cl_float check = 0;
	is.read((char*)&check, 4);
	assert(check == 202021.25);
	is.read((char*)width, 4);
	is.read((char*)height, 4);
	size_t area = *width * *height;
	size_t toread = area * 4;
	assert(is.tellg() == (std::streampos)HEADER_LENGTH);
	*data = new cl_float[area];
	is.read((char*)*data, toread);
	assert(!is.fail());
	assert(!is.eof());
	is.read((char*)&check, 1);
	assert(is.eof());
	assert(is.fail());
}

bool dimension_needs_array(size_t dimension) {
	switch (dimension) {
	case 1:
	case 2:
	case 4:
	case 8:
	case 16:
		return false;
	default:
		return true;
	}
}

template<> const char* precisionString<cl_float>() {
	return "single";
}
template<> const char* precisionString<cl_double>() {
	return "double";
}