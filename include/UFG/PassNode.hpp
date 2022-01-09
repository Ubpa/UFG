#pragma once

#include <string>
#include <vector>
#include <span>
#include <typeinfo>
#include <cassert>

namespace Ubpa::UFG {
	class PassNode {
	public:
		enum class Type { General, Copy };

		PassNode(Type type,
			std::string name,
			std::vector<size_t> inputs,
			std::vector<size_t> outputs)
			: type{ type }
			, name{ std::move(name) }
			, inputs{ std::move(inputs) }
			, outputs{ std::move(outputs) }
		{ assert(IsValid()); }

		bool IsValid() const noexcept {
			if (name.empty())
				return false;

			if (type == Type::Copy) {
				if (inputs.size() != outputs.size())
					return false;
			}

			return true;
		}

		Type GetType() const noexcept { return type; }
		std::string_view Name() const noexcept { return name; }
		std::span<const size_t> Inputs() const noexcept { return inputs; }
		std::span<const size_t> Outputs() const noexcept { return outputs; }

	protected:
		Type type;
		std::string name;
		std::vector<size_t> inputs;
		std::vector<size_t> outputs;
	};
}
