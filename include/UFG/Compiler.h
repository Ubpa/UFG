#pragma once

#include "FrameGraph.h"

#include <unordered_map>
#include <set>

namespace Ubpa::FG {
	class Compiler {
	public:
		struct Result {
			struct RsrcInfo {
				// first == writer
				// size_t first; // index in sortedPass
				size_t last; // index in sortedPass

				std::vector<size_t> readers;
				size_t writer;
			};

			struct PassInfo {
				std::vector<std::string> constructRsrcs;
				std::vector<std::string> destructRsrcs;
			};

			struct PassGraph {
				void Clear() { adjList.clear(); }
				std::tuple<bool, std::vector<size_t>> TopoSort() const;
				std::unordered_map<size_t, std::set<size_t>> adjList;
			};

			std::unordered_map<std::string, RsrcInfo> rsrc2info;
			PassGraph passgraph;
			std::vector<size_t> sortedPasses;
			std::unordered_map<size_t, PassInfo> idx2info; // pass index to pass info
		};

		std::tuple<bool, Result> Compile(const FrameGraph& fg);
	};
}
