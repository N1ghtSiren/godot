#include "rasterizer_canvas_gles3.h"
#include "servers/visual/visual_server_raster.h"

static const GLenum gl_primitive[] = {
	GL_POINTS,
	GL_LINES,
	GL_LINE_STRIP,
	GL_LINE_LOOP,
	GL_TRIANGLES,
	GL_TRIANGLE_STRIP,
	GL_TRIANGLE_FAN
};


void RasterizerCanvasGLES3::canvas_end() {
	batch_canvas_end();
	RasterizerCanvasBaseGLES3::canvas_end();
}

void RasterizerCanvasGLES3::canvas_begin() {
	batch_canvas_begin();
	RasterizerCanvasBaseGLES3::canvas_begin();
}

void RasterizerCanvasGLES3::canvas_render_items_begin(const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) {
	batch_canvas_render_items_begin(p_modulate, p_light, p_base_transform);
}

void RasterizerCanvasGLES3::canvas_render_items_end() {
	batch_canvas_render_items_end();
}

void RasterizerCanvasGLES3::canvas_render_items(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) {
	batch_canvas_render_items(p_item_list, p_z, p_modulate, p_light, p_base_transform);
}

void RasterizerCanvasGLES3::gl_enable_scissor(int p_x, int p_y, int p_width, int p_height) const {
	glEnable(GL_SCISSOR_TEST);
	glScissor(p_x, p_y, p_width, p_height);
}

void RasterizerCanvasGLES3::gl_disable_scissor() const {
	glDisable(GL_SCISSOR_TEST);
}

// Legacy non-batched implementation for regression testing.
// Should be removed after testing phase to avoid duplicate codepaths.
void RasterizerCanvasGLES3::_canvas_render_item(Item *p_ci, RenderItemState &r_ris) {
	storage->info.render._2d_item_count++;

	if (r_ris.prev_distance_field != p_ci->distance_field) {

		state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_DISTANCE_FIELD, p_ci->distance_field);
		r_ris.prev_distance_field = p_ci->distance_field;
		r_ris.rebind_shader = true;
	}

	if (r_ris.current_clip != p_ci->final_clip_owner) {

		r_ris.current_clip = p_ci->final_clip_owner;

		//setup clip
		if (r_ris.current_clip) {

			glEnable(GL_SCISSOR_TEST);
			int y = storage->frame.current_rt->height - (r_ris.current_clip->final_clip_rect.position.y + r_ris.current_clip->final_clip_rect.size.y);
			if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_VFLIP])
				y = r_ris.current_clip->final_clip_rect.position.y;

			glScissor(r_ris.current_clip->final_clip_rect.position.x, y, r_ris.current_clip->final_clip_rect.size.x, r_ris.current_clip->final_clip_rect.size.y);

		} else {

			glDisable(GL_SCISSOR_TEST);
		}
	}

	if (p_ci->copy_back_buffer) {

		if (p_ci->copy_back_buffer->full) {

			_copy_texscreen(Rect2());
		} else {
			_copy_texscreen(p_ci->copy_back_buffer->rect);
		}
	}

	RasterizerStorageGLES3::Skeleton *skeleton = NULL;

	{
		//skeleton handling
		if (p_ci->skeleton.is_valid() && storage->skeleton_owner.owns(p_ci->skeleton)) {
			skeleton = storage->skeleton_owner.get(p_ci->skeleton);
			if (!skeleton->use_2d) {
				skeleton = NULL;
			} else {
				state.skeleton_transform = r_ris.item_group_base_transform * skeleton->base_transform_2d;
				state.skeleton_transform_inverse = state.skeleton_transform.affine_inverse();
			}
		}

		bool use_skeleton = skeleton != NULL;
		if (r_ris.prev_use_skeleton != use_skeleton) {
			r_ris.rebind_shader = true;
			state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SKELETON, use_skeleton);
			r_ris.prev_use_skeleton = use_skeleton;
		}

		if (skeleton) {
			glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 4);
			glBindTexture(GL_TEXTURE_2D, skeleton->texture);
			state.using_skeleton = true;
		} else {
			state.using_skeleton = false;
		}
	}

	//begin rect
	Item *material_owner = p_ci->material_owner ? p_ci->material_owner : p_ci;

	RID material = material_owner->material;

	if (material != r_ris.canvas_last_material || r_ris.rebind_shader) {

		RasterizerStorageGLES3::Material *material_ptr = storage->material_owner.getornull(material);
		RasterizerStorageGLES3::Shader *shader_ptr = NULL;

		if (material_ptr) {

			shader_ptr = material_ptr->shader;

			if (shader_ptr && shader_ptr->mode != VS::SHADER_CANVAS_ITEM) {
				shader_ptr = NULL; //do not use non canvasitem shader
			}
		}

		if (shader_ptr) {

			if (shader_ptr->canvas_item.uses_screen_texture && !state.canvas_texscreen_used) {
				//copy if not copied before
				_copy_texscreen(Rect2());

				// blend mode will have been enabled so make sure we disable it again later on
				r_ris.last_blend_mode = r_ris.last_blend_mode != RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED ? r_ris.last_blend_mode : -1;
			}

			if (shader_ptr != r_ris.shader_cache || r_ris.rebind_shader) {

				if (shader_ptr->canvas_item.uses_time) {
					VisualServerRaster::redraw_request();
				}

				state.canvas_shader.set_custom_shader(shader_ptr->custom_code_id);
				state.canvas_shader.bind();
			}

			if (material_ptr->ubo_id) {
				glBindBufferBase(GL_UNIFORM_BUFFER, 2, material_ptr->ubo_id);
			}

			int tc = material_ptr->textures.size();
			RID *textures = material_ptr->textures.ptrw();
			ShaderLanguage::ShaderNode::Uniform::Hint *texture_hints = shader_ptr->texture_hints.ptrw();

			for (int i = 0; i < tc; i++) {

				glActiveTexture(GL_TEXTURE2 + i);

				RasterizerStorageGLES3::Texture *t = storage->texture_owner.getornull(textures[i]);
				if (!t) {

					switch (texture_hints[i]) {
						case ShaderLanguage::ShaderNode::Uniform::HINT_BLACK_ALBEDO:
						case ShaderLanguage::ShaderNode::Uniform::HINT_BLACK: {
							glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);
						} break;
						case ShaderLanguage::ShaderNode::Uniform::HINT_ANISO: {
							glBindTexture(GL_TEXTURE_2D, storage->resources.aniso_tex);
						} break;
						case ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL: {
							glBindTexture(GL_TEXTURE_2D, storage->resources.normal_tex);
						} break;
						default: {
							glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);
						} break;
					}

					//check hints

					continue;
				}

				if (t->redraw_if_visible) { //check before proxy, because this is usually used with proxies
					VisualServerRaster::redraw_request();
				}

				t = t->get_ptr();

				if (storage->config.srgb_decode_supported && t->using_srgb) {
					//no srgb in 2D
					glTexParameteri(t->target, _TEXTURE_SRGB_DECODE_EXT, _SKIP_DECODE_EXT);
					t->using_srgb = false;
				}

				glBindTexture(t->target, t->tex_id);
			}

		} else {
			state.canvas_shader.set_custom_shader(0);
			state.canvas_shader.bind();
		}

		r_ris.shader_cache = shader_ptr;

		r_ris.canvas_last_material = material;
		r_ris.rebind_shader = false;
	}

	int blend_mode = r_ris.shader_cache ? r_ris.shader_cache->canvas_item.blend_mode : RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX;
	if (blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED && (!storage->frame.current_rt || !storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT])) {
		blend_mode = RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX;
	}
	bool unshaded = r_ris.shader_cache && (r_ris.shader_cache->canvas_item.light_mode == RasterizerStorageGLES3::Shader::CanvasItem::LIGHT_MODE_UNSHADED || (blend_mode != RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX && blend_mode != RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_PMALPHA));
	bool reclip = false;

	if (r_ris.last_blend_mode != blend_mode) {
		if (r_ris.last_blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED) {
			// re-enable it
			glEnable(GL_BLEND);
		} else if (blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED) {
			// disable it
			glDisable(GL_BLEND);
		}

		switch (blend_mode) {

			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED: {

				// nothing to do here

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX: {

				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
				}

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_ADD: {

				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
				}

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_SUB: {

				glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
				}
			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MUL: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO);
				} else {
					glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
				}

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_PMALPHA: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				} else {
					glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
				}

			} break;
		}

		r_ris.last_blend_mode = blend_mode;
	}

	state.canvas_item_modulate = unshaded ? p_ci->final_modulate : Color(p_ci->final_modulate.r * r_ris.item_group_modulate.r, p_ci->final_modulate.g * r_ris.item_group_modulate.g, p_ci->final_modulate.b * r_ris.item_group_modulate.b, p_ci->final_modulate.a * r_ris.item_group_modulate.a);

	state.final_transform = p_ci->final_transform;
	state.extra_matrix = Transform2D();

	if (state.using_skeleton) {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM, state.skeleton_transform);
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM_INVERSE, state.skeleton_transform_inverse);
	}

	state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE, state.canvas_item_modulate);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX, state.extra_matrix);
	if (storage->frame.current_rt) {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0 / storage->frame.current_rt->width, 1.0 / storage->frame.current_rt->height));
	} else {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0, 1.0));
	}
	if (unshaded || (state.canvas_item_modulate.a > 0.001 && (!r_ris.shader_cache || r_ris.shader_cache->canvas_item.light_mode != RasterizerStorageGLES3::Shader::CanvasItem::LIGHT_MODE_LIGHT_ONLY) && !p_ci->light_masked))
		_canvas_item_render_commands(p_ci, r_ris.current_clip, reclip, nullptr);

	if ((blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX || blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_PMALPHA) && r_ris.item_group_light && !unshaded) {

		Light *light = r_ris.item_group_light;
		bool light_used = false;
		VS::CanvasLightMode mode = VS::CANVAS_LIGHT_MODE_ADD;
		state.canvas_item_modulate = p_ci->final_modulate; // remove the canvas modulate

		while (light) {

			if (p_ci->light_mask & light->item_mask && r_ris.item_group_z >= light->z_min && r_ris.item_group_z <= light->z_max && p_ci->global_rect_cache.intersects_transformed(light->xform_cache, light->rect_cache)) {

				//intersects this light

				if (!light_used || mode != light->mode) {

					mode = light->mode;

					switch (mode) {

						case VS::CANVAS_LIGHT_MODE_ADD: {
							glBlendEquation(GL_FUNC_ADD);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE);

						} break;
						case VS::CANVAS_LIGHT_MODE_SUB: {
							glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE);
						} break;
						case VS::CANVAS_LIGHT_MODE_MIX:
						case VS::CANVAS_LIGHT_MODE_MASK: {
							glBlendEquation(GL_FUNC_ADD);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

						} break;
					}
				}

				if (!light_used) {

					state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_LIGHTING, true);
					light_used = true;
				}

				bool has_shadow = light->shadow_buffer.is_valid() && p_ci->light_mask & light->item_shadow_mask;

				state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SHADOWS, has_shadow);
				if (has_shadow) {
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_USE_GRADIENT, light->shadow_gradient_length > 0);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_NEAREST, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_NONE);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF3, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF3);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF5, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF5);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF7, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF7);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF9, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF9);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF13, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF13);
				}

				bool light_rebind = state.canvas_shader.bind();

				if (light_rebind) {
					state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE, state.canvas_item_modulate);
					state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform);
					state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX, Transform2D());
					if (storage->frame.current_rt) {
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0 / storage->frame.current_rt->width, 1.0 / storage->frame.current_rt->height));
					} else {
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0, 1.0));
					}
					if (state.using_skeleton) {
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM, state.skeleton_transform);
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM_INVERSE, state.skeleton_transform_inverse);
					}
				}

				glBindBufferBase(GL_UNIFORM_BUFFER, 1, static_cast<LightInternal *>(light->light_internal.get_data())->ubo);

				if (has_shadow) {

					RasterizerStorageGLES3::CanvasLightShadow *cls = storage->canvas_light_shadow_owner.get(light->shadow_buffer);
					glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 2);
					glBindTexture(GL_TEXTURE_2D, cls->distance);

					/*canvas_shader.set_uniform(CanvasShaderGLES3::SHADOW_MATRIX,light->shadow_matrix_cache);
					canvas_shader.set_uniform(CanvasShaderGLES3::SHADOW_ESM_MULTIPLIER,light->shadow_esm_mult);
					canvas_shader.set_uniform(CanvasShaderGLES3::LIGHT_SHADOW_COLOR,light->shadow_color);*/
				}

				glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 1);
				RasterizerStorageGLES3::Texture *t = storage->texture_owner.getornull(light->texture);
				if (!t) {
					glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);
				} else {
					t = t->get_ptr();

					glBindTexture(t->target, t->tex_id);
				}

				glActiveTexture(GL_TEXTURE0);
				_canvas_item_render_commands(p_ci, r_ris.current_clip, reclip, nullptr); //redraw using light
			}

			light = light->next_ptr;
		}

		if (light_used) {

			state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_LIGHTING, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SHADOWS, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_NEAREST, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF3, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF5, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF7, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF9, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF13, false);

			state.canvas_shader.bind();

			r_ris.last_blend_mode = -1;

			/*
			//this is set again, so it should not be needed anyway?
			state.canvas_item_modulate = unshaded ? ci->final_modulate : Color(
						ci->final_modulate.r * p_modulate.r,
						ci->final_modulate.g * p_modulate.g,
						ci->final_modulate.b * p_modulate.b,
						ci->final_modulate.a * p_modulate.a );


			state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX,state.final_transform);
			state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX,Transform2D());
			state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE,state.canvas_item_modulate);

			glBlendEquation(GL_FUNC_ADD);

			if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			} else {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			//@TODO RESET canvas_blend_mode
			*/
		}
	}

	if (reclip) {

		glEnable(GL_SCISSOR_TEST);
		int y = storage->frame.current_rt->height - (r_ris.current_clip->final_clip_rect.position.y + r_ris.current_clip->final_clip_rect.size.y);
		if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_VFLIP])
			y = r_ris.current_clip->final_clip_rect.position.y;
		glScissor(r_ris.current_clip->final_clip_rect.position.x, y, r_ris.current_clip->final_clip_rect.size.width, r_ris.current_clip->final_clip_rect.size.height);
	}
}

void RasterizerCanvasGLES3::render_batches(Item::Command *const *p_commands, Item *p_current_clip, bool &r_reclip, RasterizerStorageGLES3::Material *p_material) {
//	bdata.reset_flush();
//	return;

	int num_batches = bdata.batches.size();

	for (int batch_num = 0; batch_num < num_batches; batch_num++) {
		const Batch &batch = bdata.batches[batch_num];

		switch (batch.type) {
			case Batch::BT_RECT: {
				_batch_render_rects_test(batch, p_material);
			} break;
			default: {
				int end_command = batch.first_command + batch.num_commands;

				for (int i = batch.first_command; i < end_command; i++) {

					Item::Command *c = p_commands[i];

					switch (c->type) {

							//						case Item::Command::TYPE_LINE: {

							///////////////////////////
						case Item::Command::TYPE_LINE: {

							Item::CommandLine *line = static_cast<Item::CommandLine *>(c);
							_set_texture_rect_mode(false);

							_bind_canvas_texture(RID(), RID());

							glVertexAttrib4f(VS::ARRAY_COLOR, line->color.r, line->color.g, line->color.b, line->color.a);

							if (line->width <= 1) {
								Vector2 verts[2] = {
									Vector2(line->from.x, line->from.y),
									Vector2(line->to.x, line->to.y)
								};

#ifdef GLES_OVER_GL
								if (line->antialiased)
									glEnable(GL_LINE_SMOOTH);
#endif
								//glLineWidth(line->width);
								_draw_gui_primitive(2, verts, NULL, NULL);

#ifdef GLES_OVER_GL
								if (line->antialiased)
									glDisable(GL_LINE_SMOOTH);
#endif
							} else {
								//thicker line

								Vector2 t = (line->from - line->to).normalized().tangent() * line->width * 0.5;

								Vector2 verts[4] = {
									line->from - t,
									line->from + t,
									line->to + t,
									line->to - t,
								};

								//glLineWidth(line->width);
								_draw_gui_primitive(4, verts, NULL, NULL);
#ifdef GLES_OVER_GL
								if (line->antialiased) {
									glEnable(GL_LINE_SMOOTH);
									for (int j = 0; j < 4; j++) {
										Vector2 vertsl[2] = {
											verts[j],
											verts[(j + 1) % 4],
										};
										_draw_gui_primitive(2, vertsl, NULL, NULL);
									}
									glDisable(GL_LINE_SMOOTH);
								}
#endif
							}
						} break;
						case Item::Command::TYPE_POLYLINE: {

							Item::CommandPolyLine *pline = static_cast<Item::CommandPolyLine *>(c);
							_set_texture_rect_mode(false);

							_bind_canvas_texture(RID(), RID());

							if (pline->triangles.size()) {

								_draw_generic(GL_TRIANGLE_STRIP, pline->triangles.size(), pline->triangles.ptr(), NULL, pline->triangle_colors.ptr(), pline->triangle_colors.size() == 1);
#ifdef GLES_OVER_GL
								glEnable(GL_LINE_SMOOTH);
								if (pline->multiline) {
									//needs to be different
								} else {
									_draw_generic(GL_LINE_LOOP, pline->lines.size(), pline->lines.ptr(), NULL, pline->line_colors.ptr(), pline->line_colors.size() == 1);
								}
								glDisable(GL_LINE_SMOOTH);
#endif
							} else {

#ifdef GLES_OVER_GL
								if (pline->antialiased)
									glEnable(GL_LINE_SMOOTH);
#endif

								if (pline->multiline) {
									int todo = pline->lines.size() / 2;
									int max_per_call = data.polygon_buffer_size / (sizeof(real_t) * 4);
									int offset = 0;

									while (todo) {
										int to_draw = MIN(max_per_call, todo);
										_draw_generic(GL_LINES, to_draw * 2, &pline->lines.ptr()[offset], NULL, pline->line_colors.size() == 1 ? pline->line_colors.ptr() : &pline->line_colors.ptr()[offset], pline->line_colors.size() == 1);
										todo -= to_draw;
										offset += to_draw * 2;
									}

								} else {

									_draw_generic(GL_LINE_STRIP, pline->lines.size(), pline->lines.ptr(), NULL, pline->line_colors.ptr(), pline->line_colors.size() == 1);
								}

#ifdef GLES_OVER_GL
								if (pline->antialiased)
									glDisable(GL_LINE_SMOOTH);
#endif
							}

						} break;
						case Item::Command::TYPE_RECT: {

							Item::CommandRect *rect = static_cast<Item::CommandRect *>(c);

							//set color
							glVertexAttrib4f(VS::ARRAY_COLOR, rect->modulate.r, rect->modulate.g, rect->modulate.b, rect->modulate.a);

							RasterizerStorageGLES3::Texture *texture = _bind_canvas_texture(rect->texture, rect->normal_map);

							if (use_nvidia_rect_workaround) {
								render_rect_nvidia_workaround(rect, texture);
							} else {

								_set_texture_rect_mode(true);

								if (texture) {

									bool untile = false;

									if (rect->flags & CANVAS_RECT_TILE && !(texture->flags & VS::TEXTURE_FLAG_REPEAT)) {
										glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
										glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
										untile = true;
									}

									Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
									Rect2 src_rect = (rect->flags & CANVAS_RECT_REGION) ? Rect2(rect->source.position * texpixel_size, rect->source.size * texpixel_size) : Rect2(0, 0, 1, 1);
									Rect2 dst_rect = Rect2(rect->rect.position, rect->rect.size);

									if (dst_rect.size.width < 0) {
										dst_rect.position.x += dst_rect.size.width;
										dst_rect.size.width *= -1;
									}
									if (dst_rect.size.height < 0) {
										dst_rect.position.y += dst_rect.size.height;
										dst_rect.size.height *= -1;
									}

									if (rect->flags & CANVAS_RECT_FLIP_H) {
										src_rect.size.x *= -1;
									}

									if (rect->flags & CANVAS_RECT_FLIP_V) {
										src_rect.size.y *= -1;
									}

									if (rect->flags & CANVAS_RECT_TRANSPOSE) {
										dst_rect.size.x *= -1; // Encoding in the dst_rect.z uniform
									}

									state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, texpixel_size);

									state.canvas_shader.set_uniform(CanvasShaderGLES3::DST_RECT, Color(dst_rect.position.x, dst_rect.position.y, dst_rect.size.x, dst_rect.size.y));
									state.canvas_shader.set_uniform(CanvasShaderGLES3::SRC_RECT, Color(src_rect.position.x, src_rect.position.y, src_rect.size.x, src_rect.size.y));
									state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, rect->flags & CANVAS_RECT_CLIP_UV);

									glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
									storage->info.render._2d_draw_call_count++;

									if (untile) {
										glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
										glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
									}

								} else {
									Rect2 dst_rect = Rect2(rect->rect.position, rect->rect.size);

									if (dst_rect.size.width < 0) {
										dst_rect.position.x += dst_rect.size.width;
										dst_rect.size.width *= -1;
									}
									if (dst_rect.size.height < 0) {
										dst_rect.position.y += dst_rect.size.height;
										dst_rect.size.height *= -1;
									}

									state.canvas_shader.set_uniform(CanvasShaderGLES3::DST_RECT, Color(dst_rect.position.x, dst_rect.position.y, dst_rect.size.x, dst_rect.size.y));
									state.canvas_shader.set_uniform(CanvasShaderGLES3::SRC_RECT, Color(0, 0, 1, 1));
									state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, false);
									glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
									storage->info.render._2d_draw_call_count++;
								}
							} // if not use nvidia workaround
						} break;
						case Item::Command::TYPE_NINEPATCH: {

							Item::CommandNinePatch *np = static_cast<Item::CommandNinePatch *>(c);

							_set_texture_rect_mode(true, true);

							glVertexAttrib4f(VS::ARRAY_COLOR, np->color.r, np->color.g, np->color.b, np->color.a);

							RasterizerStorageGLES3::Texture *texture = _bind_canvas_texture(np->texture, np->normal_map);

							Size2 texpixel_size;

							if (!texture) {

								texpixel_size = Size2(1, 1);

								state.canvas_shader.set_uniform(CanvasShaderGLES3::SRC_RECT, Color(0, 0, 1, 1));

							} else {

								if (np->source != Rect2()) {
									texpixel_size = Size2(1.0 / np->source.size.width, 1.0 / np->source.size.height);
									state.canvas_shader.set_uniform(CanvasShaderGLES3::SRC_RECT, Color(np->source.position.x / texture->width, np->source.position.y / texture->height, np->source.size.x / texture->width, np->source.size.y / texture->height));
								} else {
									texpixel_size = Size2(1.0 / texture->width, 1.0 / texture->height);
									state.canvas_shader.set_uniform(CanvasShaderGLES3::SRC_RECT, Color(0, 0, 1, 1));
								}
							}

							state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, texpixel_size);
							state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, false);
							state.canvas_shader.set_uniform(CanvasShaderGLES3::NP_REPEAT_H, int(np->axis_x));
							state.canvas_shader.set_uniform(CanvasShaderGLES3::NP_REPEAT_V, int(np->axis_y));
							state.canvas_shader.set_uniform(CanvasShaderGLES3::NP_DRAW_CENTER, np->draw_center);
							state.canvas_shader.set_uniform(CanvasShaderGLES3::NP_MARGINS, Color(np->margin[MARGIN_LEFT], np->margin[MARGIN_TOP], np->margin[MARGIN_RIGHT], np->margin[MARGIN_BOTTOM]));
							state.canvas_shader.set_uniform(CanvasShaderGLES3::DST_RECT, Color(np->rect.position.x, np->rect.position.y, np->rect.size.x, np->rect.size.y));

							glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

							storage->info.render._2d_draw_call_count++;
						} break;
						case Item::Command::TYPE_PRIMITIVE: {

							Item::CommandPrimitive *primitive = static_cast<Item::CommandPrimitive *>(c);
							_set_texture_rect_mode(false);

							ERR_CONTINUE(primitive->points.size() < 1);

							RasterizerStorageGLES3::Texture *texture = _bind_canvas_texture(primitive->texture, primitive->normal_map);

							if (texture) {
								Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
								state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, texpixel_size);
							}
							if (primitive->colors.size() == 1 && primitive->points.size() > 1) {

								Color col = primitive->colors[0];
								glVertexAttrib4f(VS::ARRAY_COLOR, col.r, col.g, col.b, col.a);

							} else if (primitive->colors.empty()) {
								glVertexAttrib4f(VS::ARRAY_COLOR, 1, 1, 1, 1);
							}

							_draw_gui_primitive(primitive->points.size(), primitive->points.ptr(), primitive->colors.ptr(), primitive->uvs.ptr());

						} break;
						case Item::Command::TYPE_POLYGON: {

							Item::CommandPolygon *polygon = static_cast<Item::CommandPolygon *>(c);
							_set_texture_rect_mode(false);

							RasterizerStorageGLES3::Texture *texture = _bind_canvas_texture(polygon->texture, polygon->normal_map);

							if (texture) {
								Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
								state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, texpixel_size);
							}

							_draw_polygon(polygon->indices.ptr(), polygon->count, polygon->points.size(), polygon->points.ptr(), polygon->uvs.ptr(), polygon->colors.ptr(), polygon->colors.size() == 1, polygon->bones.ptr(), polygon->weights.ptr());
#ifdef GLES_OVER_GL
							if (polygon->antialiased) {
								glEnable(GL_LINE_SMOOTH);
								if (polygon->antialiasing_use_indices) {
									_draw_generic_indices(GL_LINE_STRIP, polygon->indices.ptr(), polygon->count, polygon->points.size(), polygon->points.ptr(), polygon->uvs.ptr(), polygon->colors.ptr(), polygon->colors.size() == 1);
								} else {
									_draw_generic(GL_LINE_LOOP, polygon->points.size(), polygon->points.ptr(), polygon->uvs.ptr(), polygon->colors.ptr(), polygon->colors.size() == 1);
								}
								glDisable(GL_LINE_SMOOTH);
							}
#endif

						} break;
						case Item::Command::TYPE_MESH: {

							Item::CommandMesh *mesh = static_cast<Item::CommandMesh *>(c);
							_set_texture_rect_mode(false);

							RasterizerStorageGLES3::Texture *texture = _bind_canvas_texture(mesh->texture, mesh->normal_map);

							if (texture) {
								Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
								state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, texpixel_size);
							}

							state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform * mesh->transform);

							RasterizerStorageGLES3::Mesh *mesh_data = storage->mesh_owner.getornull(mesh->mesh);
							if (mesh_data) {

								for (int j = 0; j < mesh_data->surfaces.size(); j++) {
									RasterizerStorageGLES3::Surface *s = mesh_data->surfaces[j];
									// materials are ignored in 2D meshes, could be added but many things (ie, lighting mode, reading from screen, etc) would break as they are not meant be set up at this point of drawing
									glBindVertexArray(s->array_id);

									glVertexAttrib4f(VS::ARRAY_COLOR, mesh->modulate.r, mesh->modulate.g, mesh->modulate.b, mesh->modulate.a);

									if (s->index_array_len) {
										glDrawElements(gl_primitive[s->primitive], s->index_array_len, (s->array_len >= (1 << 16)) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, 0);
									} else {
										glDrawArrays(gl_primitive[s->primitive], 0, s->array_len);
									}
									storage->info.render._2d_draw_call_count++;

									glBindVertexArray(0);
								}
							}
							state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform);

						} break;
						case Item::Command::TYPE_MULTIMESH: {

							Item::CommandMultiMesh *mmesh = static_cast<Item::CommandMultiMesh *>(c);

							RasterizerStorageGLES3::MultiMesh *multi_mesh = storage->multimesh_owner.getornull(mmesh->multimesh);

							if (!multi_mesh)
								break;

							RasterizerStorageGLES3::Mesh *mesh_data = storage->mesh_owner.getornull(multi_mesh->mesh);

							if (!mesh_data)
								break;

							RasterizerStorageGLES3::Texture *texture = _bind_canvas_texture(mmesh->texture, mmesh->normal_map);

							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCE_CUSTOM, multi_mesh->custom_data_format != VS::MULTIMESH_CUSTOM_DATA_NONE);
							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCING, true);
							//reset shader and force rebind
							state.using_texture_rect = true;
							_set_texture_rect_mode(false);

							if (texture) {
								Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
								state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, texpixel_size);
							}

							int amount = MIN(multi_mesh->size, multi_mesh->visible_instances);

							if (amount == -1) {
								amount = multi_mesh->size;
							}

							for (int j = 0; j < mesh_data->surfaces.size(); j++) {
								RasterizerStorageGLES3::Surface *s = mesh_data->surfaces[j];
								// materials are ignored in 2D meshes, could be added but many things (ie, lighting mode, reading from screen, etc) would break as they are not meant be set up at this point of drawing
								glBindVertexArray(s->instancing_array_id);

								glBindBuffer(GL_ARRAY_BUFFER, multi_mesh->buffer); //modify the buffer

								int stride = (multi_mesh->xform_floats + multi_mesh->color_floats + multi_mesh->custom_data_floats) * 4;
								glEnableVertexAttribArray(8);
								glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(0));
								glVertexAttribDivisor(8, 1);
								glEnableVertexAttribArray(9);
								glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(4 * 4));
								glVertexAttribDivisor(9, 1);

								int color_ofs;

								if (multi_mesh->transform_format == VS::MULTIMESH_TRANSFORM_3D) {
									glEnableVertexAttribArray(10);
									glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(8 * 4));
									glVertexAttribDivisor(10, 1);
									color_ofs = 12 * 4;
								} else {
									glDisableVertexAttribArray(10);
									glVertexAttrib4f(10, 0, 0, 1, 0);
									color_ofs = 8 * 4;
								}

								int custom_data_ofs = color_ofs;

								switch (multi_mesh->color_format) {

									case VS::MULTIMESH_COLOR_MAX:
									case VS::MULTIMESH_COLOR_NONE: {
										glDisableVertexAttribArray(11);
										glVertexAttrib4f(11, 1, 1, 1, 1);
									} break;
									case VS::MULTIMESH_COLOR_8BIT: {
										glEnableVertexAttribArray(11);
										glVertexAttribPointer(11, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, CAST_INT_TO_UCHAR_PTR(color_ofs));
										glVertexAttribDivisor(11, 1);
										custom_data_ofs += 4;

									} break;
									case VS::MULTIMESH_COLOR_FLOAT: {
										glEnableVertexAttribArray(11);
										glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(color_ofs));
										glVertexAttribDivisor(11, 1);
										custom_data_ofs += 4 * 4;
									} break;
								}

								switch (multi_mesh->custom_data_format) {

									case VS::MULTIMESH_CUSTOM_DATA_MAX:
									case VS::MULTIMESH_CUSTOM_DATA_NONE: {
										glDisableVertexAttribArray(12);
										glVertexAttrib4f(12, 1, 1, 1, 1);
									} break;
									case VS::MULTIMESH_CUSTOM_DATA_8BIT: {
										glEnableVertexAttribArray(12);
										glVertexAttribPointer(12, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, CAST_INT_TO_UCHAR_PTR(custom_data_ofs));
										glVertexAttribDivisor(12, 1);

									} break;
									case VS::MULTIMESH_CUSTOM_DATA_FLOAT: {
										glEnableVertexAttribArray(12);
										glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(custom_data_ofs));
										glVertexAttribDivisor(12, 1);
									} break;
								}

								if (s->index_array_len) {
									glDrawElementsInstanced(gl_primitive[s->primitive], s->index_array_len, (s->array_len >= (1 << 16)) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, 0, amount);
								} else {
									glDrawArraysInstanced(gl_primitive[s->primitive], 0, s->array_len, amount);
								}
								storage->info.render._2d_draw_call_count++;

								glBindVertexArray(0);
							}

							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCE_CUSTOM, false);
							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCING, false);
							state.using_texture_rect = true;
							_set_texture_rect_mode(false);

						} break;
						case Item::Command::TYPE_PARTICLES: {

							Item::CommandParticles *particles_cmd = static_cast<Item::CommandParticles *>(c);

							RasterizerStorageGLES3::Particles *particles = storage->particles_owner.getornull(particles_cmd->particles);
							if (!particles)
								break;

							if (particles->inactive && !particles->emitting)
								break;

							glVertexAttrib4f(VS::ARRAY_COLOR, 1, 1, 1, 1); //not used, so keep white

							VisualServerRaster::redraw_request();

							storage->particles_request_process(particles_cmd->particles);
							//enable instancing

							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCE_CUSTOM, true);
							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_PARTICLES, true);
							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCING, true);
							//reset shader and force rebind
							state.using_texture_rect = true;
							_set_texture_rect_mode(false);

							RasterizerStorageGLES3::Texture *texture = _bind_canvas_texture(particles_cmd->texture, particles_cmd->normal_map);

							if (texture) {
								Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
								state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, texpixel_size);
							} else {
								state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, Vector2(1.0, 1.0));
							}

							if (!particles->use_local_coords) {

								Transform2D inv_xf;
								inv_xf.set_axis(0, Vector2(particles->emission_transform.basis.get_axis(0).x, particles->emission_transform.basis.get_axis(0).y));
								inv_xf.set_axis(1, Vector2(particles->emission_transform.basis.get_axis(1).x, particles->emission_transform.basis.get_axis(1).y));
								inv_xf.set_origin(Vector2(particles->emission_transform.get_origin().x, particles->emission_transform.get_origin().y));
								inv_xf.affine_invert();

								state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform * inv_xf);
							}

							glBindVertexArray(data.particle_quad_array); //use particle quad array
							glBindBuffer(GL_ARRAY_BUFFER, particles->particle_buffers[0]); //bind particle buffer

							int stride = sizeof(float) * 4 * 6;

							int amount = particles->amount;

							if (particles->draw_order != VS::PARTICLES_DRAW_ORDER_LIFETIME) {

								glEnableVertexAttribArray(8); //xform x
								glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 3));
								glVertexAttribDivisor(8, 1);
								glEnableVertexAttribArray(9); //xform y
								glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 4));
								glVertexAttribDivisor(9, 1);
								glEnableVertexAttribArray(10); //xform z
								glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 5));
								glVertexAttribDivisor(10, 1);
								glEnableVertexAttribArray(11); //color
								glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, stride, NULL);
								glVertexAttribDivisor(11, 1);
								glEnableVertexAttribArray(12); //custom
								glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 2));
								glVertexAttribDivisor(12, 1);

								glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, amount);
								storage->info.render._2d_draw_call_count++;
							} else {
								//split
								int split = int(Math::ceil(particles->phase * particles->amount));

								if (amount - split > 0) {
									glEnableVertexAttribArray(8); //xform x
									glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + sizeof(float) * 4 * 3));
									glVertexAttribDivisor(8, 1);
									glEnableVertexAttribArray(9); //xform y
									glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + sizeof(float) * 4 * 4));
									glVertexAttribDivisor(9, 1);
									glEnableVertexAttribArray(10); //xform z
									glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + sizeof(float) * 4 * 5));
									glVertexAttribDivisor(10, 1);
									glEnableVertexAttribArray(11); //color
									glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + 0));
									glVertexAttribDivisor(11, 1);
									glEnableVertexAttribArray(12); //custom
									glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * split + sizeof(float) * 4 * 2));
									glVertexAttribDivisor(12, 1);

									glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, amount - split);
									storage->info.render._2d_draw_call_count++;
								}

								if (split > 0) {
									glEnableVertexAttribArray(8); //xform x
									glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 3));
									glVertexAttribDivisor(8, 1);
									glEnableVertexAttribArray(9); //xform y
									glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 4));
									glVertexAttribDivisor(9, 1);
									glEnableVertexAttribArray(10); //xform z
									glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 5));
									glVertexAttribDivisor(10, 1);
									glEnableVertexAttribArray(11); //color
									glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, stride, NULL);
									glVertexAttribDivisor(11, 1);
									glEnableVertexAttribArray(12); //custom
									glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * 2));
									glVertexAttribDivisor(12, 1);

									glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, split);
									storage->info.render._2d_draw_call_count++;
								}
							}

							glBindVertexArray(0);

							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCE_CUSTOM, false);
							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_PARTICLES, false);
							state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_INSTANCING, false);
							state.using_texture_rect = true;
							_set_texture_rect_mode(false);

						} break;
						case Item::Command::TYPE_CIRCLE: {

							_set_texture_rect_mode(false);

							Item::CommandCircle *circle = static_cast<Item::CommandCircle *>(c);
							static const int numpoints = 32;
							Vector2 points[numpoints + 1];
							points[numpoints] = circle->pos;
							int indices[numpoints * 3];

							for (int j = 0; j < numpoints; j++) {

								points[j] = circle->pos + Vector2(Math::sin(j * Math_PI * 2.0 / numpoints), Math::cos(j * Math_PI * 2.0 / numpoints)) * circle->radius;
								indices[j * 3 + 0] = j;
								indices[j * 3 + 1] = (j + 1) % numpoints;
								indices[j * 3 + 2] = numpoints;
							}

							_bind_canvas_texture(RID(), RID());
							_draw_polygon(indices, numpoints * 3, numpoints + 1, points, NULL, &circle->color, true, NULL, NULL);

							//_draw_polygon(numpoints*3,indices,points,NULL,&circle->color,RID(),true);
							//canvas_draw_circle(circle->indices.size(),circle->indices.ptr(),circle->points.ptr(),circle->uvs.ptr(),circle->colors.ptr(),circle->texture,circle->colors.size()==1);
						} break;
						case Item::Command::TYPE_TRANSFORM: {

							Item::CommandTransform *transform = static_cast<Item::CommandTransform *>(c);
							state.extra_matrix = transform->xform;
							state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX, state.extra_matrix);

						} break;
						case Item::Command::TYPE_CLIP_IGNORE: {

							Item::CommandClipIgnore *ci = static_cast<Item::CommandClipIgnore *>(c);
							if (p_current_clip) {

								if (ci->ignore != r_reclip) {
									if (ci->ignore) {

										glDisable(GL_SCISSOR_TEST);
										r_reclip = true;
									} else {

										glEnable(GL_SCISSOR_TEST);
										//glScissor(viewport.x+current_clip->final_clip_rect.pos.x,viewport.y+ (viewport.height-(current_clip->final_clip_rect.pos.y+current_clip->final_clip_rect.size.height)),
										//current_clip->final_clip_rect.size.width,current_clip->final_clip_rect.size.height);
										int y = storage->frame.current_rt->height - (p_current_clip->final_clip_rect.position.y + p_current_clip->final_clip_rect.size.y);
										if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_VFLIP])
											y = p_current_clip->final_clip_rect.position.y;

										glScissor(p_current_clip->final_clip_rect.position.x, y, p_current_clip->final_clip_rect.size.x, p_current_clip->final_clip_rect.size.y);

										r_reclip = false;
									}
								}
							}

						} break;

							//////////////////////////

							//					} break;

						default: {
							// FIXME: Proper error handling if relevant
							//print_line("other");
						} break;
					}
				}

			} // default
			break;
		}
	}

	// zero all the batch data ready for a new run
	bdata.reset_flush();
}

void RasterizerCanvasGLES3::render_joined_item(const BItemJoined &p_bij, RenderItemState &r_ris) {
	storage->info.render._2d_item_count++;

#ifdef DEBUG_ENABLED
	if (bdata.diagnose_frame) {
		bdata.frame_string += "\tjoined_item " + itos(p_bij.num_item_refs) + " refs\n";
		if (p_bij.z_index != 0) {
			bdata.frame_string += "\t\t(z " + itos(p_bij.z_index) + ")\n";
		}
	}
#endif

	// this must be reset for each joined item,
	// it only exists to prevent capturing the screen more than once per item
	state.canvas_texscreen_used = false;

	// all the joined items will share the same state with the first item
	Item *p_ci = bdata.item_refs[p_bij.first_item_ref].item;

	if (r_ris.prev_distance_field != p_ci->distance_field) {

		state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_DISTANCE_FIELD, p_ci->distance_field);
		r_ris.prev_distance_field = p_ci->distance_field;
		r_ris.rebind_shader = true;
	}

	if (r_ris.current_clip != p_ci->final_clip_owner) {

		r_ris.current_clip = p_ci->final_clip_owner;

		//setup clip
		if (r_ris.current_clip) {

			glEnable(GL_SCISSOR_TEST);
			int y = storage->frame.current_rt->height - (r_ris.current_clip->final_clip_rect.position.y + r_ris.current_clip->final_clip_rect.size.y);
			if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_VFLIP])
				y = r_ris.current_clip->final_clip_rect.position.y;

			glScissor(r_ris.current_clip->final_clip_rect.position.x, y, r_ris.current_clip->final_clip_rect.size.x, r_ris.current_clip->final_clip_rect.size.y);

		} else {

			glDisable(GL_SCISSOR_TEST);
		}
	}

	if (p_ci->copy_back_buffer) {

		if (p_ci->copy_back_buffer->full) {

			_copy_texscreen(Rect2());
		} else {
			_copy_texscreen(p_ci->copy_back_buffer->rect);
		}
	}

	RasterizerStorageGLES3::Skeleton *skeleton = NULL;

	{
		//skeleton handling
		if (p_ci->skeleton.is_valid() && storage->skeleton_owner.owns(p_ci->skeleton)) {
			skeleton = storage->skeleton_owner.get(p_ci->skeleton);
			if (!skeleton->use_2d) {
				skeleton = NULL;
			} else {
				state.skeleton_transform = r_ris.item_group_base_transform * skeleton->base_transform_2d;
				state.skeleton_transform_inverse = state.skeleton_transform.affine_inverse();
			}
		}

		bool use_skeleton = skeleton != NULL;
		if (r_ris.prev_use_skeleton != use_skeleton) {
			r_ris.rebind_shader = true;
			state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SKELETON, use_skeleton);
			r_ris.prev_use_skeleton = use_skeleton;
		}

		if (skeleton) {
			glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 4);
			glBindTexture(GL_TEXTURE_2D, skeleton->texture);
			state.using_skeleton = true;
		} else {
			state.using_skeleton = false;
		}
	}

	//begin rect
	Item *material_owner = p_ci->material_owner ? p_ci->material_owner : p_ci;

	RID material = material_owner->material;

	if (material != r_ris.canvas_last_material || r_ris.rebind_shader) {

		RasterizerStorageGLES3::Material *material_ptr = storage->material_owner.getornull(material);
		RasterizerStorageGLES3::Shader *shader_ptr = NULL;

		if (material_ptr) {

			shader_ptr = material_ptr->shader;

			if (shader_ptr && shader_ptr->mode != VS::SHADER_CANVAS_ITEM) {
				shader_ptr = NULL; //do not use non canvasitem shader
			}
		}

		if (shader_ptr) {

			if (shader_ptr->canvas_item.uses_screen_texture && !state.canvas_texscreen_used) {
				//copy if not copied before
				_copy_texscreen(Rect2());

				// blend mode will have been enabled so make sure we disable it again later on
				r_ris.last_blend_mode = r_ris.last_blend_mode != RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED ? r_ris.last_blend_mode : -1;
			}

			if (shader_ptr != r_ris.shader_cache || r_ris.rebind_shader) {

				if (shader_ptr->canvas_item.uses_time) {
					VisualServerRaster::redraw_request();
				}

				state.canvas_shader.set_custom_shader(shader_ptr->custom_code_id);
				state.canvas_shader.bind();
			}

			if (material_ptr->ubo_id) {
				glBindBufferBase(GL_UNIFORM_BUFFER, 2, material_ptr->ubo_id);
			}

			int tc = material_ptr->textures.size();
			RID *textures = material_ptr->textures.ptrw();
			ShaderLanguage::ShaderNode::Uniform::Hint *texture_hints = shader_ptr->texture_hints.ptrw();

			for (int i = 0; i < tc; i++) {

				glActiveTexture(GL_TEXTURE2 + i);

				RasterizerStorageGLES3::Texture *t = storage->texture_owner.getornull(textures[i]);
				if (!t) {

					switch (texture_hints[i]) {
						case ShaderLanguage::ShaderNode::Uniform::HINT_BLACK_ALBEDO:
						case ShaderLanguage::ShaderNode::Uniform::HINT_BLACK: {
							glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);
						} break;
						case ShaderLanguage::ShaderNode::Uniform::HINT_ANISO: {
							glBindTexture(GL_TEXTURE_2D, storage->resources.aniso_tex);
						} break;
						case ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL: {
							glBindTexture(GL_TEXTURE_2D, storage->resources.normal_tex);
						} break;
						default: {
							glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);
						} break;
					}

					//check hints

					continue;
				}

				if (t->redraw_if_visible) { //check before proxy, because this is usually used with proxies
					VisualServerRaster::redraw_request();
				}

				t = t->get_ptr();

				if (storage->config.srgb_decode_supported && t->using_srgb) {
					//no srgb in 2D
					glTexParameteri(t->target, _TEXTURE_SRGB_DECODE_EXT, _SKIP_DECODE_EXT);
					t->using_srgb = false;
				}

				glBindTexture(t->target, t->tex_id);
			}

		} else {
			state.canvas_shader.set_custom_shader(0);
			state.canvas_shader.bind();
		}

		r_ris.shader_cache = shader_ptr;

		r_ris.canvas_last_material = material;
		r_ris.rebind_shader = false;
	}

	int blend_mode = r_ris.shader_cache ? r_ris.shader_cache->canvas_item.blend_mode : RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX;
	if (blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED && (!storage->frame.current_rt || !storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT])) {
		blend_mode = RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX;
	}
	bool unshaded = r_ris.shader_cache && (r_ris.shader_cache->canvas_item.light_mode == RasterizerStorageGLES3::Shader::CanvasItem::LIGHT_MODE_UNSHADED || (blend_mode != RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX && blend_mode != RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_PMALPHA));
	bool reclip = false;

	if (r_ris.last_blend_mode != blend_mode) {
		if (r_ris.last_blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED) {
			// re-enable it
			glEnable(GL_BLEND);
		} else if (blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED) {
			// disable it
			glDisable(GL_BLEND);
		}

		switch (blend_mode) {

			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_DISABLED: {

				// nothing to do here

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX: {

				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
				}

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_ADD: {

				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
				}

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_SUB: {

				glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
				}
			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MUL: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO);
				} else {
					glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
				}

			} break;
			case RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_PMALPHA: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				} else {
					glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
				}

			} break;
		}

		r_ris.last_blend_mode = blend_mode;
	}

	state.canvas_item_modulate = unshaded ? p_ci->final_modulate : Color(p_ci->final_modulate.r * r_ris.item_group_modulate.r, p_ci->final_modulate.g * r_ris.item_group_modulate.g, p_ci->final_modulate.b * r_ris.item_group_modulate.b, p_ci->final_modulate.a * r_ris.item_group_modulate.a);

	state.final_transform = p_ci->final_transform;
	state.extra_matrix = Transform2D();

	if (state.using_skeleton) {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM, state.skeleton_transform);
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM_INVERSE, state.skeleton_transform_inverse);
	}

	state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE, state.canvas_item_modulate);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX, state.extra_matrix);
	if (storage->frame.current_rt) {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0 / storage->frame.current_rt->width, 1.0 / storage->frame.current_rt->height));
	} else {
		state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0, 1.0));
	}
	if (unshaded || (state.canvas_item_modulate.a > 0.001 && (!r_ris.shader_cache || r_ris.shader_cache->canvas_item.light_mode != RasterizerStorageGLES3::Shader::CanvasItem::LIGHT_MODE_LIGHT_ONLY) && !p_ci->light_masked))
	{
		RasterizerStorageGLES3::Material *material_ptr = nullptr;
		render_joined_item_commands(p_bij, NULL, reclip, material_ptr, false);
//		_canvas_item_render_commands(p_ci, r_ris.current_clip, reclip, nullptr);
	}

	if ((blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_MIX || blend_mode == RasterizerStorageGLES3::Shader::CanvasItem::BLEND_MODE_PMALPHA) && r_ris.item_group_light && !unshaded) {

		Light *light = r_ris.item_group_light;
		bool light_used = false;
		VS::CanvasLightMode mode = VS::CANVAS_LIGHT_MODE_ADD;
		state.canvas_item_modulate = p_ci->final_modulate; // remove the canvas modulate

		while (light) {

			if (p_ci->light_mask & light->item_mask && r_ris.item_group_z >= light->z_min && r_ris.item_group_z <= light->z_max && p_ci->global_rect_cache.intersects_transformed(light->xform_cache, light->rect_cache)) {

				//intersects this light

				if (!light_used || mode != light->mode) {

					mode = light->mode;

					switch (mode) {

						case VS::CANVAS_LIGHT_MODE_ADD: {
							glBlendEquation(GL_FUNC_ADD);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE);

						} break;
						case VS::CANVAS_LIGHT_MODE_SUB: {
							glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE);
						} break;
						case VS::CANVAS_LIGHT_MODE_MIX:
						case VS::CANVAS_LIGHT_MODE_MASK: {
							glBlendEquation(GL_FUNC_ADD);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

						} break;
					}
				}

				if (!light_used) {

					state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_LIGHTING, true);
					light_used = true;
				}

				bool has_shadow = light->shadow_buffer.is_valid() && p_ci->light_mask & light->item_shadow_mask;

				state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SHADOWS, has_shadow);
				if (has_shadow) {
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_USE_GRADIENT, light->shadow_gradient_length > 0);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_NEAREST, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_NONE);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF3, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF3);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF5, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF5);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF7, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF7);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF9, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF9);
					state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF13, light->shadow_filter == VS::CANVAS_LIGHT_FILTER_PCF13);
				}

				bool light_rebind = state.canvas_shader.bind();

				if (light_rebind) {
					state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE, state.canvas_item_modulate);
					state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX, state.final_transform);
					state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX, Transform2D());
					if (storage->frame.current_rt) {
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0 / storage->frame.current_rt->width, 1.0 / storage->frame.current_rt->height));
					} else {
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SCREEN_PIXEL_SIZE, Vector2(1.0, 1.0));
					}
					if (state.using_skeleton) {
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM, state.skeleton_transform);
						state.canvas_shader.set_uniform(CanvasShaderGLES3::SKELETON_TRANSFORM_INVERSE, state.skeleton_transform_inverse);
					}
				}

				glBindBufferBase(GL_UNIFORM_BUFFER, 1, static_cast<LightInternal *>(light->light_internal.get_data())->ubo);

				if (has_shadow) {

					RasterizerStorageGLES3::CanvasLightShadow *cls = storage->canvas_light_shadow_owner.get(light->shadow_buffer);
					glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 2);
					glBindTexture(GL_TEXTURE_2D, cls->distance);

					/*canvas_shader.set_uniform(CanvasShaderGLES3::SHADOW_MATRIX,light->shadow_matrix_cache);
					canvas_shader.set_uniform(CanvasShaderGLES3::SHADOW_ESM_MULTIPLIER,light->shadow_esm_mult);
					canvas_shader.set_uniform(CanvasShaderGLES3::LIGHT_SHADOW_COLOR,light->shadow_color);*/
				}

				glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 1);
				RasterizerStorageGLES3::Texture *t = storage->texture_owner.getornull(light->texture);
				if (!t) {
					glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);
				} else {
					t = t->get_ptr();

					glBindTexture(t->target, t->tex_id);
				}

				glActiveTexture(GL_TEXTURE0);
				_canvas_item_render_commands(p_ci, r_ris.current_clip, reclip, nullptr); //redraw using light
			}

			light = light->next_ptr;
		}

		if (light_used) {

			state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_LIGHTING, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SHADOWS, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_NEAREST, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF3, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF5, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF7, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF9, false);
			state.canvas_shader.set_conditional(CanvasShaderGLES3::SHADOW_FILTER_PCF13, false);

			state.canvas_shader.bind();

			r_ris.last_blend_mode = -1;

			/*
			//this is set again, so it should not be needed anyway?
			state.canvas_item_modulate = unshaded ? ci->final_modulate : Color(
						ci->final_modulate.r * p_modulate.r,
						ci->final_modulate.g * p_modulate.g,
						ci->final_modulate.b * p_modulate.b,
						ci->final_modulate.a * p_modulate.a );


			state.canvas_shader.set_uniform(CanvasShaderGLES3::MODELVIEW_MATRIX,state.final_transform);
			state.canvas_shader.set_uniform(CanvasShaderGLES3::EXTRA_MATRIX,Transform2D());
			state.canvas_shader.set_uniform(CanvasShaderGLES3::FINAL_MODULATE,state.canvas_item_modulate);

			glBlendEquation(GL_FUNC_ADD);

			if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT]) {
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			} else {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			//@TODO RESET canvas_blend_mode
			*/
		}
	}

	if (reclip) {

		glEnable(GL_SCISSOR_TEST);
		int y = storage->frame.current_rt->height - (r_ris.current_clip->final_clip_rect.position.y + r_ris.current_clip->final_clip_rect.size.y);
		if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_VFLIP])
			y = r_ris.current_clip->final_clip_rect.position.y;
		glScissor(r_ris.current_clip->final_clip_rect.position.x, y, r_ris.current_clip->final_clip_rect.size.width, r_ris.current_clip->final_clip_rect.size.height);
	}


}

// This function is a dry run of the state changes when drawing the item.
// It should duplicate the logic in _canvas_render_item,
// to decide whether items are similar enough to join
// i.e. no state differences between the 2 items.
bool RasterizerCanvasGLES3::try_join_item(Item *p_ci, RenderItemState &r_ris, bool &r_batch_break) {

	return false;
}

void RasterizerCanvasGLES3::canvas_render_items_implementation(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) {

	// parameters are easier to pass around in a structure
	RenderItemState ris;
	ris.item_group_z = p_z;
	ris.item_group_modulate = p_modulate;
	ris.item_group_light = p_light;
	ris.item_group_base_transform = p_base_transform;
	ris.prev_distance_field = false;

	glBindBuffer(GL_UNIFORM_BUFFER, state.canvas_item_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(CanvasItemUBO), &state.canvas_item_ubo_data, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	state.current_tex = RID();
	state.current_tex_ptr = NULL;
	state.current_normal = RID();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);

	//	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SKELETON, false);
	//	state.current_tex = RID();
	//	state.current_tex_ptr = NULL;
	//	state.current_normal = RID();
	//	state.canvas_texscreen_used = false;
	//	glActiveTexture(GL_TEXTURE0);
	//	glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);

	if (bdata.settings_use_batching) {
		for (int j = 0; j < bdata.items_joined.size(); j++) {
			render_joined_item(bdata.items_joined[j], ris);
		}
	} else {
		while (p_item_list) {

			Item *ci = p_item_list;
			_canvas_render_item(ci, ris);
			p_item_list = p_item_list->next;
		}
	}

	if (ris.current_clip) {
		glDisable(GL_SCISSOR_TEST);
	}

	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_SKELETON, false);
}

void RasterizerCanvasGLES3::_batch_upload_buffers() {

	// noop?
	if (!bdata.vertices.size())
		return;

	glBindBuffer(GL_ARRAY_BUFFER, bdata.gl_vertex_buffer);

	// orphan the old (for now)
	glBufferData(GL_ARRAY_BUFFER, 0, 0, GL_DYNAMIC_DRAW);

	if (!bdata.use_colored_vertices) {
		glBufferData(GL_ARRAY_BUFFER, sizeof(BatchVertex) * bdata.vertices.size(), bdata.vertices.get_data(), GL_DYNAMIC_DRAW);
	} else {
		glBufferData(GL_ARRAY_BUFFER, sizeof(BatchVertexColored) * bdata.vertices_colored.size(), bdata.vertices_colored.get_data(), GL_DYNAMIC_DRAW);
	}

	// might not be necessary
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RasterizerCanvasGLES3::_batch_render_rects_test(const Batch &p_batch, RasterizerStorageGLES3::Material *p_material) {

	_set_texture_rect_mode(false);

	// batch tex
	const BatchTex &tex = bdata.batch_textures[p_batch.batch_texture_id];

	RasterizerStorageGLES3::Texture *p_texture = _bind_canvas_texture(tex.RID_texture, tex.RID_normal);

	if (p_texture) {

		bool untile = false;

//		if (p_rect->flags & CANVAS_RECT_TILE && !(p_texture->flags & VS::TEXTURE_FLAG_REPEAT)) {
//			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//			untile = true;
//		}

		Size2 texpixel_size(1.0 / p_texture->width, 1.0 / p_texture->height);

		//state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, p_rect->flags & CANVAS_RECT_CLIP_UV);
		state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, false);

		Vector2 points[4];
		bdata.vertices[0].pos.to(points[0]);
		bdata.vertices[1].pos.to(points[1]);
		bdata.vertices[2].pos.to(points[2]);
		bdata.vertices[3].pos.to(points[3]);

//		Vector2 points[4] = {
//			p_rect->rect.position,
//			p_rect->rect.position + Vector2(p_rect->rect.size.x, 0.0),
//			p_rect->rect.position + p_rect->rect.size,
//			p_rect->rect.position + Vector2(0.0, p_rect->rect.size.y),
//		};

//		if (p_rect->rect.size.x < 0) {
//			SWAP(points[0], points[1]);
//			SWAP(points[2], points[3]);
//		}
//		if (p_rect->rect.size.y < 0) {
//			SWAP(points[0], points[3]);
//			SWAP(points[1], points[2]);
//		}
//		Rect2 src_rect = (p_rect->flags & CANVAS_RECT_REGION) ? Rect2(p_rect->source.position * texpixel_size, p_rect->source.size * texpixel_size) : Rect2(0, 0, 1, 1);

		Vector2 uvs[4];
		bdata.vertices[0].uv.to(uvs[0]);
		bdata.vertices[1].uv.to(uvs[1]);
		bdata.vertices[2].uv.to(uvs[2]);
		bdata.vertices[3].uv.to(uvs[3]);

//		Vector2 uvs[4] = {
//			src_rect.position,
//			src_rect.position + Vector2(src_rect.size.x, 0.0),
//			src_rect.position + src_rect.size,
//			src_rect.position + Vector2(0.0, src_rect.size.y),
//		};

//		if (p_rect->flags & CANVAS_RECT_TRANSPOSE) {
//			SWAP(uvs[1], uvs[3]);
//		}

//		if (p_rect->flags & CANVAS_RECT_FLIP_H) {
//			SWAP(uvs[0], uvs[1]);
//			SWAP(uvs[2], uvs[3]);
//		}
//		if (p_rect->flags & CANVAS_RECT_FLIP_V) {
//			SWAP(uvs[0], uvs[3]);
//			SWAP(uvs[1], uvs[2]);
//		}

		_draw_gui_primitive(4, points, NULL, uvs);

//		if (untile) {
//			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//		}

	} else {
//		state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, false);

//		Vector2 points[4] = {
//			p_rect->rect.position,
//			p_rect->rect.position + Vector2(p_rect->rect.size.x, 0.0),
//			p_rect->rect.position + p_rect->rect.size,
//			p_rect->rect.position + Vector2(0.0, p_rect->rect.size.y),
//		};

//		_draw_gui_primitive(4, points, NULL, nullptr);
	}

}

void RasterizerCanvasGLES3::_batch_render_rects(const Batch &p_batch, RasterizerStorageGLES3::Material *p_material) {
	ERR_FAIL_COND(p_batch.num_commands <= 0);

	const bool &colored_verts = bdata.use_colored_vertices;
	int sizeof_vert;
	if (!colored_verts) {
		sizeof_vert = sizeof(BatchVertex);
	} else {
		sizeof_vert = sizeof(BatchVertexColored);
	}

	_set_texture_rect_mode(false);
//	state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_TEXTURE_RECT, false);

//	state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, p_rect->flags & CANVAS_RECT_CLIP_UV);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::CLIP_RECT_UV, false);

//	if (state.canvas_shader.bind()) {
//		_set_uniforms();
//		state.canvas_shader.use_material((void *)p_material);
//	}

	// batch tex
	const BatchTex &tex = bdata.batch_textures[p_batch.batch_texture_id];

	_bind_canvas_texture(tex.RID_texture, tex.RID_normal);

	// bind the index and vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, bdata.gl_vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bdata.gl_index_buffer);

	uint64_t pointer = 0;
	glVertexAttribPointer(VS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof_vert, (const void *)pointer);

	// always send UVs, even within a texture specified because a shader can still use UVs
	glVertexAttribPointer(VS::ARRAY_TEX_UV, 2, GL_FLOAT, GL_FALSE, sizeof_vert, CAST_INT_TO_UCHAR_PTR(pointer + (2 * 4)));
	glEnableVertexAttribArray(VS::ARRAY_TEX_UV);

	// color
	if (!colored_verts) {
		glDisableVertexAttribArray(VS::ARRAY_COLOR);
		glVertexAttrib4fv(VS::ARRAY_COLOR, p_batch.color.get_data());
	} else {
		glVertexAttribPointer(VS::ARRAY_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof_vert, CAST_INT_TO_UCHAR_PTR(pointer + (4 * 4)));
		glEnableVertexAttribArray(VS::ARRAY_COLOR);
	}

	switch (tex.tile_mode) {
//		case BatchTex::TILE_FORCE_REPEAT: {
//			state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_FORCE_REPEAT, true);
//		} break;
		case BatchTex::TILE_NORMAL: {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		} break;
		default: {
		} break;
	}

	// we need to convert explicitly from pod Vec2 to Vector2 ...
	// could use a cast but this might be unsafe in future
	Vector2 tps;
	tex.tex_pixel_size.to(tps);
	state.canvas_shader.set_uniform(CanvasShaderGLES3::COLOR_TEXPIXEL_SIZE, tps);

	int64_t offset = p_batch.first_quad * 6 * 2; // 6 inds per quad at 2 bytes each

	int num_elements = p_batch.num_commands * 6;
	glDrawElements(GL_TRIANGLES, num_elements, GL_UNSIGNED_SHORT, (void *)offset);

	storage->info.render._2d_draw_call_count++;

	switch (tex.tile_mode) {
//		case BatchTex::TILE_FORCE_REPEAT: {
//			state.canvas_shader.set_conditional(CanvasShaderGLES3::USE_FORCE_REPEAT, false);
//		} break;
		case BatchTex::TILE_NORMAL: {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		} break;
		default: {
		} break;
	}

	glDisableVertexAttribArray(VS::ARRAY_TEX_UV);
	glDisableVertexAttribArray(VS::ARRAY_COLOR);

	// may not be necessary .. state change optimization still TODO
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void RasterizerCanvasGLES3::initialize() {
	RasterizerCanvasBaseGLES3::initialize();

	batch_initialize();

	// just reserve some space (may not be needed as we are orphaning, but hey ho)
	glGenBuffers(1, &bdata.gl_vertex_buffer);

	if (bdata.vertex_buffer_size_bytes) {
		glBindBuffer(GL_ARRAY_BUFFER, bdata.gl_vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, bdata.vertex_buffer_size_bytes, NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// pre fill index buffer, the indices never need to change so can be static
		glGenBuffers(1, &bdata.gl_index_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bdata.gl_index_buffer);

		Vector<uint16_t> indices;
		indices.resize(bdata.index_buffer_size_units);

		for (int q = 0; q < bdata.max_quads; q++) {
			int i_pos = q * 6; //  6 inds per quad
			int q_pos = q * 4; // 4 verts per quad
			indices.set(i_pos, q_pos);
			indices.set(i_pos + 1, q_pos + 1);
			indices.set(i_pos + 2, q_pos + 2);
			indices.set(i_pos + 3, q_pos);
			indices.set(i_pos + 4, q_pos + 2);
			indices.set(i_pos + 5, q_pos + 3);

			// we can only use 16 bit indices in GLES2!
#ifdef DEBUG_ENABLED
			CRASH_COND((q_pos + 3) > 65535);
#endif
		}

		glBufferData(GL_ELEMENT_ARRAY_BUFFER, bdata.index_buffer_size_bytes, &indices[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	} // only if there is a vertex buffer (batching is on)
}

RasterizerCanvasGLES3::RasterizerCanvasGLES3() {

	batch_constructor();
}
