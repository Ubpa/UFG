#pragma once

#include <vector>
#include <string>

namespace Ubpa::FG {
	class Pass {
	public:
		Pass(std::string name,
			std::vector<std::string> inputs,
			std::vector<std::string> outputs)
			: name{ std::move(name) },
			inputs{ std::move(inputs) },
			outputs{ std::move(outputs) } {}

		const std::string& Name() const noexcept { return name; }
		const std::vector<std::string>& Inputs() const noexcept { return inputs; }
		const std::vector<std::string>& Outputs() const noexcept { return outputs; }

	private:
		std::string name;
		std::vector<std::string> inputs;
		std::vector<std::string> outputs;
	};
}