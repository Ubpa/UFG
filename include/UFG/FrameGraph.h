#pragma once

#include "Pass.h"

namespace Ubpa::FG {
	class FrameGraph {
	public:
		void AddPass(Pass pass) { passes.emplace_back(std::move(pass)); }
		const std::vector<Pass>& GetPasses() const noexcept { return passes; }
	private:
		std::vector<Pass> passes;
	};
}
