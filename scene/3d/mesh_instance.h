/*************************************************************************/
/*  mesh_instance.h                                                      */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef MESH_INSTANCE_H
#define MESH_INSTANCE_H

#include "scene/3d/skeleton.h"
#include "scene/3d/visual_instance.h"
#include "scene/resources/mesh.h"
#include "scene/resources/skin.h"

class MeshInstance : public GeometryInstance {

	GDCLASS(MeshInstance, GeometryInstance);

protected:
	Ref<Mesh> mesh;
	Ref<Mesh> mesh_instance;
	Ref<Skin> skin;
	Ref<Skin> skin_internal;
	Ref<SkinReference> skin_ref;
	NodePath skeleton_path;

	bool software_skinning;

	struct BlendShapeTrack {

		int idx;
		float value;
		BlendShapeTrack() {
			idx = 0;
			value = 0;
		}
	};

	Map<StringName, BlendShapeTrack> blend_shape_tracks;
	Vector<Ref<Material> > materials;

	void _mesh_changed();
	void _resolve_skeleton_path();

	void _initialize_skinning(bool p_update_skinning = true);
	void _update_skinning();

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_mesh() const;

	void set_skin(const Ref<Skin> &p_skin);
	Ref<Skin> get_skin() const;

	void set_skeleton_path(const NodePath &p_skeleton);
	NodePath get_skeleton_path();

	int get_surface_material_count() const;
	void set_surface_material(int p_surface, const Ref<Material> &p_material);
	Ref<Material> get_surface_material(int p_surface) const;

	void set_software_skinning(bool p_enabled);
	bool is_software_skinning_enabled() const;

	Node *create_trimesh_collision_node();
	void create_trimesh_collision();

	Node *create_convex_collision_node();
	void create_convex_collision();

	void create_debug_tangents();

	virtual AABB get_aabb() const;
	virtual PoolVector<Face3> get_faces(uint32_t p_usage_flags) const;

	MeshInstance();
	~MeshInstance();
};

#endif
