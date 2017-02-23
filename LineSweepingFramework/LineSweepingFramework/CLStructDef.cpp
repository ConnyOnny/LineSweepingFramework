#include "CLStructDef.hpp"
#include <sstream>

CLStructDef::CLStructDef(std::vector<Entry>* entries, std::string* name, size_t total_size) {
	this->entries = entries;
	this->struct_name = name;
	this->total_size = total_size;
}

CLStructDef::Builder::Builder(char* struct_name) {
	this->struct_name = new std::string(struct_name);
	this->entries = new std::vector<Entry>();
}

CLStructDef::Builder::~Builder() {
	if (struct_name) {
		delete struct_name;
		struct_name = NULL;
	}
	if (entries) {
		delete entries;
		entries = NULL;
	}
}

CLStructDef* CLStructDef::Builder::finalize() {
	// sort by size
	std::stable_sort(entries->begin(), entries->end());
	// calc offsets
	size_t current = 0;
	size_t biggest_size = 0;
	for (size_t i = 0; i < entries->size(); i++) {
		Entry *entry = &(*entries)[i];
		if (entry->ssize() > biggest_size) {
			biggest_size = entry->ssize();
		}
		size_t padding = (entry->ssize() - (current % entry->ssize())) % entry->ssize();
		current += padding;
		assert(current % entry->ssize() == 0); // self-alignment
		entry->offset = current;
		current += entry->size;
	}
	size_t final_padding = (biggest_size - (current % biggest_size)) % biggest_size;
	current += final_padding;
	assert(current % biggest_size == 0);
	CLStructDef* ret = new CLStructDef(entries, struct_name, current);
	entries = NULL;
	struct_name = NULL;
	return ret;
}

size_t CLStructDef::getMemberOffset(const char* name) const {
	for (auto it = entries->begin(); it != entries->end(); ++it) {
		if (it->name == name) {
			return it->offset;
		}
	}
	assert(false);
	return -1;
}

const CLStructDef::Entry* CLStructDef::getEntryByName(const std::vector<Entry>* entries, const char* name) {
	for (auto it = entries->begin(); it != entries->end(); ++it) {
		if (it->name == name) {
			return &(*it);
		}
	}
	return NULL;
}
size_t CLStructDef::_getSizeByName(const std::vector<Entry>* entries, const char* name) {
	const Entry* e = getEntryByName(entries, name);
	if (e) {
		return e->size;
	}
	else {
		return 0;
	}
}
size_t CLStructDef::Builder::getSizeByName(const char* name) {
	return _getSizeByName(entries, name);
}
size_t CLStructDef::getSizeByName(const char* name) {
	return _getSizeByName(entries, name);
}

void* CLStructDef::getPtr(void* dataptr, size_t member_offset, size_t array_offset) const {
	static_assert(sizeof(char) == 1, "char must be one byte");
	char* p = (char*)dataptr;
	p += array_offset * total_size + member_offset;
	return p;
}
size_t CLStructDef::byte_size() const {
	return total_size;
}
std::string CLStructDef::generateCLCode() const {
	std::stringstream ss;
	generateCLCode(ss);
	return ss.str();
}
std::string CLStructDef::getStructName() const {
	return *struct_name;
}
std::string CLStructDef::Builder::getStructName() const {
	return *struct_name;
}
void CLStructDef::generateCLCode(std::stringstream& ss) const {
	ss << "typedef struct {" << std::endl;
	for (auto it = entries->begin(); it != entries->end(); ++it) {
		ss << "\t" << it->cl_type_name << ' ';
		ss << it->name;
		if (it->arity != 1) {
			ss << '[' << it->arity << ']';
		}
		ss << ';' << std::endl;
	}
	ss << "} " << *struct_name << ';' << std::endl;
}

CLStructDef::Builder* CLStructDef::Builder::manualAddMember(const char* name, const char* cl_type_name, size_t single_element_size, size_t arity) {
	entries->emplace_back(single_element_size, arity, name, cl_type_name);
	return this;
}
