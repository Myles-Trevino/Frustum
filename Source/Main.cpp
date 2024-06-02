/*
	Copyright Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#include <fstream>
#include <iostream>
#include <regex>
#include <curl/curl.h>

#include "Constants.hpp"
#include "Utilities.hpp"
#include "Frustum.hpp"
#include "Viewer.hpp"
#include "Exporter.hpp"


void print_documentation()
{
	std::cout<<"\nTo generate a Frustum, enter: 'generate <name> <terrain dataset> <top> "
		"<left> <bottom> <right>'. The name must contain only alphanumeric characters "
		"and dashes. Supported global datasets are: 'srtmgl1' or 'aw3d30'. Supported USGS "
		"datasets are: 'usgs30m', 'usgs10m', and 'usgs1m'. For example: 'generate st-gallen "
		"srtmgl1 47.327618 9.295821 47.126480 9.621767'. Generated Frustums will be saved "
		"within the 'Frustums' folder."

		<<"\n\nTo view a generated Frustum, enter: 'view <name>'. For example: 'view "
		"st-gallen'. In the Viewer, navigate using the 'W', 'A', 'S', and 'D' keys and the  "
		"mouse. Hold 'Shift' to move faster. Press 'L' to toggle mouse locking. Press the "
		"'F' key to toggle wireframe rendering. Use the left and right arrow keys to change "
		"the light direction. Use the scrollwheel to change the FOV. Press the 'Esc' "
		"key to close the Viewer."

		<<"\n\nTo export a generated Frustum as a 3D model, enter: 'export <name> <format> "
		"<orientation>'. Valid export formats are: 'ply', 'obj', and 'stl'. The orientation "
		"can be either 'z-up' or 'y-up'. For example: 'export st-gallen stl z-up'. Exported "
		"models will be saved within the 'Exports' folder. STL is not recommended for large "
		"exports. Exporting as OBJ will generate a corresponding MTL file."

		<<"\n\nTo exit, enter 'exit'."

		<<"\n\nFor detailed documentation, visit laventh.com.\n";
}


void print_startup_message()
{
	std::cout<<LV::Constants::program_name<<" "<<LV::Constants::program_version
		<<"\nCopyright Myles Trevino"
		<<"\nlaventh.com"

		<<"\n\nLicensed under the Apache License, Version 2.0"
		<<"\nhttps://www.apache.org/licenses/LICENSE-2.0"

		<<"\n\nEnter 'help' for documentation.\n";
}


void validate_command_parameters(const std::string& command, int required, size_t given)
{
	if(given != required) throw std::runtime_error{"'"+command+"' requires "
		+std::to_string(required)+" parameters but "+std::to_string(given)+" were given."};
}


void validate_name(const std::string& name)
{
	if(!std::regex_match(name, std::regex{"^[a-zA-Z0-9-]+$"}))
		throw std::runtime_error{"Invalid Frustum name. The name must "
			"consist only of alphanumeric characters and dashes."};
}


int main(int arguments_count, const char* arguments[])
{
	std::string api_key;
	std::string input;
	
	try
	{
		// Initialize.
		print_startup_message();
		
		LV::Utilities::platform_initialization(arguments[0]);
		curl_global_init(CURL_GLOBAL_ALL);

		// Check for the OpenTopography API key.
		const int api_key_length{32};
		std::ifstream file{LV::Constants::resources_directory+"/OpenTopography API Key.txt"};
		file>>api_key;

		if(api_key.size() != api_key_length)
		{
			std::cout<<"\nPlease enter your OpenTopography API key into \"OpenTopography "
				"API Key.txt\" in the \"Resources\" folder next to the executable. To get "
				"an API key, go to: https://portal.opentopography.org/myopentopo.\n\n"
				"Press enter to exit.";
		
			std::getline(std::cin, input);
			std::exit(EXIT_FAILURE);
		}
	}
	catch(std::exception& error){ std::cout<<"\nInitialization Error: "<<error.what()<<'\n'; }
	catch(...){ std::cout<<"\nInitialization Error: Unhandled exception.\n"; }

	// Main loop.
	while(true)
	{
		try
		{
			// Prompt for input.
			std::cout<<"\n> ";
			std::getline(std::cin, input);

			// Get the tokens and command name.
			std::vector<std::string> tokens{LV::Utilities::split(input)};
			if(tokens.empty()) throw std::runtime_error{"No command entered."};

			std::string command_name{tokens[0]};
			tokens.erase(tokens.begin());

			// Parse and execute the command.
			if(command_name == "generate")
			{
				validate_command_parameters(command_name, 6, tokens.size());
				validate_name(tokens[0]);
				LV::Frustum::generate(tokens[0], tokens[1], std::stof(tokens[2]),
					std::stof(tokens[3]), std::stof(tokens[4]), std::stof(tokens[5]),
					api_key);
			}

			else if(command_name == "view")
			{
				validate_command_parameters(command_name, 1, tokens.size());
				validate_name(tokens[0]);
				LV::Viewer::view(tokens[0]);
			}

			else if(command_name == "export")
			{
				validate_command_parameters(command_name, 3, tokens.size());
				validate_name(tokens[0]);
				LV::Exporter::export_frustum(tokens[0], tokens[1], tokens[2]);
			}

			else if(command_name == "exit")
			{
				std::cout<<"Exiting...\n";
				break;
			}

			else if(command_name == "help") print_documentation();
			else throw std::runtime_error{"Unrecognized command."};
		}
		catch(std::exception& error){ std::cout<<"Error: "<<error.what()<<'\n'; }
		catch(...){ std::cout<<"Error: Unhandled exception.\n"; }
	}

	// Destroy.
	curl_global_cleanup();
}
