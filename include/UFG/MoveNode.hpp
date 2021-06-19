#pragma once

namespace Ubpa::UFG {
	class MoveNode {
	public:
		MoveNode(size_t dst, size_t src)
			: dst{ dst }, src{ src } {}

		size_t GetDestinationNodeIndex() const noexcept { return dst; }
		size_t GetSourceNodeIndex() const noexcept { return src; }
	private:
		size_t dst;
		size_t src;
	};
}
