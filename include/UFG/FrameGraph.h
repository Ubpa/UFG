#pragma once

#include "ResourceNode.h"
#include "PassNode.h"

#include <unordered_map>

namespace Ubpa::FG {
	class FrameGraph {
	public:
		size_t AddResourceNode(ResourceNode rsrcNode) {
			size_t idx = rsrcNodes.size();
			rsrcname2idx[rsrcNode.Name()] = idx;
			rsrcNodes.push_back(std::move(rsrcNode));
			return idx;
		}
		size_t AddResourceNode(std::string name) { return AddResourceNode(ResourceNode{ std::move(name) }); }
		void AddPassNode(PassNode passnode) { passnodes.emplace_back(std::move(passnode)); }

		const std::vector<PassNode>& GetPassNodes() const noexcept { return passnodes; }
		size_t GetResourceNodeIndex(const std::string& name) const noexcept { return rsrcname2idx.find(name)->second; }
		const ResourceNode& GetResourceNode(size_t i) const noexcept { return rsrcNodes[i]; }
	private:
		std::vector<ResourceNode> rsrcNodes;
		std::unordered_map<std::string, size_t> rsrcname2idx;
		std::vector<PassNode> passnodes;
	};
}
