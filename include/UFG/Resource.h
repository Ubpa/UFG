#pragma once

#include <string>

namespace Ubpa::FG {
	struct Resource {
		const void* type; // free resource can be reused by anothor resource with same type
		void* ptr; // pointer to real resource
		size_t state; // e.g. D3D12_RESOURCE_STATES
	};

	struct ResourceDecs {
		const void* type;
		std::string name;
		size_t state; // e.g. D3D12_RESOURCE_STATES
	};
}
