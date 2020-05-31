#pragma once

#include "Resource.h"

#include <vector>
#include <map>
#include <functional>

namespace Ubpa::FG {
	class Pass {
	public:
		Pass(std::vector<Named<ResourceImplDesc>> inputs,
			std::vector<Named<ResourceImplDesc>> outputs,
			std::function<void(const std::map<std::string, Resource>&)> func,
			std::string name)
			: inputs{ std::move(inputs) },
			outputs{ std::move(outputs) },
			func{ std::move(func) },
			name{ std::move(name) }{}

		void Execute(const std::map<std::string, Resource>& resources) const { func(resources); }

		const std::string& Name() const noexcept { return name; }
		const std::vector<Named<ResourceImplDesc>>& Inputs() const noexcept { return inputs; }
		const std::vector<Named<ResourceImplDesc>>& Outputs() const noexcept { return outputs; }

	private:
		std::vector<Named<ResourceImplDesc>> inputs;
		std::vector<Named<ResourceImplDesc>> outputs;
		std::function<void(const std::map<std::string, Resource>&)> func;
		std::string name;
	};
}