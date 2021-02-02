/*
	Copyright Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#pragma once

#include <string>


namespace LV::Request
{
	std::string request(const std::string& url, const std::string& payload = {});
}
