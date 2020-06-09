#pragma once

#include "ResourceNode.h"
#include "PassNode.h"

#include <UGraphviz/UGraphviz.h>

#include <unordered_map>

namespace Ubpa::FG {
	// add all resource nodes first
	// then add pass nodes
	class FrameGraph {
	public:
		std::string Name;

		size_t AddResourceNode(std::string name);

		size_t AddPassNode(
			std::string name,
			std::vector<size_t> inputs,
			std::vector<size_t> outputs);

		template<size_t N, size_t M>
		size_t AddPassNode(
			std::string name,
			const std::array<std::string, N>& inputs_str,
			const std::array<std::string, M>& outputs_str);

		size_t GetResourceNodeIndex(const std::string& name) const noexcept { return rsrcname2idx.find(name)->second; }
		size_t GetPassNodeIndex(const std::string& name) const noexcept { return passnodename2idx.find(name)->second; }

		const std::vector<PassNode>& GetPassNodes() const noexcept { return passNodes; }
		const std::vector<ResourceNode>& GetResourceNodes() const noexcept { return rsrcNodes; }

		void Clear();

		Graphviz::Graph ToGraphvizGraph() const;

	private:
		std::unordered_map<std::string, size_t> rsrcname2idx;
		std::unordered_map<std::string, size_t> passnodename2idx;
		std::vector<ResourceNode> rsrcNodes;
		std::vector<PassNode> passNodes;
	};
}

#include "detail/FrameGraph.inl"
