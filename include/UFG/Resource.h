#pragma once

#include <string>

namespace Ubpa::FG {
	class Resource {
	public:
		// free resource can be reused by anothor resource with same type
		class Type {
		public:
			virtual ~Type() = default;
		};

		Resource(const Type* type, void* ptr, std::string name) : type{ type }, ptr{ ptr }, name{ std::move(name) } {}

		const std::string& Name() const noexcept { return name; }
		void* GetPtr() const noexcept { return ptr; }
		const Type* GetType() const noexcept { return type; }

	private:
		const Type* type;
		void* ptr;
		std::string name; // globally unique 
	};

	struct ResourceDecs {
		const Resource::Type* type;
		std::string name;
		size_t state;
	};
}
