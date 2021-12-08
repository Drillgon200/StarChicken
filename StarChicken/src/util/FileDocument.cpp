#include "FileDocument.h"

namespace document {

	DocumentNode::DocumentNode(std::string& name) : name{ name }, children{}, data{} {
	}

	DocumentNode::DocumentNode(void** data) : name{ read_string(data) }, children{}, data{} {
	}

	DocumentNode::~DocumentNode() {
		for (DocumentNode* node : children) {
			delete node;
		}
		for (DocumentData* dat : data) {
			delete dat;
		}
	}

	DocumentNode* DocumentNode::get_child(std::string childName) {
		for (DocumentNode* node : children) {
			if (node->name == childName) {
				return node;
			}
		}
		return nullptr;
	}

	DocumentData* DocumentNode::get_data(std::string dataName) {
		for (DocumentData* dat : data) {
			if (dat->name == dataName) {
				return dat;
			}
		}
		return nullptr;
	}

	DocumentNode* parse_node(void** data) {
		DocumentNode* node = new DocumentNode(data);
		uint8_t* prevPos = pos(data);
		uint32_t totalLength = read_uint32(data);
		uint32_t dataLength = read_uint32(data);
		while (static_cast<uintptr_t>(pos(data) - prevPos) < dataLength) {
			DocumentData* dat = new DocumentData();
			dat->name = read_string(data);
			dat->numBytes = read_uint32(data);
			dat->data = *data;
			add_pos(data, dat->numBytes);
			node->data.push_back(dat);
		}
		while (static_cast<uintptr_t>(pos(data) - prevPos) < totalLength) {
			node->children.push_back(parse_node(data));
		}
		return node;
	}

	DocumentNode* parse_document(void* data) {
		if (std::string("DUCK") != read_string(4, &data)) {
			std::cerr << "File magic did not match!" << std::endl;
			return nullptr;
		}
		std::string rootName = "root";
		DocumentNode* root = new DocumentNode(rootName);
		while (std::string("EOF") != read_string(3, &data)) {
			add_pos(&data, -3);
			DocumentNode* node = parse_node(&data);
			root->children.push_back(node);
		}
		return root;
	}
}