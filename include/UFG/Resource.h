#pragma once

#include <string>

namespace Ubpa::FG {
	class Resource {
	public:
		Resource(const void* type, void* ptr, std::string name) : type{ type }, ptr{ ptr }, name{ std::move(name) } {}

		const std::string& Name() const noexcept { return name; }
		void* GetPtr() const noexcept { return ptr; }
		const void* GetType() const noexcept { return type; }

	private:
		const void* type;
		void* ptr;
		std::string name; // globally unique 
	};

	struct ResourceDecs {
		// free resource can be reused by anothor resource with same type
		const void* type;
		std::string name;
		size_t state;
	};
}
