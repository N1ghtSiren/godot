/*************************************************************************/
/*  mesh_instance.cpp                                                    */
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

#include "mesh_instance.h"

#include "collision_shape.h"
#include "core/core_string_names.h"
#include "physics_body.h"
#include "scene/resources/material.h"
#include "scene/scene_string_names.h"
#include "skeleton.h"

bool MeshInstance::_set(const StringName &p_name, const Variant &p_value) {

	//this is not _too_ bad performance wise, really. it only arrives here if the property was not set anywhere else.
	//add to it that it's probably found on first call to _set anyway.

	if (!get_instance().is_valid())
		return false;

	Map<StringName, BlendShapeTrack>::Element *E = blend_shape_tracks.find(p_name);
	if (E) {
		E->get().value = p_value;
		VisualServer::get_singleton()->instance_set_blend_shape_weight(get_instance(), E->get().idx, E->get().value);
		return true;
	}

	if (p_name.operator String().begins_with("material/")) {
		int idx = p_name.operator String().get_slicec('/', 1).to_int();
		if (idx >= materials.size() || idx < 0)
			return false;

		set_surface_material(idx, p_value);
		return true;
	}

	return false;
}

bool MeshInstance::_get(const StringName &p_name, Variant &r_ret) const {

	if (!get_instance().is_valid())
		return false;

	const Map<StringName, BlendShapeTrack>::Element *E = blend_shape_tracks.find(p_name);
	if (E) {
		r_ret = E->get().value;
		return true;
	}

	if (p_name.operator String().begins_with("material/")) {
		int idx = p_name.operator String().get_slicec('/', 1).to_int();
		if (idx >= materials.size() || idx < 0)
			return false;
		r_ret = materials[idx];
		return true;
	}
	return false;
}

void MeshInstance::_get_property_list(List<PropertyInfo> *p_list) const {

	List<String> ls;
	for (const Map<StringName, BlendShapeTrack>::Element *E = blend_shape_tracks.front(); E; E = E->next()) {

		ls.push_back(E->key());
	}

	ls.sort();

	for (List<String>::Element *E = ls.front(); E; E = E->next()) {
		p_list->push_back(PropertyInfo(Variant::REAL, E->get(), PROPERTY_HINT_RANGE, "0,1,0.00001"));
	}

	if (mesh.is_valid()) {
		for (int i = 0; i < mesh->get_surface_count(); i++) {
			p_list->push_back(PropertyInfo(Variant::OBJECT, "material/" + itos(i), PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial,SpatialMaterial"));
		}
	}
}

void MeshInstance::set_mesh(const Ref<Mesh> &p_mesh) {

	if (mesh == p_mesh)
		return;

	if (mesh.is_valid()) {
		mesh->disconnect(CoreStringNames::get_singleton()->changed, this, SceneStringNames::get_singleton()->_mesh_changed);
		materials.clear();
	}

	if (skin_ref.is_valid() && mesh.is_valid() && software_skinning) {
		ERR_FAIL_COND(!skin_ref->get_skeleton_node());
		skin_ref->get_skeleton_node()->disconnect("skeleton_updated", this, "_update_skinning");
	}

	mesh_instance = Ref<Mesh>();

	mesh = p_mesh;

	blend_shape_tracks.clear();
	if (mesh.is_valid()) {

		for (int i = 0; i < mesh->get_blend_shape_count(); i++) {

			BlendShapeTrack mt;
			mt.idx = i;
			mt.value = 0;
			blend_shape_tracks["blend_shapes/" + String(mesh->get_blend_shape_name(i))] = mt;
		}

		mesh->connect(CoreStringNames::get_singleton()->changed, this, SceneStringNames::get_singleton()->_mesh_changed);
		materials.resize(mesh->get_surface_count());

		_initialize_skinning();
	} else {

		set_base(RID());
	}

	update_gizmo();

	_change_notify();
}
Ref<Mesh> MeshInstance::get_mesh() const {

	return mesh;
}

void MeshInstance::_resolve_skeleton_path() {

	Ref<SkinReference> new_skin_reference;

	if (!skeleton_path.is_empty()) {
		Skeleton *skeleton = Object::cast_to<Skeleton>(get_node(skeleton_path));
		if (skeleton) {
			new_skin_reference = skeleton->register_skin(skin_internal);
			if (skin_internal.is_null()) {
				//a skin was created for us
				skin_internal = new_skin_reference->get_skin();
				_change_notify();
			}
		}
	}

	if (skin_ref.is_valid() && mesh.is_valid() && software_skinning) {
		ERR_FAIL_COND(!skin_ref->get_skeleton_node());
		skin_ref->get_skeleton_node()->disconnect("skeleton_updated", this, "_update_skinning");
	}

	skin_ref = new_skin_reference;

	_initialize_skinning(false);
}

void MeshInstance::_initialize_skinning(bool p_update_skinning) {
	VisualServer *visual_server = VisualServer::get_singleton();

	if (skin_ref.is_valid() && mesh.is_valid()) {
		if (software_skinning) {
			ERR_FAIL_COND(!skin_ref->get_skeleton_node());
			skin_ref->get_skeleton_node()->connect("skeleton_updated", this, "_update_skinning");

			if (mesh_instance.is_null()) {
				if (mesh->get_blend_shape_count() > 0) {
					ERR_PRINT("Blend shapes are not supported for software skinning.");
				}

				Ref<ArrayMesh> software_mesh;
				software_mesh.instance();

				// Initialize mesh for dynamic update
				int surface_count = mesh->get_surface_count();
				for (int surface_index = 0; surface_index < surface_count; ++surface_index) {
					uint32_t format = mesh->surface_get_format(surface_index);
					format &= ~Mesh::ARRAY_COMPRESS_VERTEX;
					format |= Mesh::ARRAY_FLAG_USE_DYNAMIC_UPDATE;

					ERR_CONTINUE(Mesh::PRIMITIVE_TRIANGLES != mesh->surface_get_primitive_type(surface_index));

					Array surface_arrays = mesh->surface_get_arrays(surface_index);
					Ref<Material> material = mesh->surface_get_material(surface_index);

					software_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, surface_arrays, Array(), format);
					software_mesh->surface_set_material(surface_index, material);
				}

				mesh_instance = software_mesh;
			}

			visual_server->instance_attach_skeleton(get_instance(), RID());

			if (p_update_skinning) {
				// Intialize from current skeleton pose
				_update_skinning();
			}
		} else {
			visual_server->instance_attach_skeleton(get_instance(), skin_ref->get_skeleton());
			mesh_instance = Ref<Mesh>();
		}
	} else {
		visual_server->instance_attach_skeleton(get_instance(), RID());
		mesh_instance = Ref<Mesh>();
	}

	RID render_mesh = mesh_instance.is_valid() ? mesh_instance->get_rid() : mesh->get_rid();
	if (render_mesh != get_base()) {
		set_base(render_mesh);

		// Update instance materials after switching mesh
		ERR_FAIL_COND(mesh.is_null());
		int surface_count = mesh->get_surface_count();
		for (int surface_index = 0; surface_index < surface_count; ++surface_index) {
			if (materials[surface_index].is_valid()) {
				visual_server->instance_set_surface_material(get_instance(), surface_index, materials[surface_index]->get_rid());
			}
		}
	}
}

void MeshInstance::_update_skinning() {
	ERR_FAIL_COND(!software_skinning);

	ERR_FAIL_COND(skin_ref.is_null());
	RID skeleton = skin_ref->get_skeleton();
	ERR_FAIL_COND(!skeleton.is_valid());

	ERR_FAIL_COND(!mesh_instance.is_valid());
	RID mesh_rid = mesh_instance->get_rid();
	ERR_FAIL_COND(!mesh_rid.is_valid());

	VisualServer *visual_server = VisualServer::get_singleton();

	// Prepare bone transforms.
	int num_bones = visual_server->skeleton_get_bone_count(skeleton);
	const int SKIN_MAX_BONES = 128;
	ERR_FAIL_COND(num_bones > SKIN_MAX_BONES);
	Transform bone_transforms[SKIN_MAX_BONES];
	for (int bone_index = 0; bone_index < num_bones; ++bone_index) {
		bone_transforms[bone_index] = visual_server->skeleton_bone_get_transform(skeleton, bone_index);
	}

	// Apply skinning.
	int surface_count = mesh_instance->get_surface_count();
	for (int surface_index = 0; surface_index < surface_count; ++surface_index) {
		uint32_t format = mesh_instance->surface_get_format(surface_index);

		ERR_CONTINUE(0 == (format & Mesh::ARRAY_FORMAT_BONES));
		ERR_CONTINUE(0 == (format & Mesh::ARRAY_FORMAT_WEIGHTS));

		const int vertex_count = mesh_instance->surface_get_array_len(surface_index);
		const int index_count = mesh_instance->surface_get_array_index_len(surface_index);

		uint32_t array_offsets[Mesh::ARRAY_MAX];
		uint32_t stride = visual_server->mesh_surface_make_offsets_from_format(format, vertex_count, index_count, array_offsets);
		uint32_t offset_vertices = array_offsets[Mesh::ARRAY_VERTEX];
		uint32_t offset_bones = array_offsets[Mesh::ARRAY_BONES];
		uint32_t offset_weights = array_offsets[Mesh::ARRAY_WEIGHTS];

		PoolByteArray buffer = visual_server->mesh_surface_get_array(mesh_rid, surface_index);
		PoolByteArray::Write buffer_write = buffer.write();

		for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
			float bone_weights[4];
			if (format & Mesh::ARRAY_COMPRESS_WEIGHTS) {
				const uint16_t *weight_ptr = (const uint16_t *)(buffer_write.ptr() + offset_weights + (vertex_index * stride));
				bone_weights[0] = (weight_ptr[0] / (float)0xFFFF);
				bone_weights[1] = (weight_ptr[1] / (float)0xFFFF);
				bone_weights[2] = (weight_ptr[2] / (float)0xFFFF);
				bone_weights[3] = (weight_ptr[3] / (float)0xFFFF);
			} else {
				const float *weight_ptr = (const float *)(buffer_write.ptr() + offset_weights + (vertex_index * stride));
				bone_weights[0] = weight_ptr[0];
				bone_weights[1] = weight_ptr[1];
				bone_weights[2] = weight_ptr[2];
				bone_weights[3] = weight_ptr[3];
			}

			int bone_ids[4];
			if (format & Mesh::ARRAY_FLAG_USE_16_BIT_BONES) {
				const uint16_t *bones_ptr = (const uint16_t *)(buffer_write.ptr() + offset_bones + (vertex_index * stride));
				bone_ids[0] = bones_ptr[0];
				bone_ids[1] = bones_ptr[1];
				bone_ids[2] = bones_ptr[2];
				bone_ids[3] = bones_ptr[3];
			} else {
				const uint8_t *bones_ptr = buffer_write.ptr() + offset_bones + (vertex_index * stride);
				bone_ids[0] = bones_ptr[0];
				bone_ids[1] = bones_ptr[1];
				bone_ids[2] = bones_ptr[2];
				bone_ids[3] = bones_ptr[3];
			}

			Transform vertex_transform;
			vertex_transform.basis.set_zero();

			for (int i = 0; i < 4; ++i) {
				int bone_id = bone_ids[i];
				ERR_CONTINUE(bone_id > num_bones);
				const Transform &bone_transform = bone_transforms[bone_id];
				float bone_weight = bone_weights[i];
				vertex_transform.origin += bone_weight * bone_transform.origin;
				vertex_transform.basis += bone_transform.basis * bone_weight;
			}

			Vector3 &vertex = (Vector3 &)buffer_write[vertex_index * stride + offset_vertices];
			vertex = vertex_transform.xform(vertex);
		}

		visual_server->mesh_surface_update_region(mesh_rid, surface_index, 0, buffer);
	}
}

void MeshInstance::set_skin(const Ref<Skin> &p_skin) {
	skin_internal = p_skin;
	skin = p_skin;
	if (!is_inside_tree())
		return;
	_resolve_skeleton_path();
}

Ref<Skin> MeshInstance::get_skin() const {
	return skin;
}

void MeshInstance::set_skeleton_path(const NodePath &p_skeleton) {

	skeleton_path = p_skeleton;
	if (!is_inside_tree())
		return;
	_resolve_skeleton_path();
}

NodePath MeshInstance::get_skeleton_path() {
	return skeleton_path;
}

AABB MeshInstance::get_aabb() const {

	if (!mesh.is_null())
		return mesh->get_aabb();

	return AABB();
}

PoolVector<Face3> MeshInstance::get_faces(uint32_t p_usage_flags) const {

	if (!(p_usage_flags & (FACES_SOLID | FACES_ENCLOSING)))
		return PoolVector<Face3>();

	if (mesh.is_null())
		return PoolVector<Face3>();

	return mesh->get_faces();
}

Node *MeshInstance::create_trimesh_collision_node() {

	if (mesh.is_null())
		return NULL;

	Ref<Shape> shape = mesh->create_trimesh_shape();
	if (shape.is_null())
		return NULL;

	StaticBody *static_body = memnew(StaticBody);
	CollisionShape *cshape = memnew(CollisionShape);
	cshape->set_shape(shape);
	static_body->add_child(cshape);
	return static_body;
}

void MeshInstance::create_trimesh_collision() {

	StaticBody *static_body = Object::cast_to<StaticBody>(create_trimesh_collision_node());
	ERR_FAIL_COND(!static_body);
	static_body->set_name(String(get_name()) + "_col");

	add_child(static_body);
	if (get_owner()) {
		CollisionShape *cshape = Object::cast_to<CollisionShape>(static_body->get_child(0));
		static_body->set_owner(get_owner());
		cshape->set_owner(get_owner());
	}
}

Node *MeshInstance::create_convex_collision_node() {

	if (mesh.is_null())
		return NULL;

	Ref<Shape> shape = mesh->create_convex_shape();
	if (shape.is_null())
		return NULL;

	StaticBody *static_body = memnew(StaticBody);
	CollisionShape *cshape = memnew(CollisionShape);
	cshape->set_shape(shape);
	static_body->add_child(cshape);
	return static_body;
}

void MeshInstance::create_convex_collision() {

	StaticBody *static_body = Object::cast_to<StaticBody>(create_convex_collision_node());
	ERR_FAIL_COND(!static_body);
	static_body->set_name(String(get_name()) + "_col");

	add_child(static_body);
	if (get_owner()) {
		CollisionShape *cshape = Object::cast_to<CollisionShape>(static_body->get_child(0));
		static_body->set_owner(get_owner());
		cshape->set_owner(get_owner());
	}
}

void MeshInstance::_notification(int p_what) {

	if (p_what == NOTIFICATION_ENTER_TREE) {
		_resolve_skeleton_path();
	}
}

int MeshInstance::get_surface_material_count() const {

	return materials.size();
}

void MeshInstance::set_surface_material(int p_surface, const Ref<Material> &p_material) {

	ERR_FAIL_INDEX(p_surface, materials.size());

	materials.write[p_surface] = p_material;

	if (materials[p_surface].is_valid())
		VS::get_singleton()->instance_set_surface_material(get_instance(), p_surface, materials[p_surface]->get_rid());
	else
		VS::get_singleton()->instance_set_surface_material(get_instance(), p_surface, RID());
}

Ref<Material> MeshInstance::get_surface_material(int p_surface) const {

	ERR_FAIL_INDEX_V(p_surface, materials.size(), Ref<Material>());

	return materials[p_surface];
}

void MeshInstance::set_software_skinning(bool p_enabled) {
	if (p_enabled == software_skinning) {
		return;
	}

	if (skin_ref.is_valid() && mesh.is_valid() && software_skinning) {
		ERR_FAIL_COND(!skin_ref->get_skeleton_node());
		skin_ref->get_skeleton_node()->disconnect("skeleton_updated", this, "_update_skinning");
	}

	software_skinning = p_enabled;

	_initialize_skinning();
}

bool MeshInstance::is_software_skinning_enabled() const {
	return software_skinning;
}

void MeshInstance::_mesh_changed() {

	materials.resize(mesh->get_surface_count());
}

void MeshInstance::create_debug_tangents() {

	Vector<Vector3> lines;
	Vector<Color> colors;

	Ref<Mesh> mesh = get_mesh();
	if (!mesh.is_valid())
		return;

	for (int i = 0; i < mesh->get_surface_count(); i++) {
		Array arrays = mesh->surface_get_arrays(i);
		Vector<Vector3> verts = arrays[Mesh::ARRAY_VERTEX];
		Vector<Vector3> norms = arrays[Mesh::ARRAY_NORMAL];
		if (norms.size() == 0)
			continue;
		Vector<float> tangents = arrays[Mesh::ARRAY_TANGENT];
		if (tangents.size() == 0)
			continue;

		for (int j = 0; j < verts.size(); j++) {
			Vector3 v = verts[j];
			Vector3 n = norms[j];
			Vector3 t = Vector3(tangents[j * 4 + 0], tangents[j * 4 + 1], tangents[j * 4 + 2]);
			Vector3 b = (n.cross(t)).normalized() * tangents[j * 4 + 3];

			lines.push_back(v); //normal
			colors.push_back(Color(0, 0, 1)); //color
			lines.push_back(v + n * 0.04); //normal
			colors.push_back(Color(0, 0, 1)); //color

			lines.push_back(v); //tangent
			colors.push_back(Color(1, 0, 0)); //color
			lines.push_back(v + t * 0.04); //tangent
			colors.push_back(Color(1, 0, 0)); //color

			lines.push_back(v); //binormal
			colors.push_back(Color(0, 1, 0)); //color
			lines.push_back(v + b * 0.04); //binormal
			colors.push_back(Color(0, 1, 0)); //color
		}
	}

	if (lines.size()) {

		Ref<SpatialMaterial> sm;
		sm.instance();

		sm->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
		sm->set_flag(SpatialMaterial::FLAG_SRGB_VERTEX_COLOR, true);
		sm->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);

		Ref<ArrayMesh> am;
		am.instance();
		Array a;
		a.resize(Mesh::ARRAY_MAX);
		a[Mesh::ARRAY_VERTEX] = lines;
		a[Mesh::ARRAY_COLOR] = colors;

		am->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, a);
		am->surface_set_material(0, sm);

		MeshInstance *mi = memnew(MeshInstance);
		mi->set_mesh(am);
		mi->set_name("DebugTangents");
		add_child(mi);
#ifdef TOOLS_ENABLED

		if (this == get_tree()->get_edited_scene_root())
			mi->set_owner(this);
		else
			mi->set_owner(get_owner());
#endif
	}
}

void MeshInstance::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &MeshInstance::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh"), &MeshInstance::get_mesh);
	ClassDB::bind_method(D_METHOD("set_skeleton_path", "skeleton_path"), &MeshInstance::set_skeleton_path);
	ClassDB::bind_method(D_METHOD("get_skeleton_path"), &MeshInstance::get_skeleton_path);
	ClassDB::bind_method(D_METHOD("set_skin", "skin"), &MeshInstance::set_skin);
	ClassDB::bind_method(D_METHOD("get_skin"), &MeshInstance::get_skin);

	ClassDB::bind_method(D_METHOD("get_surface_material_count"), &MeshInstance::get_surface_material_count);
	ClassDB::bind_method(D_METHOD("set_surface_material", "surface", "material"), &MeshInstance::set_surface_material);
	ClassDB::bind_method(D_METHOD("get_surface_material", "surface"), &MeshInstance::get_surface_material);

	ClassDB::bind_method(D_METHOD("set_software_skinning", "enabled"), &MeshInstance::set_software_skinning);
	ClassDB::bind_method(D_METHOD("is_software_skinning_enabled"), &MeshInstance::is_software_skinning_enabled);

	ClassDB::bind_method(D_METHOD("create_trimesh_collision"), &MeshInstance::create_trimesh_collision);
	ClassDB::set_method_flags("MeshInstance", "create_trimesh_collision", METHOD_FLAGS_DEFAULT);
	ClassDB::bind_method(D_METHOD("create_convex_collision"), &MeshInstance::create_convex_collision);
	ClassDB::set_method_flags("MeshInstance", "create_convex_collision", METHOD_FLAGS_DEFAULT);
	ClassDB::bind_method(D_METHOD("_mesh_changed"), &MeshInstance::_mesh_changed);
	ClassDB::bind_method(D_METHOD("_update_skinning"), &MeshInstance::_update_skinning);

	ClassDB::bind_method(D_METHOD("create_debug_tangents"), &MeshInstance::create_debug_tangents);
	ClassDB::set_method_flags("MeshInstance", "create_debug_tangents", METHOD_FLAGS_DEFAULT | METHOD_FLAG_EDITOR);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_mesh", "get_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "skin", PROPERTY_HINT_RESOURCE_TYPE, "Skin"), "set_skin", "get_skin");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "skeleton", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Skeleton"), "set_skeleton_path", "get_skeleton_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "software_skinning", PROPERTY_HINT_NONE), "set_software_skinning", "is_software_skinning_enabled");
}

MeshInstance::MeshInstance() {
	skeleton_path = NodePath("..");
	software_skinning = false;
}

MeshInstance::~MeshInstance() {
}
