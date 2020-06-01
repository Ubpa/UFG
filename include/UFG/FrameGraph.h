#pragma once

#include "ResourceNode.h"
#include "PassNode.h"

#include <unordered_map>

namespace Ubpa::FG {
	class FrameGraph {
	public:
		size_t AddResourceNode(std::string name) {
			size_t idx = rsrcNodes.size();
			rsrcname2idx[name] = idx;
			rsrcNodes.emplace_back(std::move(name));
			return idx;
		}

		void AddPassNode(PassNode passnode) { passnodes.emplace_back(std::move(passnode)); }

		const std::vector<PassNode>& GetPassNodes() const noexcept { return passnodes; }
		size_t GetResourceNodeIndex(const std::string& name) const noexcept { return rsrcname2idx.find(name)->second; }
		const std::vector<ResourceNode>& GetResourceNodes() const noexcept { return rsrcNodes; }
	private:
		std::vector<ResourceNode> rsrcNodes;
		std::unordered_map<std::string, size_t> rsrcname2idx;
		std::vector<PassNode> passnodes;
	};
}
