/*
	Copyright 2020 Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#pragma once

#include <string>
#include <glm/glm.hpp>


namespace LV::Window
{
	void create(int width, int height, const std::string& title);

	void update();

	void destroy();

	void capture_cursor(bool capture);

	// Getters.
	bool is_open();

	bool is_cursor_captured();

	bool is_held(int key);

	bool was_pressed(int key);

	bool is_minimized();

	float get_delta();

	float get_scroll_delta();

	glm::fvec2 get_cursor_delta();

	glm::ivec2 get_size();
}
