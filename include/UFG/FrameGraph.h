#pragma once

#include "ResourceNode.h"
#include "PassNode.h"

#include <unordered_map>

namespace Ubpa::FG {
	// add all resource nodes first
	// then add pass nodes
	class FrameGraph {
	public:
		size_t AddResourceNode(std::string name) {
			size_t idx = rsrcNodes.size();
			rsrcname2idx[name] = idx;
			rsrcNodes.emplace_back(std::move(name));
			return idx;
		}

		size_t AddPassNode(
			std::string name,
			std::vector<size_t> inputs,
			std::vector<size_t> outputs)
		{
			size_t idx = passNodes.size();
			passnodename2idx[name] = idx;
			passNodes.emplace_back(std::move(name), std::move(inputs), std::move(outputs));
			return idx;
		}

		template<size_t N, size_t M>
		size_t AddPassNode(
			std::string name,
			const std::array<std::string, N>& inputs_str,
			const std::array<std::string, M>& outputs_str)
		{
			std::vector<size_t> inputs(N);
			std::vector<size_t> outputs(M);

			for (size_t i = 0; i < N; i++)
				inputs[i] = GetPassNodeIndex(inputs_str[i]);
			for (size_t i = 0; i < M; i++)
				outputs[i] = GetPassNodeIndex(outputs_str[i]);

			return AddPassNode(std::move(name), std::move(inputs), std::move(outputs));
		}

		size_t GetResourceNodeIndex(const std::string& name) const noexcept { return rsrcname2idx.find(name)->second; }
		size_t GetPassNodeIndex(const std::string& name) const noexcept { return passnodename2idx.find(name)->second; }
		const std::vector<PassNode>& GetPassNodes() const noexcept { return passNodes; }
		const std::vector<ResourceNode>& GetResourceNodes() const noexcept { return rsrcNodes; }

		void Clear() {
			rsrcname2idx.clear();
			passnodename2idx.clear();
			rsrcNodes.clear();
			passNodes.clear();
		}

	private:
		std::unordered_map<std::string, size_t> rsrcname2idx;
		std::unordered_map<std::string, size_t> passnodename2idx;
		std::vector<ResourceNode> rsrcNodes;
		std::vector<PassNode> passNodes;
	};
}
