/*
	Copyright Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#pragma once

#include <string>


namespace LV::Exporter
{
	void export_frustum(const std::string& name, const std::string& format,
		const std::string& orientation);
}
