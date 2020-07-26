#pragma once

namespace Ubpa::UFG {
	class MoveNode {
	public:
		MoveNode(size_t dst, size_t src)
			: dst{ dst }, src{ src } {}

		static constexpr const char name[] = "Move";

		const size_t& GetDestinationNodeIndex() const noexcept { return dst; }
		const size_t& GetSourceNodeIndex() const noexcept { return src; }
	private:
		size_t dst;
		size_t src;
	};
}
