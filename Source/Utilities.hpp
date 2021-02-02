/*
	Copyright Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#pragma once

#include <string>
#include <vector>
#include <globjects/globjects.h>
#include <globjects/base/File.h>
#include <glm/glm.hpp>


namespace LV
{
	struct Shader
	{
		std::unique_ptr<globjects::File> vertex_file;
		std::unique_ptr<globjects::File> fragment_file;
		std::unique_ptr<globjects::Shader> vertex_shader;
		std::unique_ptr<globjects::Shader> fragment_shader;
		std::unique_ptr<globjects::Program> program;
	};

	struct VAO
	{
		std::unique_ptr<globjects::VertexArray> vao;
		std::unique_ptr<globjects::Buffer> vbo;
		std::unique_ptr<globjects::Buffer> ibo;
	};
}


namespace LV::Utilities
{
	// Platform-specific.
	void platform_initialization(const std::string& path);

	// OpenGL.
	void create_shader(Shader* shader, const std::string& name);

	void create_vao(VAO* vao, const Shader& shader,
		const std::vector<glm::fvec3>& vertices,
		const std::vector<unsigned>& indices, bool normals);

	void destroy_shader(Shader* shader);
	void destroy_vao(VAO* vao);


	// Compression.
	std::vector<uint8_t> compress(const std::string& source);

	std::string decompress(const std::vector<uint8_t>& source);

	// Streams.
	void ignore_until(std::istream* stream, char delimiter);

	// Strings.
	bool is_supported(const std::string& option,
		const std::vector<std::string>& supported_options);

	std::vector<std::string> split(const std::string& string);

	std::string to_uppercase(std::string string);
}
