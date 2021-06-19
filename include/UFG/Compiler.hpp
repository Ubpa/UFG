#pragma once

#include "FrameGraph.hpp"

#include <unordered_map>
#include <set>
#include <optional>

namespace Ubpa::UFG {
	class Compiler {
	public:
		struct Result {
			struct RsrcInfo {
				size_t first{ static_cast<size_t>(-1) }; // index in sortedPass
				size_t last{ static_cast<size_t>(-1) }; // index in sortedPass

				// writer: the unique pass writing the resource
				// readers: the passes reading the resource
				std::vector<size_t> readers;
				size_t writer{ static_cast<size_t>(-1) };
			};

			struct PassGraph {
				void Clear() { adjList.clear(); }
				std::optional<std::vector<size_t>> TopoSort() const;
				std::unordered_map<size_t, std::set<size_t>> adjList;
				UGraphviz::Graph ToGraphvizGraph(const FrameGraph& fg) const;
			};

			std::unordered_map<size_t, RsrcInfo> rsrc2info;
			PassGraph passgraph;
			std::unordered_map<size_t, size_t> moves_src2dst;
			std::unordered_map<size_t, size_t> moves_dst2src;
		};

		std::optional<Result> Compile(const FrameGraph& fg);
	};
}
