#pragma once

#include "ResourceNode.h"
#include "PassNode.h"
#include "MoveNode.h"

#include <UGraphviz/UGraphviz.h>

#include <map>
#include <unordered_map>

namespace Ubpa::UFG {
	// 1. Add all resource nodes first, then add pass nodes
	// 2. write means (read + write)
	// 3. resource lifecycle: move in -> write -> reads -> move out
	class FrameGraph {
	public:
		FrameGraph(std::string name) : name{ std::move(name) } {}

		const std::string& Name() const noexcept { return name; }

		const std::vector<ResourceNode>& GetResourceNodes() const noexcept { return resourceNodes; }
		const std::vector<PassNode>& GetPassNodes() const noexcept { return passNodes; }
		const std::vector<MoveNode>& GetMoveNodes() const noexcept { return moveNodes; }

		bool IsRegisteredResourceNode(std::string_view name) const;
		bool IsRegisteredPassNode(std::string_view name) const;
		bool IsRegisteredMoveNode(size_t dst, size_t src) const;
		bool IsMovedOut(size_t src) const;
		bool IsMovedIn(size_t dst) const;

		size_t GetResourceNodeIndex(std::string_view name) const;
		size_t GetPassNodeIndex(std::string_view name) const;
		size_t GetMoveNodeIndex(size_t dst, size_t src) const;
		size_t GetMoveSourceNodeIndex(size_t dst) const;
		size_t GetMoveDestinationNodeIndex(size_t src) const;

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
		size_t RegisterMoveNode(MoveNode node);
		size_t RegisterMoveNode(size_t dst, size_t src);

		void Clear() noexcept;

		UGraphviz::Graph ToGraphvizGraph() const;
	private:
		std::string name;
		std::vector<ResourceNode> resourceNodes;
		std::vector<PassNode> passNodes;
		std::vector<MoveNode> moveNodes;
		std::map<std::string, size_t, std::less<>> name2rsrcNodeIdx;
		std::map<std::string, size_t, std::less<>> name2passNodeIdx;
		std::unordered_map<size_t, size_t> srcRsrcNodeIdx2moveNodeIdx;
		std::unordered_map<size_t, size_t> dstRsrcNodeIdx2moveNodeIdx;
	};
}

#include "detail/FrameGraph.inl"
