#include "Models.h"
#include "..\VertexFormats.h"

namespace geom {
	Mesh::Mesh(document::DocumentNode* geo) : meshId{ 0 } {
		document::DocumentData* pos = geo->get_data("positions");
		document::DocumentData* tex = geo->get_data("texcoords");
		document::DocumentData* norm = geo->get_data("normals");
		document::DocumentData* tan = geo->get_data("tangents");
		document::DocumentData* index = geo->get_data("indices");
		vertCount = pos->numBytes / (3*sizeof(float));
		indexCount = index->numBytes / sizeof(uint16_t);
		isSkinned = false;
		uint32_t dataSize = ((pos > 0) * 3 * sizeof(float) + (tex > 0) * 2 * sizeof(float) + (norm > 0) * 3 * sizeof(float) + (tan > 0) * 3 * sizeof(float)) * vertCount + (index > 0) * indexCount * sizeof(uint16_t);
		uint8_t* data = reinterpret_cast<uint8_t*>(malloc(dataSize));

		if (pos) {
			memcpy(data, pos->data, vertCount * 3 * sizeof(float));
			positions = reinterpret_cast<vec3f*>(data);
			data += vertCount * 3 * sizeof(float);
		} else {
			positions = nullptr;
		}
		if (tex) {
			memcpy(data, tex->data, vertCount * 2 * sizeof(float));
			texcoords = reinterpret_cast<vec2f*>(data);
			data += vertCount * 2 * sizeof(float);
		} else {
			texcoords = nullptr;
		}
		if (norm) {
			memcpy(data, norm->data, vertCount * 3 * sizeof(float));
			normals = reinterpret_cast<vec3f*>(data);
			data += vertCount * 3 * sizeof(float);
		} else {
			normals = nullptr;
		}
		if (tan) {
			memcpy(data, tan->data, vertCount * 3 * sizeof(float));
			tangents = reinterpret_cast<vec3f*>(data);
			data += vertCount * 3 * sizeof(float);
		} else {
			tangents = nullptr;
		}
		if (index) {
			memcpy(data, index->data, indexCount * sizeof(uint16_t));
			indices = reinterpret_cast<uint16_t*>(data);
		} else {
			indices = nullptr;
		}
	}
	Mesh::~Mesh() {
		free(positions);
	}

	SkinnedMesh::SkinnedMesh(document::DocumentNode* geo) : geom::Mesh(geo) {
		isSkinned = true;
		//TODO implement
	}
	SkinnedMesh::~SkinnedMesh() {

	}

	Model::Model(Mesh* mesh) : mesh{ mesh }, transformChanged{ true } {
		modelMatrix.set_identity();
	}
}