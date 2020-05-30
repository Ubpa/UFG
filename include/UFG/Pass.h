#pragma once

#include "Resource.h"

#include <vector>
#include <map>
#include <functional>

namespace Ubpa::FG {
	class Pass {
	public:
		Pass(std::vector<ResourceDecs> inputs,
			std::vector<ResourceDecs> outputs,
			std::function<void(const std::map<std::string, const Resource*>&)> func,
			std::string name)
			: inputs{ std::move(inputs) },
			outputs{ std::move(outputs) },
			func{ std::move(func) },
			name{ std::move(name) }{}

		void Execute(const std::map<std::string, const Resource*>& resources) const { func(resources); }

		const std::string& Name() const noexcept { return name; }
		const std::vector<ResourceDecs>& Inputs() const noexcept { return inputs; }
		const std::vector<ResourceDecs>& Outputs() const noexcept { return outputs; }

	private:
		std::vector<ResourceDecs> inputs;
		std::vector<ResourceDecs> outputs;
		std::function<void(const std::map<std::string, const Resource*>&)> func;
		std::string name;
	};
}