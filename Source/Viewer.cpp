/*
	Copyright Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#include "Viewer.hpp"

#include <iostream>
#include <GLFW/glfw3.h>
#include <glbinding/gl33core/gl.h>
#include <glm/gtx/rotate_vector.hpp>

#include "Window.hpp"
#include "Camera.hpp"
#include "Constants.hpp"
#include "Utilities.hpp"
#include "Frustum.hpp"


namespace
{
	constexpr float light_rotation_velocity{glm::radians(30.f)};
	constexpr float light_rotation_limit{glm::radians(85.f)};

	glm::fvec2 frustum_size;
	LV::Mesh terrain_mesh;
	LV::Mesh buildings_mesh;
	LV::Mesh base_mesh;

	LV::VAO terrain_vao;
	LV::VAO buildings_vao;
	LV::VAO base_vao;
	LV::Shader shadow_shader;
	LV::Shader solid_shader;
	LV::Shader diffuse_shader;
	std::unique_ptr<globjects::Framebuffer> shadow_map_fbo;
	std::unique_ptr<globjects::Texture> shadow_map;

	glm::fvec3 base_light_direction{0.f, 1.f, 0.f};
	glm::fvec2 light_rotation;
	glm::fvec3 light_direction;
	glm::fmat4 light_space_matrix;

	bool show_wireframe{true};


	void recalculate_lighting()
	{
		// Light direction.
		light_direction = base_light_direction;
		light_direction = glm::rotate(light_direction,
			light_rotation.y, glm::fvec3{1.f, 0.f, 0.f});
		light_direction = glm::rotate(light_direction,
			light_rotation.x, glm::fvec3{0.f, 0.f, 1.f});

		// Light space matrix.
		const float shadow_radius{std::max(frustum_size.x, frustum_size.y)/1.5f};

		glm::fmat4 light_rotation_matrix{
			glm::rotate(light_rotation.x, glm::fvec3{0.f, 1.f, 0.f})*
			glm::rotate(-light_rotation.y, glm::fvec3{1.f, 0.f, 0.f})};

		glm::fmat4 shadow_view_matrix{glm::ortho(shadow_radius, -shadow_radius,
			0.f, shadow_radius*2, shadow_radius, -shadow_radius)};

		glm::fmat4 shadow_projection_matrix{light_rotation_matrix*glm::lookAt(
			-base_light_direction, glm::fvec3{0.f, 1.f, 0.f}, glm::fvec3{0.f, 0.f, 1.f})};

		light_space_matrix = shadow_projection_matrix*shadow_view_matrix;
	}


	void update_light()
	{
		glm::fvec2 light_rotation_direction{};
		// if(LV::Window::is_held(GLFW_KEY_DOWN)) light_rotation_direction.y += 1.f;
		// if(LV::Window::is_held(GLFW_KEY_UP)) light_rotation_direction.y -= 1.f;
		if(LV::Window::is_held(GLFW_KEY_LEFT)) light_rotation_direction.x += 1.f;
		if(LV::Window::is_held(GLFW_KEY_RIGHT)) light_rotation_direction.x -= 1.f;

		if(light_rotation_direction.x || light_rotation_direction.y)
		{
			// Calculate the new light rotation.
			light_rotation += light_rotation_direction*light_rotation_velocity*
				static_cast<float>(LV::Window::get_delta());

			light_rotation = glm::clamp(light_rotation,
				-light_rotation_limit, light_rotation_limit);

			// Clamp the rotation to a radius.
			float distance{glm::distance(light_rotation, glm::fvec2{0.f, 0.f})};
			if(distance > light_rotation_limit)
				light_rotation = light_rotation*light_rotation_limit/distance;

			// Recalculate lighting.
			recalculate_lighting();
		}
	}


	void create_shadow_buffer()
	{
		// Create the shadow map texture.
		shadow_map = globjects::Texture::createDefault(gl::GL_TEXTURE_2D);
		shadow_map->image2D(0, gl::GL_DEPTH_COMPONENT16,
			glm::ivec2{LV::Constants::shadow_resolution},
			0, gl::GL_DEPTH_COMPONENT, gl::GL_FLOAT, nullptr);

		// Generate the shadow map framebuffer object.
		shadow_map_fbo = globjects::Framebuffer::create();
		shadow_map_fbo->bind();
		shadow_map_fbo->attachTexture(gl::GL_DEPTH_ATTACHMENT, shadow_map.get());
		shadow_map_fbo->setDrawBuffer(gl::GL_NONE);
		shadow_map_fbo->unbind();
	}


	void bind_matricies_and_shadow_map(const LV::Shader& shader)
	{
		shader.program->setUniform("view_matrix", LV::Camera::get_view());
		shader.program->setUniform("projection_matrix", LV::Camera::get_projection());
		shader.program->setUniform("light_space_matrix", light_space_matrix);

		shader.program->setUniform("shadow_map", 0);
		shadow_map->bindActive(0);
	}


	void bind_solid_shader(const glm::fvec3& color, float shadow_intensity,
		const glm::fvec3& offset = {0.f, 0.f, 0.f})
	{
		bind_matricies_and_shadow_map(solid_shader);
		solid_shader.program->setUniform("offset", offset);
		solid_shader.program->setUniform("color", color);
		solid_shader.program->setUniform("shadow_intensity", shadow_intensity);
		solid_shader.program->use();
	}


	void bind_diffuse_shader(const glm::fvec3& color)
	{
		bind_matricies_and_shadow_map(diffuse_shader);
		diffuse_shader.program->setUniform("light_direction", light_direction);
		diffuse_shader.program->setUniform("color", color);
		diffuse_shader.program->use();
	}


	void render_mesh(const LV::VAO& vao, const LV::Mesh& mesh, bool cull = true)
	{
		if(cull) gl::glEnable(gl::GL_CULL_FACE);

		vao.vao->drawElements(gl::GL_TRIANGLES, static_cast<gl::GLsizei>(
			mesh.indices.size()), gl::GL_UNSIGNED_INT, nullptr);

		if(cull) gl::glDisable(gl::GL_CULL_FACE);
	}


	void shadow_map_pass()
	{
		// Initialize the shadow map framebuffer.
		shadow_map_fbo->bind();
		gl::glViewport(0, 0, LV::Constants::shadow_resolution,
			LV::Constants::shadow_resolution);
		gl::glClear(gl::GL_DEPTH_BUFFER_BIT);

		// Initialize the shader.
		shadow_shader.program->setUniform("light_space_matrix", light_space_matrix);
		shadow_shader.program->use();

		// Render.
		render_mesh(terrain_vao, terrain_mesh);
		render_mesh(base_vao, terrain_mesh);
		render_mesh(buildings_vao, buildings_mesh, false);

		// Return the framebuffer to defaults.
		globjects::Framebuffer::defaultFBO()->bind();
		glm::ivec2 window_size{LV::Window::get_size()};
		gl::glViewport(0, 0, window_size.x, window_size.y);
	}


	void wireframe_pass()
	{
		bind_solid_shader(LV::Constants::terrain_wireframe_color,
			.5f, glm::fvec3{0.f, .01f, 0.f});

		gl::glPolygonMode(gl::GL_FRONT_AND_BACK, gl::GL_LINE);

		render_mesh(terrain_vao, terrain_mesh);
		solid_shader.program->setUniform("color", LV::Constants::buildings_wireframe_color);

		render_mesh(buildings_vao, buildings_mesh, false);

		gl::glPolygonMode(gl::GL_FRONT_AND_BACK, gl::GL_FILL);
	}
}


void LV::Viewer::view(const std::string& name)
{
	// Load the Frustum.
	LV::Frustum::load(name);
	frustum_size = LV::Frustum::get_size();
	terrain_mesh = LV::Frustum::get_terrain_mesh();
	buildings_mesh = LV::Frustum::get_buildings_mesh();
	base_mesh = LV::Frustum::get_base_mesh();

	// Disable wireframe by default if necessary.
	if((terrain_mesh.indices.size()+buildings_mesh.indices.size())/3
		> LV::Constants::wireframe_triangle_limit) show_wireframe = false;

	light_direction = base_light_direction;
	light_rotation = LV::Constants::initial_light_rotation;
	Camera::set_defaults();

	// Create the window.
	std::cout<<"Launching the viewer...\n";
	Window::create(1260, 720, LV::Constants::program_name+
		" Viewer "+LV::Constants::program_version);
	Window::capture_cursor(true);

	// Compile the shaders.
	std::cout<<"Compiling the shaders...\n";
	LV::Utilities::create_shader(&shadow_shader, "Shadow");
	LV::Utilities::create_shader(&solid_shader, "Solid");
	LV::Utilities::create_shader(&diffuse_shader, "Diffuse");

	// Create the VAOs.
	std::cout<<"Buffering the mesh data...\n";
	LV::Utilities::create_vao(&terrain_vao, diffuse_shader,
		terrain_mesh.vertices, terrain_mesh.indices, true);

	LV::Utilities::create_vao(&buildings_vao, solid_shader,
		buildings_mesh.vertices, buildings_mesh.indices, false);

	LV::Utilities::create_vao(&base_vao, diffuse_shader,
		base_mesh.vertices, base_mesh.indices, false);

	// Create shadow buffer and calculate lighting.
	create_shadow_buffer();
	recalculate_lighting();

	// While the window is open...
	std::cout<<"Rendering...\n";
	while(Window::is_open())
	{
		// Update.
		Window::update();
		if(Window::is_minimized()) continue;

		// Input.
		Camera::update();
		update_light();
		if(Window::was_pressed(GLFW_KEY_F)) show_wireframe = !show_wireframe;
		if(Window::was_pressed(GLFW_KEY_L))
			Window::capture_cursor(!Window::is_cursor_captured());

		// Shadow map pass.
		shadow_map_pass();

		// Terrain pass.
		bind_diffuse_shader(LV::Constants::terrain_color);
		render_mesh(terrain_vao, terrain_mesh);

		// Buildings pass.
		bind_solid_shader(LV::Constants::buildings_color, .7f);
		render_mesh(buildings_vao, buildings_mesh, false);

		// Base pass.
		bind_solid_shader(LV::Constants::base_color, 0.f);
		render_mesh(base_vao, base_mesh);

		// Wireframe pass.
		if(show_wireframe) wireframe_pass();
	}

	// Destroy.
	shadow_map_fbo.reset();
	shadow_map.reset();

	Utilities::destroy_vao(&base_vao);
	Utilities::destroy_vao(&buildings_vao);
	Utilities::destroy_vao(&terrain_vao);

	Utilities::destroy_shader(&diffuse_shader);
	Utilities::destroy_shader(&solid_shader);
	Utilities::destroy_shader(&shadow_shader);

	Window::destroy();
	std::cout<<"Viewer exited.\n";
}
