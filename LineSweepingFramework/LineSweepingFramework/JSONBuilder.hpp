#pragma once

#include <sstream>

class JSONBuilder
{
protected:
	std::stringstream* output;
	void appendStr(const char* s);
	bool hasValues;
public:
	JSONBuilder();
	JSONBuilder(const JSONBuilder&); // copy constructor
	~JSONBuilder();
	JSONBuilder* add(const char* name, const char* value);
	JSONBuilder* add(const char* name, const std::string& value);
	JSONBuilder* add(const char* name, const int value);
	JSONBuilder* add(const char* name, const long value);
	JSONBuilder* add(const char* name, const double value);
	JSONBuilder* add(const char* name, const bool value);
	std::string done();
};

