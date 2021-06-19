#pragma once

#include <string>
#include <vector>
#include <span>

namespace Ubpa::UFG {
	class PassNode {
	public:
		PassNode(std::string name,
			std::vector<size_t> inputs,
			std::vector<size_t> outputs)
			: name{ std::move(name) },
			inputs{ std::move(inputs) },
			outputs{ std::move(outputs) } {}

		std::string_view Name() const noexcept { return name; }
		std::span<const size_t> Inputs() const noexcept { return inputs; }
		std::span<const size_t> Outputs() const noexcept { return outputs; }

	private:
		std::string name;
		std::vector<size_t> inputs;
		std::vector<size_t> outputs;
	};
}
