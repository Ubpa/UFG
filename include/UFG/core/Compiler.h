#pragma once

#include "FrameGraph.h"

#include <unordered_map>
#include <set>

namespace Ubpa::UFG {
	class Compiler {
	public:
		struct Result {
			struct RsrcInfo {
				size_t first{ static_cast<size_t>(-1) }; // index in sortedPass
				size_t last{ static_cast<size_t>(-1) }; // index in sortedPass

				std::vector<size_t> readers;
				size_t writer{ static_cast<size_t>(-1) };
			};

			struct PassInfo {
				std::vector<size_t> constructRsrcs;
				std::vector<size_t> destructRsrcs;
				std::vector<size_t> moveRsrcs; // src
			};

			struct PassGraph {
				void Clear() { adjList.clear(); }
				std::tuple<bool, std::vector<size_t>> TopoSort() const;
				std::unordered_map<size_t, std::set<size_t>> adjList;
				UGraphviz::Graph ToGraphvizGraph(const FrameGraph& fg) const;
			};

			std::unordered_map<size_t, RsrcInfo> rsrc2info;
			PassGraph passgraph;
			std::vector<size_t> sortedPasses;
			std::unordered_map<size_t, PassInfo> idx2info; // pass index to pass info
			std::unordered_map<size_t, size_t> moves_src2dst;
			std::unordered_map<size_t, size_t> moves_dst2src;
		};

		std::tuple<bool, Result> Compile(const FrameGraph& fg);
	};
}
