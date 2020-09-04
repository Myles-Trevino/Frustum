/*
	Copyright 2020 Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#include "Window.hpp"

#include <GLFW/glfw3.h>
#include <glbinding/gl33core/gl.h>
#include <globjects/globjects.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include "Constants.hpp"


namespace
{
	GLFWwindow* window;
	bool cursor_captured;
	bool held_keys[GLFW_KEY_LAST];
	bool pressed_keys[GLFW_KEY_LAST];
	bool negate_cursor_delta;
	glm::fvec2 cursor_position;
	glm::fvec2 cursor_delta;
	float scroll_delta;
	float previous_time;
	float delta;


	void glfw_error_callback(int code, const char* message)
	{ throw std::runtime_error{std::string{"GLFW Error: "}+message}; } }


	void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		if(action == GLFW_PRESS)
		{
			pressed_keys[key] = true;
			held_keys[key] = true;
		}

		if(action == GLFW_RELEASE) held_keys[key] = false;
	}


	void cursor_position_callback(GLFWwindow* window, double x_position, double y_position)
	{
		const glm::fvec2 new_cursor_position{x_position, y_position};

		if(negate_cursor_delta)
		{
			cursor_delta = glm::fvec2{0.f, 0.f};
			negate_cursor_delta = false;
		}
		else cursor_delta = new_cursor_position-cursor_position;

		cursor_position = new_cursor_position;
	}


	void scroll_callback(GLFWwindow* window, double x_offset, double y_offset)
	{ scroll_delta = static_cast<float>(y_offset); }



void LV::Window::create(int width, int height, const std::string& title)
{
	// Initialize GLFW.
	if(!glfwInit()) throw std::runtime_error{"Failed to initialize GLFW."};

	// Debugging.
	glfwSetErrorCallback(glfw_error_callback);
	if(!LV::Constants::opengl_logging) globjects::setLoggingHandler(nullptr);

	// Create the window.
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
	glfwWindowHint(GLFW_SAMPLES, LV::Constants::samples);

	window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
	if(!window) throw std::runtime_error{"GLFW failed to create a window."};

	// Set the window icons.
	GLFWimage icons[5];
	std::vector<std::string> icon_files{"16.png", "32.png", "48.png", "64.png", "96.png"};

	for(int index{}; index < icon_files.size(); ++index)
		icons[index].pixels = stbi_load(
			(LV::Constants::resources_directory+"/Icons/"+icon_files[index]).c_str(),
			&icons[index].width, &icons[index].height, nullptr, 4);

	glfwSetWindowIcon(window, static_cast<int>(icon_files.size()), icons);

	for(const GLFWimage& icon : icons) stbi_image_free(icon.pixels);

	// Initialize the OpenGL rendering context.
	glfwMakeContextCurrent(window);
	globjects::init([](const char* name){ return glfwGetProcAddress(name); });
	gl::glEnable(gl::GL_DEPTH_TEST);

	// Bind the input callbacks.
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetScrollCallback(window, scroll_callback);
}


void LV::Window::update()
{
	// Swap buffers.
	glfwSwapBuffers(window);

	// Clear the buffer.
	constexpr glm::fvec3 color{LV::Constants::clear_color};
	gl::glClearColor(color.r, color.g, color.b, 1.f);
	gl::glClear(gl::GL_COLOR_BUFFER_BIT|gl::GL_DEPTH_BUFFER_BIT);

	// Reset deltas and pressed keys.
	cursor_delta = glm::fvec2{0.f, 0.f};
	scroll_delta = 0.f;
	for(bool& key : pressed_keys) key = false;

	// Poll for window events.
	glfwPollEvents();

	// Update the delta.
	const float time{static_cast<float>(glfwGetTime())};
	delta = time-previous_time;
	previous_time = time;

	// Destroy if the Escape key is pressed.
	if(was_pressed(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, GLFW_TRUE);
}


void LV::Window::destroy()
{
	capture_cursor(false);
	glfwDestroyWindow(window);
	glfwPollEvents();
	glfwTerminate();
}


void LV::Window::capture_cursor(bool capture)
{
	glfwSetInputMode(window, GLFW_CURSOR, capture
		? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

	negate_cursor_delta = true;
	cursor_captured = capture;
}


bool LV::Window::is_open(){ return !glfwWindowShouldClose(window); }

bool LV::Window::is_cursor_captured(){ return cursor_captured; }

bool LV::Window::is_held(int key){ return held_keys[key]; }

bool LV::Window::was_pressed(int key){ return pressed_keys[key]; }


float LV::Window::get_delta(){ return delta; }

glm::ivec2 LV::Window::get_size()
{
	glm::ivec2 size;
	glfwGetFramebufferSize(window, &size.x, &size.y);
	return size;
}

bool LV::Window::is_minimized(){ return glfwGetWindowAttrib(window, GLFW_ICONIFIED); }

glm::fvec2 LV::Window::get_cursor_delta(){ return cursor_delta; }

float LV::Window::get_scroll_delta(){ return scroll_delta; }
