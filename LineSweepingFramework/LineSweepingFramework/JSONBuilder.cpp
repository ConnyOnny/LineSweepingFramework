#include "JSONBuilder.hpp"

void JSONBuilder::appendStr(const char* s) {
	*output << '"';
	for (size_t i = 0; s[i] != '\0'; i++) {
		switch (s[i]) {
		case '\"':
			*output << '\\' << '"';
			break;
		case '\n':
			*output << '\\' << 'n';
			break;
		case '\t':
			*output << '\\' << 't';
			break;
		default:
			*output << s[i];
			break;
		}
	}
	*output << '"';
}

JSONBuilder::JSONBuilder() {
	output = new std::stringstream;
	*output << '{' << std::endl;
	hasValues = false;
}

JSONBuilder::JSONBuilder(const JSONBuilder& old) {
	if (old.output != NULL) {
		this->output = new std::stringstream();
		*(this->output) << old.output->str();
	}
	else {
		this->output = NULL;
	}
	this->hasValues = old.hasValues;
}

JSONBuilder::~JSONBuilder() {
	if (output)
		delete output;
}

JSONBuilder* JSONBuilder::add(const char* name, const char* value) {
	if (hasValues)
		*output << ',' << std::endl;
	appendStr(name);
	*output << ':';
	appendStr(value);
	hasValues = true;
	return this;
}

JSONBuilder* JSONBuilder::add(const char* name, const std::string& value) {
	return add(name, value.c_str());
}

JSONBuilder* JSONBuilder::add(const char* name, const int value) {
	return add(name, (long)value);
}

JSONBuilder* JSONBuilder::add(const char* name, const long value) {
	if (hasValues)
		*output << ',' << std::endl;
	appendStr(name);
	*output << ':' << value;
	hasValues = true;
	return this;
}

JSONBuilder* JSONBuilder::add(const char* name, const double value) {
	if (hasValues)
		*output << ',' << std::endl;
	appendStr(name);
	*output << ':' << value;
	hasValues = true;
	return this;
}

JSONBuilder* JSONBuilder::add(const char* name, const bool value) {
	if (hasValues)
		*output << ',' << std::endl;
	appendStr(name);
	*output << ':' << (value ? "true" : "false");
	hasValues = true;
	return this;
}

std::string JSONBuilder::done() {
	*output << '}';
	std::string ret = output->str();
	delete output;
	output = NULL;
	return ret;
}
