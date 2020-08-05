/*
	Copyright 2020 Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#pragma once

#include <string>
#include <vector>
#include <GLM/glm.hpp>


namespace LV::Constants
{
	// General.
	const std::string program_name{"Laventh Frustum"};
	const std::string program_version{"2020-8-5"};
	const std::string resources_directory{"Resources"};
	constexpr bool opengl_logging{false};
	constexpr bool curl_verbose{false};

	// Generator.
	const std::string frustum_directory_name{"Frustums"};
	const std::string metadata_file_name{"metadata.lfm"};
	const std::string terrain_file_name{"terrain.lft"};
	const std::string buildings_file_name{"buildings.lfb"};
	const std::vector<std::string> supported_datasets{"aw3d30", "srtmgl1"};
	constexpr float meters_per_frustum_base_unit{30};
	constexpr float terrain_normal_smoothing{1.f};
	constexpr float default_building_height{20.f};
	constexpr float building_level_height{3.428f};
	constexpr float building_depth{20.f};
	constexpr float bottom{-33.f};

	// Viewer.
	constexpr int samples{4};
	constexpr glm::fvec3 clear_color{.9f, .9f, .9f};
	constexpr unsigned wireframe_triangle_limit{1500000};
	constexpr glm::fvec3 default_camera_position{0.f, 500.f, 0.f};
	constexpr glm::fvec2 default_camera_axes{0.f, -glm::radians(88.f)};
	constexpr float default_camera_fov{glm::radians(100.f)};
	constexpr bool smooth_camera{true};
	constexpr int shadow_resolution{8192};
	constexpr glm::fvec2 initial_light_rotation{glm::radians(75.f), 0.f};
	constexpr glm::fvec3 terrain_color{.7f, .7f, .7f};
	constexpr glm::fvec3 base_color{.07f, .07f, .07f};
	constexpr glm::fvec3 buildings_color{1.f, 1.f, 1.f};
	constexpr glm::fvec3 terrain_wireframe_color{1.f, 1.f, 1.f};
	constexpr glm::fvec3 buildings_wireframe_color{1.f, 0.f, .2f};

	// Exporter.
	const std::vector<std::string> supported_formats{"ply", "obj", "stl"};
	const std::string material_name{"Material"};
	constexpr glm::fvec3 material_color{.5f, .5f, .5f};
}