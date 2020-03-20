/*************************************************************************/
/*  rasterizer_canvas_gles2.h                                            */
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

#ifndef RASTERIZERCANVASGLES2_H
#define RASTERIZERCANVASGLES2_H

#include "rasterizer_canvas_base_gles2.h"

class RasterizerSceneGLES2;

class RasterizerCanvasGLES2 : public RasterizerCanvasBaseGLES2 {
public:

	// pod versions of vector and color and RID, need to be 32 bit for vertex format
	struct BatchVector2 {
		float x, y;
		void set(const Vector2 &o) {
			x = o.x;
			y = o.y;
		}
		void to(Vector2 &o) const {
			o.x = x;
			o.y = y;
		}
	};

	struct BatchColor {
		float r, g, b, a;
		void set(const Color &c) {
			r = c.r;
			g = c.g;
			b = c.b;
			a = c.a;
		}
		bool equals(const Color &c) const {
			return (r == c.r) && (g == c.g) && (b == c.b) && (a == c.a);
		}
		const float *get_data() const { return &r; }
	};

	struct BatchVertex {
		// must be 32 bit pod
		BatchVector2 pos;
		BatchVector2 uv;
	};

	struct BatchVertexColored : public BatchVertex {
		// must be 32 bit pod
		BatchColor col;
	};

	struct Batch {
		enum CommandType : uint32_t {
			BT_DEFAULT,
			BT_RECT,
			BT_CHANGE_ITEM,
		};

		CommandType type;
		uint32_t first_command; // also item reference number
		uint32_t num_commands;
		uint32_t first_quad;
		uint32_t batch_texture_id;
		BatchColor color;
	};

	struct BatchTex {
		enum TileMode : uint32_t {
			TILE_OFF,
			TILE_NORMAL,
			TILE_FORCE_REPEAT,
		};
		RID RID_texture;
		RID RID_normal;
		TileMode tile_mode;
		BatchVector2 tex_pixel_size;
	};

	// batch item may represent 1 or more items
	struct BItemJoined {
		uint32_t first_item_ref;
		uint32_t num_item_refs;
	};

	struct BItemRef
	{
		Item * m_pItem;
	};

	struct BatchData {
		GLuint gl_vertex_buffer;
		GLuint gl_index_buffer;

		uint32_t max_quads;
		uint32_t vertex_buffer_size_units;
		uint32_t vertex_buffer_size_bytes;
		uint32_t index_buffer_size_units;
		uint32_t index_buffer_size_bytes;

		RasterizerArray<BatchVertex> vertices;
		RasterizerArray<BatchVertexColored> vertices_colored;
		RasterizerArray<Batch> batches;
		RasterizerArray<Batch> batches_temp; // used for translating to colored vertex batches
		RasterizerArray_non_pod<BatchTex> batch_textures; // the only reason this is non-POD is because of RIDs

		bool use_colored_vertices;
		bool use_batching;

		RasterizerArray<BItemJoined> items_joined;
		RasterizerArray<BItemRef> item_refs;

		// counts
		int total_quads;

		// we keep a record of how many color changes caused new batches
		// if the colors are causing an excessive number of batches, we switch
		// to alternate batching method and add color to the vertex format.
		int total_color_changes;
	} bdata;

	struct RIState
	{
		RIState() {Reset();}
		void Reset()
		{
			current_clip = NULL;
			shader_cache = NULL;
			rebind_shader = true;
			prev_use_skeleton = false;
			last_blend_mode = -1;
			canvas_last_material = RID();
		}
		Item *current_clip;
		RasterizerStorageGLES2::Shader *shader_cache;
		bool rebind_shader;
		bool prev_use_skeleton;
		int last_blend_mode;
		RID canvas_last_material;

		// item group
		int IG_z;
		Color IG_modulate;
		Light * IG_light;
		Transform2D IG_base_transform;
	};





	virtual void canvas_render_items(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform);
	void canvas_render_items_implementation(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform);
	void _canvas_render_item(Item * ci, RIState &ris);
	void _canvas_render_joined_item(const BItemJoined &bij, RIState &ris);

	void join_items(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform);
	bool _try_join_item(Item * ci, RIState &ris);

	_FORCE_INLINE_ void _canvas_item_render_commands(Item *p_item, Item *current_clip, bool &reclip, RasterizerStorageGLES2::Material *p_material);
	void _canvas_joined_item_render_commands(const BItemJoined &bij, Item *current_clip, bool &reclip, RasterizerStorageGLES2::Material *p_material);

	void _render_batches(Item::Command * const *commands, int first_item_ref_id, Item *current_clip, bool &reclip, RasterizerStorageGLES2::Material *p_material);


	bool _batch_canvas_joined_item_prefill(int &r_command_start, Item *p_item, Item *current_clip, bool &reclip, RasterizerStorageGLES2::Material *p_material);
	void _flush_render_batches(Item *p_item, Item *current_clip, bool &reclip, RasterizerStorageGLES2::Material *p_material);


	int _batch_canvas_item_prefill(int p_command_start, Item *p_item, Item *current_clip, bool &reclip, RasterizerStorageGLES2::Material *p_material);
	void _batch_translate_to_colored();
	_FORCE_INLINE_ int _batch_find_or_create_tex(const RID &p_texture, const RID &p_normal, bool p_tile, int p_previous_match);
	RasterizerStorageGLES2::Texture *_get_canvas_texture(const RID &p_texture) const;
	void _batch_upload_buffers();
	void _batch_render_rects(const Batch &batch, RasterizerStorageGLES2::Material *p_material);
	BatchVertex *_batch_vert_request_new() { return bdata.vertices.request(); }
	Batch *_batch_request_new(bool p_blank = true);



	void initialize();


	RasterizerCanvasGLES2();
};

#endif // RASTERIZERCANVASGLES2_H
