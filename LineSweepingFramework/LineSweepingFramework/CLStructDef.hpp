#pragma once

#include "CLConfig.hpp"

class CLStructDef {
protected:
	struct Entry;
	// internal data
	std::string* struct_name;
	std::vector<Entry>* entries;
	size_t total_size;
	// internal methods
	size_t calcOffset(size_t member_offset, size_t array_offset) const;
	CLStructDef(std::vector<Entry>* entries, std::string* name, size_t total_size);
	static size_t _getSizeByName(const std::vector<Entry>* entries, const char* name);
	static const Entry* getEntryByName(const std::vector<Entry>* entries, const char* name);
public:
	class Builder;
	struct Vector;
	size_t getMemberOffset(const char* name) const;
	size_t getSizeByName(const char* name);
	void* getPtr(void* dataptr, size_t member_offset, size_t array_offset = 0) const;
	size_t byte_size() const;
	std::string getStructName() const;
	std::string generateCLCode() const;
	void generateCLCode(std::stringstream& ss) const;
};

struct CLStructDef::Entry {
	size_t size; // total, including arity multiplication
	size_t arity;
	size_t offset;
	std::string name;
	std::string cl_type_name;
	friend bool operator<(const Entry &lhs, const Entry &rhs) {
		return lhs.size > rhs.size;
	}
	size_t ssize() const { // singular size
		return size / arity;
	}
	Entry(size_t ssize, size_t arity, const char* name, const char* cl_type_name) {
		this->size = ssize * arity;
		this->arity = arity;
		this->name = name;
		this->cl_type_name = cl_type_name;
		this->offset = 0;
	}
};

struct CLStructDef::Vector {
	std::vector<char> data;
	CLStructDef *definition;
	void push_back(void* x) {
		char* p = (char*)x;
		data.insert(data.end(), p, p + definition->byte_size());
	}
	void *at(size_t i) const {
		assert(i < size());
		return (void*)&(data[i*definition->byte_size()]);
	}
	void *operator[](size_t i) const {
		return at(i);
	}
	size_t size() const {
		size_t bs = definition->byte_size();
		assert(data.size() % bs == 0);
		return data.size() / bs;
	}
	size_t byte_size() const {
		assert(sizeof(char) == 1);
		return data.size();
	}
	void* raw_pointer() const {
		return (void*)data.data();
	}
	void fill_zero(size_t elements) {
		data.clear();
		data.resize(elements * definition->byte_size(), 0);
	}
	Vector() {
		definition = NULL;
	}
	Vector(const Vector& other, size_t start, size_t end) : data(other.data) {
		definition = other.definition;
	}
};

class CLStructDef::Builder {
protected:
	std::vector<Entry>* entries;
	std::string* struct_name;
	size_t accumulatedSize(size_t i) const;
public:
	Builder(char* struct_name);
	~Builder();
	std::string getStructName() const;
	template <typename T> Builder* addMember(const char* name, size_t arity = 1);
	Builder* manualAddMember(const char* name, const char* cl_type_name, size_t single_element_size, size_t arity);
	size_t getSizeByName(const char* name);
	/**
	 * destroys the builder and returns a new CLStructDef
	 * Subsequent calls to the builder will result in a SEGFAULT
	 */
	CLStructDef *finalize();
};

template <typename T> CLStructDef::Builder* CLStructDef::Builder::addMember(const char* name, size_t arity) {
	entries->emplace_back(sizeof(T), arity, name, getTypeName<T>());
	return this; // for adding multiple members in one statement
}
