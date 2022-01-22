/*
	Copyright Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>


namespace LV
{
	struct Mesh
	{
		std::vector<glm::fvec3> vertices;
		std::vector<unsigned> indices;
	};
}


namespace LV::Frustum
{
	void generate(const std::string& name, const std::string& dataset, float top,
		float left, float bottom, float right, const std::string& api_key);

	void load(const std::string& name);

	// Getters.
	glm::ivec2 get_size();

	Mesh get_terrain_mesh();

	Mesh get_buildings_mesh();

	Mesh get_base_mesh();
}
