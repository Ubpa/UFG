#pragma once

#include <string>

namespace Ubpa::UFG {
	class ResourceNode {
	public:
		ResourceNode(std::string name)
			: name{ std::move(name) } {}

		const std::string& Name() const noexcept { return name; }
	private:
		std::string name;
	};
}
