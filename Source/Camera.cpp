/*
	Copyright Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#include "Camera.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtx/quaternion.hpp>

#include "Constants.hpp"
#include "Window.hpp"


namespace
{
	constexpr float near_clip{3.f};
	constexpr float far_clip{3000.f};
	constexpr float minimum_fov{glm::radians(20.f)};
	constexpr float maximum_fov{glm::radians(150.f)};
	constexpr float pitch_limit{glm::radians(89.f)};

	constexpr float mouse_sensitivity{.001f};
	constexpr float smooth_mouse_speed{15.f};
	constexpr float fov_speed{100.f};
	constexpr float damping{15.f};
	constexpr float walk_speed{30.f};
	constexpr float run_speed{300.f};

	constexpr glm::fvec3 world_up{0.f, 1.f, 0.f};
	constexpr glm::fvec3 world_right{1.f, 0.f, 0.f};
	constexpr glm::fvec3 world_forward{0.f, 0.f, -1.f};

	bool smooth{LV::Constants::smooth_camera};

	glm::fvec2 axes;
	glm::fvec3 position;
	glm::fvec3 direction;
	float fov;

	glm::fvec2 axes_velocity;
	float fov_velocity;

	glm::fvec3 forward;
	glm::fvec3 right;
	glm::fmat4 view;
	glm::fmat4 projection;


	float dampen(float x)
	{
		x /= 1.f+damping*LV::Window::get_delta();
		if(abs(x) < .00001f) x = 0.f;
		return x;
	}
}


void LV::Camera::update()
{
	if(Window::is_cursor_captured())
	{
		// FOV.
		const float scroll_delta{-Window::get_scroll_delta()};

		fov_velocity = dampen(fov_velocity+glm::radians(
			scroll_delta*fov_speed*Window::get_delta()));

		fov += fov_velocity;
		if(fov > maximum_fov) fov = maximum_fov;
		else if(fov < minimum_fov) fov = minimum_fov;

		// Cursor movement.
		glm::fvec2 cursor_delta{-glm::fvec2{Window::get_cursor_delta()}*mouse_sensitivity};

		if(smooth)
		{
			const float fov_divisor{(1.5f-fov)*2.f};
			const float fov_adjusted_mouse_speed{static_cast<float>(smooth_mouse_speed*
				Window::get_delta())/(fov_divisor < .7f ? .7f : fov_divisor)};

			axes_velocity.x = dampen(axes_velocity.x+cursor_delta.x*fov_adjusted_mouse_speed);
			axes_velocity.y = dampen(axes_velocity.y+cursor_delta.y*fov_adjusted_mouse_speed);

			axes += axes_velocity;
		}

		else axes += cursor_delta;

		if(axes.y > pitch_limit) axes.y = pitch_limit;
		else if(axes.y < -pitch_limit) axes.y = -pitch_limit;

		direction = glm::rotate(glm::angleAxis(axes.x, world_up)*
			glm::angleAxis(axes.y, world_right), world_forward);

		right = glm::normalize(glm::cross(direction, world_up));
		forward = glm::normalize(direction*glm::fvec3{1.f, 0.f, 1.f});

		// Keyboard movement.
		const float velocity{(Window::is_held(GLFW_KEY_LEFT_SHIFT) ?
			run_speed : walk_speed)*static_cast<float>(Window::get_delta())};

		if(Window::is_held(GLFW_KEY_W)) position += direction*velocity;
		if(Window::is_held(GLFW_KEY_S)) position -= direction*velocity;
		if(Window::is_held(GLFW_KEY_A)) position -= right*velocity;
		if(Window::is_held(GLFW_KEY_D)) position += right*velocity;
	}

	view = glm::lookAt(position, position+direction, world_up);
	projection = glm::perspective(fov, Window::get_size().x/
		static_cast<float>(Window::get_size().y), near_clip, far_clip);
}


void LV::Camera::set_defaults()
{
	position = LV::Constants::default_camera_position;
	axes = LV::Constants::default_camera_axes;
	fov = LV::Constants::default_camera_fov;
}


float LV::Camera::get_near_clip(){ return near_clip; }

float LV::Camera::get_far_clip(){ return far_clip; }

const glm::fmat4& LV::Camera::get_view(){ return view; }

const glm::fmat4& LV::Camera::get_projection(){ return projection; }

const glm::fvec3& LV::Camera::get_position(){ return position; }

const glm::fvec3& LV::Camera::get_direction(){ return direction; }
