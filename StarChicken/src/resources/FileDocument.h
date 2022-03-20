#pragma once

#include <string>
#include <vector>
#include <iostream>

namespace document {

	inline uint32_t read_uint32(void** data) {
		uint32_t* dat = reinterpret_cast<uint32_t*>(*data);
		*data = dat + 1;
		return *dat;
	}

	inline uint16_t read_uint16(void** data) {
		uint16_t* dat = reinterpret_cast<uint16_t*>(*data);
		*data = dat + 1;
		return *dat;
	}

	inline uint16_t read_uint8(void** data) {
		uint8_t* dat = reinterpret_cast<uint8_t*>(*data);
		*data = dat + 1;
		return *dat;
	}

	template<typename T>
	inline T read_t(void** data) {
		T* dat = reinterpret_cast<T*>(*data);
		*data = dat + 1;
		return *dat;
	}

	inline std::string read_string(uint32_t length, void** data) {
		char* dat = reinterpret_cast<char*>(*data);
		*data = dat + length;
		char* cstr = reinterpret_cast<char*>(alloca(length + 1));
		cstr[length] = '\0';
		memcpy(cstr, dat, length);
		return std::string(cstr);
	}

	inline std::string read_string(void** data) {
		uint32_t len = read_uint32(data);
		return read_string(len, data);
	}

	inline void add_pos(void** data, int32_t toAdd) {
		uint8_t* dat = reinterpret_cast<uint8_t*>(*data);
		*data = dat + toAdd;
	}

	inline uint8_t* pos(void** data) {
		uint8_t* dat = reinterpret_cast<uint8_t*>(*data);
		return dat;
	}

	struct DocumentData {
		std::string name;
		void* data;
		uint32_t numBytes;

		inline uint8_t get_ubyte() {
			return *reinterpret_cast<uint8_t*>(data);
		}

		inline uint16_t get_ushort() {
			return *reinterpret_cast<uint16_t*>(data);
		}

		inline uint32_t get_uint() {
			return *reinterpret_cast<uint32_t*>(data);
		}

		template<typename T>
		inline T& get_t() {
			return *reinterpret_cast<T*>(data);
		}
	};

	struct DocumentNode {
		std::string name;
		std::vector<DocumentNode*> children;
		std::vector<DocumentData*> data;

		DocumentNode(std::string& name);
		DocumentNode(void** data);
		~DocumentNode();

		DocumentNode* get_child(std::string childName);

		DocumentData* get_data(std::string dataName);
	};

	DocumentNode* parse_node(void** data);

	DocumentNode* parse_document(void* data);
}