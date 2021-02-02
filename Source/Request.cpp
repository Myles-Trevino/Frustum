/*
	Copyright Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#include "Request.hpp"

#include <stdexcept>
#include <curl/curl.h>

#include "Constants.hpp"


namespace
{
	size_t write_callback(void* buffer, size_t element_size,
		size_t element_count, std::string* string)
	{
		const size_t buffer_size{element_size*element_count};
		const size_t previous_size{string->size()};

		string->resize(previous_size+buffer_size);
		std::copy(static_cast<char*>(buffer), static_cast<char*>(buffer)+
			buffer_size, string->begin()+previous_size);

		return buffer_size;
	}
}


std::string LV::Request::request(const std::string& url, const std::string& payload)
{
	// Get a cURL handle.
	std::string response;
	char error_buffer[CURL_ERROR_SIZE]{};

	CURL* curl_handle{curl_easy_init()};
	if(!curl_handle) throw std::runtime_error{"Failed to initialize cURL."};

	// Set the cURL options.
	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl_handle, CURLOPT_CAINFO,
		(LV::Constants::resources_directory+"/Certificates.pem").c_str());
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, true);

	if(!payload.empty()) curl_easy_setopt(curl_handle,
		CURLOPT_POSTFIELDS, payload.c_str());

	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &response);

	curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, error_buffer);
	if(LV::Constants::curl_verbose) curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, true);

	// Perform the request.
	const CURLcode curl_result{curl_easy_perform(curl_handle)};

	if(curl_result != CURLE_OK) throw std::runtime_error{
		"The cURL request failed. Code: "+std::to_string(curl_result)+"."};

	// Free the cURL resources.
	curl_easy_cleanup(curl_handle);

	// Return.
	return response;
}
