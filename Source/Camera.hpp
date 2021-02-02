/*
	Copyright Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#pragma once

#include <glm/glm.hpp>


namespace LV
{
	namespace Camera
	{
		void update();

		void set_defaults();

		// Getters.
		float get_near_clip();

		float get_far_clip();

		const glm::fmat4& get_view();

		const glm::fmat4& get_projection();

		const glm::fvec3& get_position();

		const glm::fvec3& get_direction();
	}
}
