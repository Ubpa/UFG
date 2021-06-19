#pragma once

#include <string>

namespace Ubpa::UFG {
	class ResourceNode {
	public:
		ResourceNode(std::string name)
			: name{ std::move(name) } {}

		std::string_view Name() const noexcept { return name; }
	private:
		std::string name;
	};
}
