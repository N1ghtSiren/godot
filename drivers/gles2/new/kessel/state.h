#pragma once

#include "batch.h"
#include "drivers/gles2/rasterizer_storage_gles2.h"
#include "servers/visual/rasterizer.h"

namespace Batch
{

class BState
{
};

class State_Fill
{
public:
	State_Fill() { Reset(); }
	void Reset();
	Batch *m_pBatch_ItemGroup;
	Batch *m_pBatch_Item;
};

class State_ItemGroup
{
public:
	State_ItemGroup() { Reset(); }
	void Reset();
	BItemGroup *m_pItemGroup;
	RasterizerCanvas::Item *current_clip;
	RasterizerStorageGLES2::Shader *shader_cache;
	bool rebind_shader;
	bool prev_use_skeleton;
	int last_blend_mode;
	RID canvas_last_material;

	RasterizerCanvas::Item *m_pScissorItem;
	bool m_bScissorActive;
};

class State_Item
{
public:
	State_Item() { Reset(); }
	void Reset();
	RasterizerStorageGLES2::Skeleton *skeleton;
	bool use_skeleton;

	bool unshaded;
	int blend_mode;
	bool reclip;
	RasterizerStorageGLES2::Material *material_ptr;
};

} // namespace Batch
