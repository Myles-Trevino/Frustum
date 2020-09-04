/*
	Copyright 2020 Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#include "Utilities.hpp"

#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#elif __APPLE__
#include <unistd.h>
#endif
#include <glbinding/gl33core/gl.h>
#include <globjects/VertexAttributeBinding.h>
#include <zstd/zstd.h>

#include "Constants.hpp"


namespace
{
	template<typename T>
	std::unique_ptr<globjects::Buffer> create_buffer(const std::vector<T>& data)
	{
		std::unique_ptr<globjects::Buffer> buffer{globjects::Buffer::create()};
		buffer->setData(data, gl::GL_STATIC_DRAW);
		return buffer;
	}


	void bind_attribute(const LV::Shader& shader, const LV::VAO& vao, gl::GLuint index,
		const std::string& attribute, gl::GLint offset, gl::GLint stride, gl::GLint size)
	{
		globjects::VertexAttributeBinding* vertex_binding{vao.vao->binding(index)};
		vertex_binding->setAttribute(shader.program->getAttributeLocation(attribute));
		vertex_binding->setBuffer(vao.vbo.get(), offset, stride);
		vertex_binding->setFormat(size, gl::GL_FLOAT);
		vao.vao->enable(index);
	}


	#ifdef _WIN32
	void set_icon(HINSTANCE module_handle, HWND console_handle, WPARAM type, int size)
	{
		// Note: An icon resource must be added for Windows builds.
		// https://docs.microsoft.com/en-us/windows/win32/menurc/about-resource-files
		HANDLE icon{LoadImage(module_handle, "MAINICON", IMAGE_ICON, size, size, LR_SHARED)};
		if(!icon) throw std::exception{"Failed to load the icon."};
		SendMessage(console_handle, WM_SETICON, type, reinterpret_cast<LPARAM>(icon));
	}
	#endif
}


void LV::Utilities::platform_initialization(const std::string& path)
{
	#ifdef _WIN32
	// Set the icon.
	const HINSTANCE module_handle{GetModuleHandle(NULL)};
	HWND console_handle{GetConsoleWindow()};
	set_icon(module_handle, console_handle, ICON_SMALL, 32);
	set_icon(module_handle, console_handle, ICON_BIG, 64);
	
	#elif __APPLE__
	// Set the working directory.
	std::string directory{path.substr(0, path.find_last_of('/'))};
	chdir(directory.c_str());
	#endif
}


void LV::Utilities::create_shader(Shader* shader, const std::string& name)
{
	// Load the shader files.
	shader->vertex_file = globjects::Shader::sourceFromFile(
		LV::Constants::resources_directory+"/Shaders/"+name+".vertex");

	shader->fragment_file = globjects::Shader::sourceFromFile(
		LV::Constants::resources_directory+"/Shaders/"+name+".fragment");

	// Create the shaders.
	shader->vertex_shader = globjects::Shader::create(
		gl::GL_VERTEX_SHADER, shader->vertex_file.get());

	shader->fragment_shader = globjects::Shader::create(
		gl::GL_FRAGMENT_SHADER, shader->fragment_file.get());

	// Create the shader program.
	shader->program = globjects::Program::create();
	shader->program->attach(shader->vertex_shader.get(), shader->fragment_shader.get());
}


void LV::Utilities::create_vao(VAO* vao, const Shader& shader,
	const std::vector<glm::fvec3>& vertices,
	const std::vector<unsigned>& indices, bool normals)
{
	// Generate the VBO and IBO.
	vao->vbo = create_buffer(vertices);
	vao->ibo = create_buffer(indices);

	// Generate the VAO.
	vao->vao = globjects::VertexArray::create();
	vao->vao->bindElementBuffer(vao->ibo.get());

	gl::GLint stride{static_cast<gl::GLint>(
		sizeof(glm::fvec3)*(normals ? 2 : 1))};
	bind_attribute(shader, *vao, 0, "input_vertex", 0, stride, 3);
	if(normals) bind_attribute(shader, *vao, 1,
		"input_normal", sizeof(glm::fvec3), stride, 3);
}


void LV::Utilities::destroy_shader(Shader* shader)
{
	shader->program.reset();
	shader->fragment_shader.reset();
	shader->vertex_shader.reset();
	shader->fragment_file.reset();
	shader->vertex_file.reset();
}


void LV::Utilities::destroy_vao(VAO* vao)
{
	vao->vao.reset();
	vao->ibo.reset();
	vao->vbo.reset();
}


std::vector<uint8_t> LV::Utilities::compress(const std::string& source)
{
	// Allocate the destination buffer.
	const size_t buffer_size{ZSTD_compressBound(source.size())};
	uint8_t* buffer{new uint8_t[buffer_size]};

	// Compress.
	const size_t compressed_size{ZSTD_compress(buffer, buffer_size,
		source.c_str(), source.size(), ZSTD_maxCLevel())};

	if(ZSTD_isError(compressed_size)) throw std::runtime_error{"Failed to compress."};

	// Return the result.
	const std::vector<uint8_t> result{buffer, buffer+compressed_size};
	delete[] buffer;

	return result;
}


std::string LV::Utilities::decompress(const std::vector<uint8_t>& source)
{
	// Allocate the destination buffer.
	const unsigned long long buffer_size{
		ZSTD_getFrameContentSize(source.data(), source.size())};

	if(ZSTD_isError(buffer_size)) throw std::runtime_error{"Failed to decompress."};

	uint8_t* buffer{new uint8_t[buffer_size]};

	// Decompress.
	const size_t decompressed_size{ZSTD_decompress(buffer,
		buffer_size, source.data(), source.size())};

	if(ZSTD_isError(decompressed_size))
		throw std::runtime_error{"Failed to decompress."};

	// Return the result.
	const std::string result{buffer, buffer+decompressed_size};
	delete[] buffer;

	return result;
}


void LV::Utilities::ignore_until(std::istream* stream, char delimiter)
{ stream->ignore(std::numeric_limits<std::streamsize>::max(), delimiter); }


bool LV::Utilities::is_supported(const std::string& option,
	const std::vector<std::string>& supported_options)
{
	for(const std::string& supported_option : supported_options)
		if(option == supported_option) return true;

	return false;
}


std::vector<std::string> LV::Utilities::split(const std::string& string)
{
	std::istringstream stream(string);

	return {std::istream_iterator<std::string>(stream), 
		std::istream_iterator<std::string>()};
}
