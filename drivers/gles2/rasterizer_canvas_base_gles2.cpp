#include "rasterizer_canvas_base_gles2.h"

#include "core/os/os.h"
#include "core/project_settings.h"
#include "rasterizer_scene_gles2.h"
#include "servers/visual/visual_server_raster.h"

#ifndef GLES_OVER_GL
#define glClearDepth glClearDepthf
#endif

void RasterizerCanvasBaseGLES2::canvas_begin() {

	state.canvas_shader.bind();
	state.using_transparent_rt = false;
	int viewport_x, viewport_y, viewport_width, viewport_height;

	if (storage->frame.current_rt) {
		glBindFramebuffer(GL_FRAMEBUFFER, storage->frame.current_rt->fbo);
		state.using_transparent_rt = storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_TRANSPARENT];

		if (storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_DIRECT_TO_SCREEN]) {
			// set Viewport and Scissor when rendering directly to screen
			viewport_width = storage->frame.current_rt->width;
			viewport_height = storage->frame.current_rt->height;
			viewport_x = storage->frame.current_rt->x;
			viewport_y = OS::get_singleton()->get_window_size().height - viewport_height - storage->frame.current_rt->y;
			glScissor(viewport_x, viewport_y, viewport_width, viewport_height);
			glViewport(viewport_x, viewport_y, viewport_width, viewport_height);
			glEnable(GL_SCISSOR_TEST);
		}
	}

	if (storage->frame.clear_request) {
		glClearColor(storage->frame.clear_request_color.r,
				storage->frame.clear_request_color.g,
				storage->frame.clear_request_color.b,
				state.using_transparent_rt ? storage->frame.clear_request_color.a : 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		storage->frame.clear_request = false;
	}

	/*
	if (storage->frame.current_rt) {
		glBindFramebuffer(GL_FRAMEBUFFER, storage->frame.current_rt->fbo);
		glColorMask(1, 1, 1, 1);
	}
	*/

	reset_canvas();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);

	glVertexAttrib4f(VS::ARRAY_COLOR, 1, 1, 1, 1);
	glDisableVertexAttribArray(VS::ARRAY_COLOR);

	// set up default uniforms

	Transform canvas_transform;

	if (storage->frame.current_rt) {

		float csy = 1.0;
		if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_VFLIP]) {
			csy = -1.0;
		}
		canvas_transform.translate(-(storage->frame.current_rt->width / 2.0f), -(storage->frame.current_rt->height / 2.0f), 0.0f);
		canvas_transform.scale(Vector3(2.0f / storage->frame.current_rt->width, csy * -2.0f / storage->frame.current_rt->height, 1.0f));
	} else {
		Vector2 ssize = OS::get_singleton()->get_window_size();
		canvas_transform.translate(-(ssize.width / 2.0f), -(ssize.height / 2.0f), 0.0f);
		canvas_transform.scale(Vector3(2.0f / ssize.width, -2.0f / ssize.height, 1.0f));
	}

	state.uniforms.projection_matrix = canvas_transform;

	state.uniforms.final_modulate = Color(1, 1, 1, 1);

	state.uniforms.modelview_matrix = Transform2D();
	state.uniforms.extra_matrix = Transform2D();

	_set_uniforms();
	_bind_quad_buffer();
}

void RasterizerCanvasBaseGLES2::canvas_end() {

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	for (int i = 0; i < VS::ARRAY_MAX; i++) {
		glDisableVertexAttribArray(i);
	}

	if (storage->frame.current_rt && storage->frame.current_rt->flags[RasterizerStorage::RENDER_TARGET_DIRECT_TO_SCREEN]) {
		//reset viewport to full window size
		int viewport_width = OS::get_singleton()->get_window_size().width;
		int viewport_height = OS::get_singleton()->get_window_size().height;
		glViewport(0, 0, viewport_width, viewport_height);
		glScissor(0, 0, viewport_width, viewport_height);
	}

	state.using_texture_rect = false;
	state.using_skeleton = false;
	state.using_ninepatch = false;
	state.using_transparent_rt = false;
}

void RasterizerCanvasBaseGLES2::draw_generic_textured_rect(const Rect2 &p_rect, const Rect2 &p_src) {

	state.canvas_shader.set_uniform(CanvasShaderGLES2::DST_RECT, Color(p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y));
	state.canvas_shader.set_uniform(CanvasShaderGLES2::SRC_RECT, Color(p_src.position.x, p_src.position.y, p_src.size.x, p_src.size.y));

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}


RasterizerStorageGLES2::Texture *RasterizerCanvasBaseGLES2::_bind_canvas_texture(const RID &p_texture, const RID &p_normal_map) {

	RasterizerStorageGLES2::Texture *tex_return = NULL;

	if (p_texture.is_valid()) {

		RasterizerStorageGLES2::Texture *texture = storage->texture_owner.getornull(p_texture);

		if (!texture) {
			state.current_tex = RID();
			state.current_tex_ptr = NULL;

			glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 1);
			glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);

		} else {

			if (texture->redraw_if_visible) {
				VisualServerRaster::redraw_request();
			}

			texture = texture->get_ptr();

			if (texture->render_target) {
				texture->render_target->used_in_frame = true;
			}

			glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 1);
			glBindTexture(GL_TEXTURE_2D, texture->tex_id);

			state.current_tex = p_texture;
			state.current_tex_ptr = texture;

			tex_return = texture;
		}
	} else {
		state.current_tex = RID();
		state.current_tex_ptr = NULL;

		glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 1);
		glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);
	}

	if (p_normal_map == state.current_normal) {
		//do none
		state.canvas_shader.set_uniform(CanvasShaderGLES2::USE_DEFAULT_NORMAL, state.current_normal.is_valid());

	} else if (p_normal_map.is_valid()) {

		RasterizerStorageGLES2::Texture *normal_map = storage->texture_owner.getornull(p_normal_map);

		if (!normal_map) {
			state.current_normal = RID();
			glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 2);
			glBindTexture(GL_TEXTURE_2D, storage->resources.normal_tex);
			state.canvas_shader.set_uniform(CanvasShaderGLES2::USE_DEFAULT_NORMAL, false);

		} else {

			if (normal_map->redraw_if_visible) { //check before proxy, because this is usually used with proxies
				VisualServerRaster::redraw_request();
			}

			normal_map = normal_map->get_ptr();

			glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 2);
			glBindTexture(GL_TEXTURE_2D, normal_map->tex_id);
			state.current_normal = p_normal_map;
			state.canvas_shader.set_uniform(CanvasShaderGLES2::USE_DEFAULT_NORMAL, true);
		}

	} else {

		state.current_normal = RID();
		glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 2);
		glBindTexture(GL_TEXTURE_2D, storage->resources.normal_tex);
		state.canvas_shader.set_uniform(CanvasShaderGLES2::USE_DEFAULT_NORMAL, false);
	}

	return tex_return;
}

void RasterizerCanvasBaseGLES2::draw_window_margins(int *black_margin, RID *black_image) {

	Vector2 window_size = OS::get_singleton()->get_window_size();
	int window_h = window_size.height;
	int window_w = window_size.width;

	glBindFramebuffer(GL_FRAMEBUFFER, storage->system_fbo);
	glViewport(0, 0, window_size.width, window_size.height);
	canvas_begin();

	if (black_image[MARGIN_LEFT].is_valid()) {
		_bind_canvas_texture(black_image[MARGIN_LEFT], RID());
		Size2 sz(storage->texture_get_width(black_image[MARGIN_LEFT]), storage->texture_get_height(black_image[MARGIN_LEFT]));
		draw_generic_textured_rect(Rect2(0, 0, black_margin[MARGIN_LEFT], window_h), Rect2(0, 0, sz.x, sz.y));
	} else if (black_margin[MARGIN_LEFT]) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);

		draw_generic_textured_rect(Rect2(0, 0, black_margin[MARGIN_LEFT], window_h), Rect2(0, 0, 1, 1));
	}

	if (black_image[MARGIN_RIGHT].is_valid()) {
		_bind_canvas_texture(black_image[MARGIN_RIGHT], RID());
		Size2 sz(storage->texture_get_width(black_image[MARGIN_RIGHT]), storage->texture_get_height(black_image[MARGIN_RIGHT]));
		draw_generic_textured_rect(Rect2(window_w - black_margin[MARGIN_RIGHT], 0, black_margin[MARGIN_RIGHT], window_h), Rect2(0, 0, sz.x, sz.y));
	} else if (black_margin[MARGIN_RIGHT]) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);

		draw_generic_textured_rect(Rect2(window_w - black_margin[MARGIN_RIGHT], 0, black_margin[MARGIN_RIGHT], window_h), Rect2(0, 0, 1, 1));
	}

	if (black_image[MARGIN_TOP].is_valid()) {
		_bind_canvas_texture(black_image[MARGIN_TOP], RID());

		Size2 sz(storage->texture_get_width(black_image[MARGIN_TOP]), storage->texture_get_height(black_image[MARGIN_TOP]));
		draw_generic_textured_rect(Rect2(0, 0, window_w, black_margin[MARGIN_TOP]), Rect2(0, 0, sz.x, sz.y));

	} else if (black_margin[MARGIN_TOP]) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);

		draw_generic_textured_rect(Rect2(0, 0, window_w, black_margin[MARGIN_TOP]), Rect2(0, 0, 1, 1));
	}

	if (black_image[MARGIN_BOTTOM].is_valid()) {

		_bind_canvas_texture(black_image[MARGIN_BOTTOM], RID());

		Size2 sz(storage->texture_get_width(black_image[MARGIN_BOTTOM]), storage->texture_get_height(black_image[MARGIN_BOTTOM]));
		draw_generic_textured_rect(Rect2(0, window_h - black_margin[MARGIN_BOTTOM], window_w, black_margin[MARGIN_BOTTOM]), Rect2(0, 0, sz.x, sz.y));

	} else if (black_margin[MARGIN_BOTTOM]) {

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);

		draw_generic_textured_rect(Rect2(0, window_h - black_margin[MARGIN_BOTTOM], window_w, black_margin[MARGIN_BOTTOM]), Rect2(0, 0, 1, 1));
	}

	canvas_end();
}


void RasterizerCanvasBaseGLES2::_bind_quad_buffer() {
	glBindBuffer(GL_ARRAY_BUFFER, data.canvas_quad_vertices);
	glEnableVertexAttribArray(VS::ARRAY_VERTEX);
	glVertexAttribPointer(VS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, NULL);
}

void RasterizerCanvasBaseGLES2::_set_uniforms() {

	state.canvas_shader.set_uniform(CanvasShaderGLES2::PROJECTION_MATRIX, state.uniforms.projection_matrix);
	state.canvas_shader.set_uniform(CanvasShaderGLES2::MODELVIEW_MATRIX, state.uniforms.modelview_matrix);
	state.canvas_shader.set_uniform(CanvasShaderGLES2::EXTRA_MATRIX, state.uniforms.extra_matrix);

	state.canvas_shader.set_uniform(CanvasShaderGLES2::FINAL_MODULATE, state.uniforms.final_modulate);

	state.canvas_shader.set_uniform(CanvasShaderGLES2::TIME, storage->frame.time[0]);

	if (storage->frame.current_rt) {
		Vector2 screen_pixel_size;
		screen_pixel_size.x = 1.0 / storage->frame.current_rt->width;
		screen_pixel_size.y = 1.0 / storage->frame.current_rt->height;

		state.canvas_shader.set_uniform(CanvasShaderGLES2::SCREEN_PIXEL_SIZE, screen_pixel_size);
	}

	if (state.using_skeleton) {
		state.canvas_shader.set_uniform(CanvasShaderGLES2::SKELETON_TRANSFORM, state.skeleton_transform);
		state.canvas_shader.set_uniform(CanvasShaderGLES2::SKELETON_TRANSFORM_INVERSE, state.skeleton_transform_inverse);
		state.canvas_shader.set_uniform(CanvasShaderGLES2::SKELETON_TEXTURE_SIZE, state.skeleton_texture_size);
	}

	if (state.using_light) {

		Light *light = state.using_light;
		state.canvas_shader.set_uniform(CanvasShaderGLES2::LIGHT_MATRIX, light->light_shader_xform);
		Transform2D basis_inverse = light->light_shader_xform.affine_inverse().orthonormalized();
		basis_inverse[2] = Vector2();
		state.canvas_shader.set_uniform(CanvasShaderGLES2::LIGHT_MATRIX_INVERSE, basis_inverse);
		state.canvas_shader.set_uniform(CanvasShaderGLES2::LIGHT_LOCAL_MATRIX, light->xform_cache.affine_inverse());
		state.canvas_shader.set_uniform(CanvasShaderGLES2::LIGHT_COLOR, light->color * light->energy);
		state.canvas_shader.set_uniform(CanvasShaderGLES2::LIGHT_POS, light->light_shader_pos);
		state.canvas_shader.set_uniform(CanvasShaderGLES2::LIGHT_HEIGHT, light->height);
		state.canvas_shader.set_uniform(CanvasShaderGLES2::LIGHT_OUTSIDE_ALPHA, light->mode == VS::CANVAS_LIGHT_MODE_MASK ? 1.0 : 0.0);

		if (state.using_shadow) {
			RasterizerStorageGLES2::CanvasLightShadow *cls = storage->canvas_light_shadow_owner.get(light->shadow_buffer);
			glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 5);
			glBindTexture(GL_TEXTURE_2D, cls->distance);
			state.canvas_shader.set_uniform(CanvasShaderGLES2::SHADOW_MATRIX, light->shadow_matrix_cache);
			state.canvas_shader.set_uniform(CanvasShaderGLES2::LIGHT_SHADOW_COLOR, light->shadow_color);

			state.canvas_shader.set_uniform(CanvasShaderGLES2::SHADOWPIXEL_SIZE, (1.0 / light->shadow_buffer_size) * (1.0 + light->shadow_smooth));
			if (light->radius_cache == 0) {
				state.canvas_shader.set_uniform(CanvasShaderGLES2::SHADOW_GRADIENT, 0.0);
			} else {
				state.canvas_shader.set_uniform(CanvasShaderGLES2::SHADOW_GRADIENT, light->shadow_gradient_length / (light->radius_cache * 1.1));
			}
			state.canvas_shader.set_uniform(CanvasShaderGLES2::SHADOW_DISTANCE_MULT, light->radius_cache * 1.1);

			/*canvas_shader.set_uniform(CanvasShaderGLES2::SHADOW_MATRIX,light->shadow_matrix_cache);
			canvas_shader.set_uniform(CanvasShaderGLES2::SHADOW_ESM_MULTIPLIER,light->shadow_esm_mult);
			canvas_shader.set_uniform(CanvasShaderGLES2::LIGHT_SHADOW_COLOR,light->shadow_color);*/
		}
	}
}
