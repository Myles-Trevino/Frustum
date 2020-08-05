/*
	Copyright 2020 Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#include "Frustum.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <GLM/gtc/reciprocal.hpp>
#include <GLM/gtx/transform.hpp>
#include <NLohmann/json.hpp>
#include <Earcut/earcut.hpp>

#include "Request.hpp"
#include "Constants.hpp"
#include "Utilities.hpp"


namespace
{
	struct Bounds
	{
		float top;
		float left;
		float bottom;
		float right;
	};

	struct Building
	{
		float height;
		std::vector<glm::fvec2> outline;
	};


	std::string name;
	std::string dataset;

	Bounds bounds;
	glm::ivec2 size;
	glm::fmat4 center_matrix;

	std::vector<std::vector<float>> terrain_data;
	std::vector<Building> buildings_data;
	LV::Mesh terrain_mesh;
	LV::Mesh buildings_mesh;
	LV::Mesh base_mesh;


	Bounds get_compensated_bounds(const Bounds& bounds)
	{
		Bounds compensated_bounds{bounds};

		float average_latitude{std::abs(bounds.bottom+bounds.top)/2.f};
		float compensation_factor{glm::sec(glm::radians(average_latitude))};
		float distance{glm::distance(bounds.left, bounds.right)};
		float compensation{(distance-distance/compensation_factor)/2.f};

		compensated_bounds.left += compensation;
		compensated_bounds.right -= compensation;

		return compensated_bounds;
	}


	void add_vertex(LV::Mesh* mesh, const glm::fvec3& vertex)
	{ mesh->vertices.emplace_back(center_matrix*glm::fvec4{vertex, 1.f}); }


	void generate_square_indicies(std::vector<unsigned>* indicies, unsigned top_left,
		unsigned bottom_left, unsigned bottom_right, unsigned top_right)
	{
		// Bottom left triangle.
		indicies->emplace_back(top_left);
		indicies->emplace_back(bottom_left);
		indicies->emplace_back(bottom_right);

		// Top right triangle.
		indicies->emplace_back(bottom_right);
		indicies->emplace_back(top_right);
		indicies->emplace_back(top_left);
	}


	void retrieve_terrain_data()
	{
		std::cout<<"Retrieving the topography data...\n";

		// Validate the dataset.
		if(!LV::Utilities::is_supported(dataset, LV::Constants::supported_datasets))
			throw std::runtime_error{"Unrecognized dataset."};

		// Make the request (OpenTopography API).
		std::stringstream coordinates;
		coordinates<<"&west="<<bounds.left<<"&south="<<bounds.bottom<<
			"&east="<<bounds.right<<"&north="<<bounds.top;

		std::string response{LV::Request::request("https://portal.opentopography.org/otr/"
			"getdem?demtype="+dataset+coordinates.str()+"&outputFormat=AAIGrid")};
		std::stringstream response_stream{response};

		if(response.find("Error") != std::string::npos) throw std::runtime_error{
			"Failed to retrieve the topography data. Response: \""+response+"\"."};

		// Get the number of rows and columns of the Arc ASCII dataset.
		std::cout<<"Parsing the topography data...\n";
		LV::Utilities::ignore_until(&response_stream, ' ');
		response_stream>>size.x;
		LV::Utilities::ignore_until(&response_stream, ' ');
		response_stream>>size.y;

		size.y -= 1; // The last row of AW3D30 can be incorrect, so ignore it.

		// Ignore the rest of the header.
		std::string line;
		for(int index{}; index < 5; ++index) std::getline(response_stream, line);

		// Parse the grid.
		terrain_data.clear();

		for(int z{}; z < size.y; ++z)
		{
			terrain_data.emplace_back();
			std::getline(response_stream, line);
			std::stringstream line_stream{line};

			for(int x{}; x < size.x; ++x)
			{
				int y;
				line_stream>>y;
				float elevation;

				if(y <= -9999) elevation = (x == 0) ? 0 : terrain_data.back().back();
				else elevation = y/LV::Constants::meters_per_frustum_base_unit;

				terrain_data.back().emplace_back(elevation);
			}

			if(terrain_data.back().size() != size.x)
				throw std::runtime_error{"Failed to parse the topography data."};
		}

		if(terrain_data.size() != size.y)
			throw std::runtime_error{"Failed to parse the topography data."};
	}


	void generate_terrain_mesh()
	{
		std::cout<<"Generating the terrain mesh...\n";

		terrain_mesh.vertices.clear();
		terrain_mesh.indices.clear();

		// For each row of the grid...
		for(int z{}; z < size.y; ++z)
		{
			// For each column in the row...
			for(int x{}; x < size.x; ++x)
			{
				// Generate the vertex.
				add_vertex(&terrain_mesh, glm::fvec3{x, terrain_data[z][x], z});

				// Generate the normal.
				float adjacent_left_height{terrain_data[z][(x > 0) ? x-1 : 0]};
				float adjacent_right_height{terrain_data[z][(x < size.x-1) ? x+1 : x]};
				float adjacent_top_height{terrain_data[(z > 0) ? z-1 : 0][x]};
				float adjacent_bottom_height{terrain_data[(z < size.y-1) ? z+1 : z][x]};

				const glm::fvec3 normal{
					adjacent_left_height-adjacent_right_height,
					LV::Constants::terrain_normal_smoothing,
					adjacent_top_height-adjacent_bottom_height};

				terrain_mesh.vertices.emplace_back(glm::normalize(normal));

				// Generate the indices.
				if(z >= size.y-1 || x >= size.x-1) continue;

				const unsigned top_left{static_cast<unsigned>(z*size.x+x)};
				const unsigned	bottom_left{static_cast<unsigned>(top_left+size.x)};
				const unsigned	bottom_right{bottom_left+1};
				const unsigned	top_right{top_left+1};

				generate_square_indicies(&terrain_mesh.indices,
					top_left, bottom_left, bottom_right, top_right);
			}
		}
	}


	bool get_building_outline(Building* building,
		const nlohmann::json& json, const glm::fvec2 scale_factor)
	{
		const nlohmann::json::const_iterator geometry{json.find("geometry")};
		if(geometry == json.end()) return false;

		for(const nlohmann::json& point : geometry.value())
		{
			const glm::fvec2 coordinate{glm::fvec2{point["lon"], point["lat"]}};
			const glm::fvec2 relative_coordinate{
				coordinate.x-bounds.left, bounds.top-coordinate.y};
			const glm::fvec2 result{relative_coordinate*scale_factor};

			if(result.x >= size.x-1 || result.y >= size.y-1 ||
				result.x < 0.f || result.y < 0.f) return false;

			building->outline.emplace_back(result);
		}

		return building->outline.size() >= 3;
	}


	void retrieve_buildings_data()
	{
		// Make the request (OpenStreetMap Overpass API).
		std::cout<<"Retrieving the building data...\n";
		std::stringstream coordinates;
		coordinates<<bounds.bottom<<','<<bounds.left
			<<','<<bounds.top<<','<<bounds.right;

		std::string payload
		{
			"[out:json][timeout:10];\n"

			"(\n"
			"	way[\"building\"]("+coordinates.str()+");\n"
			"	relation[\"building\"]("+coordinates.str()+");\n"
			");\n"

			"out geom;"
		};

		const std::string response{LV::Request::request(
			"https://lz4.overpass-api.de/api/interpreter", payload)};

		if(response.find("<?xml") != std::string::npos)
			throw std::runtime_error{"Failed to retrieve the building data."};

		// Parse the response data.
		std::cout<<"Parsing the building data...\n";

		glm::fvec2 scale_factor{size.x/glm::distance(bounds.left, bounds.right),
			size.y/glm::distance(bounds.top, bounds.bottom)};

		buildings_data.clear();
		const nlohmann::json json{nlohmann::json::parse(response)};

		// For each building...
		for(const nlohmann::json& element : json["elements"])
		{
			try
			{
				Building building;
				const nlohmann::json::const_iterator tags{element.find("tags")};
				if(tags == element.end()) continue;

				// Get the building's height.
				const nlohmann::json::const_iterator height{tags.value().find("height")};
				const nlohmann::json::const_iterator levels{
					tags.value().find("building:levels")};

				if(height != tags.value().end())
					building.height = std::stof(height.value().get<std::string>());

				else if(levels != tags.value().end())
					building.height = LV::Constants::building_level_height*
						std::stof(levels.value().get<std::string>());
					
				else building.height = LV::Constants::default_building_height;

				building.height /= LV::Constants::meters_per_frustum_base_unit;

				// Get the geometry of "way" buildings.
				const nlohmann::json::const_iterator type{element.find("type")};
				if(type == element.end()) continue;

				if(type.value() == "way")
				{
					if(!get_building_outline(&building, element, scale_factor)) continue;
				}

				// Get the geometry of "relation" buildings.
				else if(type.value() == "relation")
				{
					const nlohmann::json::const_iterator members{element.find("members")};
					if(members == element.end()) continue;

					if(!get_building_outline(&building,
						members.value().begin().value(), scale_factor)) continue;
				}

				else continue;

				// Add the parsed building to the vector.
				buildings_data.emplace_back(building);
			}
			catch(...){ continue; }
		}
	}


	void generate_buildings_mesh()
	{
		std::cout<<"Generating the buildings mesh...\n";
		buildings_mesh.vertices.clear();
		buildings_mesh.indices.clear();

		// For each building...
		unsigned wall_index_base{1};
		for(const Building& building : buildings_data)
		{
			// Get the base height.
			const glm::ivec2 location{building.outline[0]};

			if(location.x >= terrain_data[0].size() ||
				location.y >= terrain_data.size() ||
				location.x < 0 || location.y < 0) continue;

			const float base_height{terrain_data[location.y][location.x]};

			// Generate the walls.
			unsigned roof_index_base{wall_index_base-1};
			for(unsigned index{}; index < building.outline.size(); ++index)
			{
				// Generate the vertices (top and bottom).
				const glm::fvec2 point{building.outline[index]};

				add_vertex(&buildings_mesh, glm::fvec3{point.x,
					base_height+building.height, point.y});

				add_vertex(&buildings_mesh, glm::fvec3{point.x,
					base_height-LV::Constants::building_depth/
					LV::Constants::meters_per_frustum_base_unit, point.y});

				// Generate the indicies.
				if(index >= building.outline.size()-1) continue;

				generate_square_indicies(&buildings_mesh.indices, wall_index_base-1,
					wall_index_base, wall_index_base+2, wall_index_base+1);

				wall_index_base += 2;
			}

			wall_index_base += 2;

			// Generate the roof.
			std::vector<std::vector<std::array<float, 2>>> earclip_data{1};
			for(const glm::fvec2& point : building.outline)
				earclip_data.back().push_back({point.x, point.y});

			std::vector<unsigned> roof_indices{mapbox::earcut<unsigned>(earclip_data)};
			for(unsigned roof_index : roof_indices)
				buildings_mesh.indices.emplace_back(roof_index_base+roof_index*2);
		}
	}


	void generate_side_mesh(bool iterate_x, bool extreme)
	{
		const int max{iterate_x ? size.x : size.y};
		const int static_value{extreme ? iterate_x ? size.y-1 : size.x-1 : 0};
		int z{static_value}, x{static_value};

		for(int index{}; index < max; ++index)
		{
			if(iterate_x) x = index; else z = index;

			// Generate the verticies (top and bottom).
			add_vertex(&base_mesh, glm::fvec3{x, terrain_data[z][x], z});
			add_vertex(&base_mesh, glm::fvec3{x, LV::Constants::bottom, z});

			// Generate the indicies.
			if(index >= max-1) continue;

			const unsigned base_index{static_cast<unsigned>(base_mesh.vertices.size()-2)};
			const bool couterclockwise{iterate_x ? extreme : !extreme};

			if(couterclockwise) generate_square_indicies(&base_mesh.indices,
				base_index, base_index+1, base_index+3, base_index+2);

			else generate_square_indicies(&base_mesh.indices,
				base_index+2, base_index+3, base_index+1, base_index);
		}
	}


	void generate_bottom_mesh()
	{
		const unsigned base_index{static_cast<unsigned>(base_mesh.vertices.size())};

		// Generate the vertices (top-left, bottom-left, bottom-right, top-right).
		add_vertex(&base_mesh, glm::fvec3{0.f, LV::Constants::bottom, size.y-1});
		add_vertex(&base_mesh, glm::fvec3{size.x-1, LV::Constants::bottom, size.y-1});
		add_vertex(&base_mesh, glm::fvec3{0.f, LV::Constants::bottom, 0.f});
		add_vertex(&base_mesh, glm::fvec3{size.x-1, LV::Constants::bottom, 0.f});

		// Generate the indicies.
		generate_square_indicies(&base_mesh.indices,
			base_index, base_index+2, base_index+3, base_index+1);
	}


	void generate_base_mesh()
	{
		base_mesh.vertices.clear();
		base_mesh.indices.clear();

		// Generate a mesh for each side.
		generate_side_mesh(true, false);
		generate_side_mesh(true, true);
		generate_side_mesh(false, false);
		generate_side_mesh(false, true);

		// Generate a mesh for the bottom.
		generate_bottom_mesh();
	}


	void generate_meshes()
	{
		center_matrix = glm::translate(glm::fvec3{-size.x/2.f, 0.f, -size.y/2.f});

		generate_terrain_mesh();
		generate_buildings_mesh();
		generate_base_mesh();
	}


	void save_compressed(const std::string& file_path, const std::string& data)
	{
		std::vector<uint8_t> compressed_data{LV::Utilities::compress(data)};

		std::ofstream file{file_path, std::ios::binary};
		if(!file) throw std::runtime_error{"Failed to save the Frustum."};

		file.write(reinterpret_cast<const char*>(
			compressed_data.data()), compressed_data.size());
	}


	std::string load_compressed(const std::string& file_path)
	{
		std::ifstream file{file_path, std::ios::binary};
		if(!file) throw std::runtime_error{"Failed to load the Frustum."};

		std::vector<uint8_t> compressed_data{
			std::istreambuf_iterator<char>(file),
			std::istreambuf_iterator<char>()};

		return LV::Utilities::decompress(compressed_data);
	}


	void save()
	{
		std::cout<<"Saving the generated Frustum...\n";
		const std::string directory{LV::Constants::frustum_directory_name+"/"+name+"/"};
		std::filesystem::create_directories(directory);

		// Save the metadata.
		std::ofstream metadata_file{directory+LV::Constants::metadata_file_name};
		metadata_file<<name<<'\n'<<dataset<<'\n'<<std::fixed<<std::setprecision(6)<<
			bounds.top<<' '<<bounds.left<<' '<<bounds.bottom<<' '<<bounds.right;

		// Save the terrain data.
		std::stringstream terrain_save_data;
		for(const std::vector<float>& row : terrain_data)
		{
			for(float point : row)
				terrain_save_data<<std::fixed<<std::setprecision(3)<<point<<' ';

			terrain_save_data<<'\n';
		}

		save_compressed(directory+LV::Constants::terrain_file_name,
			terrain_save_data.str());

		// Save the buildings data.
		std::stringstream buildings_save_data;
		for(const Building& building : buildings_data)
		{
			buildings_save_data<<std::defaultfloat<<building.height<<' ';

			for(const glm::fvec2& point : building.outline) buildings_save_data
				<<std::fixed<<std::setprecision(3)<<point.x<<' '<<point.y<<' ';

			buildings_save_data<<'\n';
		}

		save_compressed(directory+LV::Constants::buildings_file_name,
			buildings_save_data.str());
	}
}


void validate_coordinate(const std::string& direction, float coordinate, float maximum)
{
	if(std::abs(coordinate) > maximum) throw std::runtime_error{
		"The "+direction+" coordinate must not exceed "+std::to_string(maximum)+"."};
}


void LV::Frustum::generate(const std::string& name, const std::string& dataset,
	float top, float left, float bottom, float right)
{
	::name = name;
	::dataset = dataset;

	// Validate the coordinates.
	validate_coordinate("top", top, 80.f);
	validate_coordinate("bottom", bottom, 80.f);
	validate_coordinate("left", left, 180.f);
	validate_coordinate("right", right, 180.f);

	if(bottom > top) throw std::runtime_error{
		"The bottom coordinate is higher than the top coordinate."};

	if(left > right) throw std::runtime_error{
		"The left coordinate is father right than the right coordinate."};

	// Compensate for Mercator projection distortion.
	bounds = get_compensated_bounds(Bounds{top, left, bottom, right});

	// Retrieve the data.
	retrieve_terrain_data();
	retrieve_buildings_data();

	// Save the Frustum data.
	save();
	std::cout<<"Frustum generation complete.\n";
}


void LV::Frustum::load(const std::string& name)
{
	::name = name;

	std::cout<<"Loading the Frustum...\n";
	const std::string directory{LV::Constants::frustum_directory_name+"/"+name+"/"};
	std::string line;

	// Load the metadata.
	std::ifstream metadata_file{directory+LV::Constants::metadata_file_name};
	if(!metadata_file) throw std::runtime_error{"Failed to load the Frustum."};
	metadata_file>>::name>>dataset>>bounds.top>>bounds.left>>bounds.bottom>>bounds.right;

	// Load the terrain data.
	terrain_data.clear();
	std::stringstream terrain_save_data{load_compressed(
		directory+LV::Constants::terrain_file_name)};

	while(std::getline(terrain_save_data, line))
	{
		std::stringstream stream{line};
		terrain_data.emplace_back();

		float point;
		while(stream>>point) terrain_data.back().emplace_back(point);
	}

	size = {terrain_data[0].size(), terrain_data.size()};

	// Load the buildings data.
	buildings_data.clear();
	std::stringstream buildings_save_data{load_compressed(
		directory+LV::Constants::buildings_file_name)};

	while(std::getline(buildings_save_data, line))
	{
		std::stringstream stream{line};
		buildings_data.emplace_back();
		stream>>buildings_data.back().height;

		glm::fvec2 point;
		while(stream>>point.x>>point.y)
			buildings_data.back().outline.emplace_back(point);
	}

	// Generate the meshes.
	generate_meshes();
}


glm::ivec2 LV::Frustum::get_size(){ return size; }

LV::Mesh LV::Frustum::get_terrain_mesh(){ return terrain_mesh; }

LV::Mesh LV::Frustum::get_buildings_mesh(){ return buildings_mesh; }

LV::Mesh LV::Frustum::get_base_mesh(){ return base_mesh; }