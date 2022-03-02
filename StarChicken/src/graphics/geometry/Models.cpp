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
		uint8_t* data = reinterpret_cast<uint8_t*>(malloc(vertCount * vku::POSITION_TEX_NORMAL_TANGENT.size_bytes() + indexCount * sizeof(uint16_t)));

		memcpy(data, pos->data, vertCount * 3 * sizeof(float));
		positions = reinterpret_cast<vec3f*>(data);
		data += vertCount * 3 * sizeof(float);
		memcpy(data, tex->data, vertCount * 2 * sizeof(float));
		texcoords = reinterpret_cast<vec2f*>(data);
		data += vertCount * 2 * sizeof(float);
		memcpy(data, norm->data, vertCount * 3 * sizeof(float));
		normals = reinterpret_cast<vec3f*>(data);
		data += vertCount * 3 * sizeof(float);
		memcpy(data, tan->data, vertCount * 3 * sizeof(float));
		tangents = reinterpret_cast<vec3f*>(data);
		data += vertCount * 3 * sizeof(float);
		memcpy(data, index->data, indexCount * sizeof(uint16_t));
		indices = reinterpret_cast<uint16_t*>(data);
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