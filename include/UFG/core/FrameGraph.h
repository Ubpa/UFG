#pragma once

#include "ResourceNode.h"
#include "PassNode.h"

#include <UGraphviz/UGraphviz.h>

#include <map>

namespace Ubpa::UFG {
	// add all resource nodes first
	// then add pass nodes
	class FrameGraph {
	public:
		FrameGraph(std::string name) : name{ std::move(name) } {}

		const std::string& Name() const noexcept { return name; }

		const std::vector<ResourceNode>& GetResourceNodes() const noexcept { return resourceNodes; }
		const std::vector<PassNode>& GetPassNodes() const noexcept { return passNodes; }

		bool IsRegisteredResourceNode(std::string_view name) const;
		bool IsRegisteredPassNode(std::string_view name) const;

		size_t GetResourceNodeIndex(std::string_view name) const;
		size_t GetPassNodeIndex(std::string_view name) const;

		size_t RegisterResourceNode(ResourceNode node);
		size_t RegisterResourceNode(std::string node);
		size_t RegisterPassNode(PassNode node);
		size_t RegisterPassNode(
			std::string name,
			std::vector<size_t> inputs,
			std::vector<size_t> outputs
		);
		template<size_t N, size_t M>
		size_t RegisterPassNode(
			std::string name,
			const std::array<std::string_view, N>& inputs_str,
			const std::array<std::string_view, M>& outputs_str
		);

		void Clear() noexcept;

		UGraphviz::Graph ToGraphvizGraph() const;
	private:
		std::string name;
		std::vector<ResourceNode> resourceNodes;
		std::vector<PassNode> passNodes;
		std::map<std::string, size_t, std::less<>> name2rsrcNodeIdx;
		std::map<std::string, size_t, std::less<>> name2passNodeIdx;
	};
}

#include "detail/FrameGraph.inl"
