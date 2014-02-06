#ifndef STKMESH_H
#define STKMESH_H


#include <IMeshSceneNode.h>
#include <IMesh.h>
#include "../lib/irrlicht/source/Irrlicht/CMeshSceneNode.h"
#include "glwrap.hpp"
#include <vector>

struct GLMesh {
	GLuint vao_first_pass;
	GLuint vao_second_pass;
	GLuint vao_glow_pass;
	GLuint vertex_buffer;
	GLuint index_buffer;
	GLuint textures[2];
	GLenum PrimitiveType;
	GLenum IndexType;
	size_t IndexCount;
	size_t Stride;
};

class STKMesh : public irr::scene::CMeshSceneNode
{
protected:
	std::vector<GLMesh> GLmeshes;
	void draw(const GLMesh &mesh, video::E_MATERIAL_TYPE type);
public:
	STKMesh(irr::scene::IMesh* mesh, ISceneNode* parent, irr::scene::ISceneManager* mgr,	irr::s32 id,
		const irr::core::vector3df& position = irr::core::vector3df(0,0,0),
		const irr::core::vector3df& rotation = irr::core::vector3df(0,0,0),
		const irr::core::vector3df& scale = irr::core::vector3df(1.0f, 1.0f, 1.0f));
	virtual void render();
	~STKMesh();
};

#endif // STKMESH_H
