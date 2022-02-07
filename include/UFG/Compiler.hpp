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
				size_t first{ static_cast<size_t>(-1) }; // index in sorted_passes
				size_t last{ static_cast<size_t>(-1) }; // index in sorted_passes

				// writer: the unique pass writing the resource
				// readers: the passes reading/copy-out the resource
				// copy_in: the unique pass copy-in the resource
				size_t writer{ static_cast<size_t>(-1) };
				std::vector<size_t> readers;
				size_t copy_in{ static_cast<size_t>(-1) };
			};
			struct PassInfo {
				std::vector<size_t> construct_resources;
				std::vector<size_t> destruct_resources;
				std::vector<size_t> move_resources;
			};

			struct PassGraph {
				void Clear() { adjList.clear(); }
				std::optional<std::vector<size_t>> TopoSort() const;
				std::unordered_map<size_t, std::set<size_t>> adjList;
				UGraphviz::Graph ToGraphvizGraph(const FrameGraph& fg) const;
			};

			std::vector<RsrcInfo> rsrcinfos;
			PassGraph passgraph;
			std::vector<size_t> sorted_passes;
			std::unordered_map<size_t, PassInfo> pass2info;
			std::vector<size_t> pass2order;
			std::unordered_map<size_t, size_t> moves_src2dst;
			std::unordered_map<size_t, size_t> moves_dst2src;
			std::unordered_map<size_t, size_t> copys_src2dst;
			std::unordered_map<size_t, size_t> copys_dst2src;
		};

		// throw std::logic_error when compilation failing
		Result Compile(const FrameGraph& fg);
	};
}
