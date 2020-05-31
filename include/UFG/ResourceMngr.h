#pragma once

#include "Resource.h"

#include <unordered_map>
#include <functional>

namespace Ubpa::FG {
	class ResourceMngr {
	public:
		Resource Request(const void* raw_type, size_t raw_state, const void* impl_type);
		void Recycle(const void* raw_type, void* ptr_raw, size_t raw_state);

		virtual void Transition(void* ptr_raw, size_t from, size_t to) = 0;

		// return ptr_impl
		virtual void* RequestImpl(void* ptr_raw, const void* impl_type) = 0;
	protected:
		// return ptr_raw
		virtual void* CreateRaw(const void* raw_type, size_t raw_state) = 0;
		
	private:
		// raw_type -> (ptr_raw, raw_state)
		std::unordered_map<const void*, std::vector<std::tuple<void*, size_t>>> frees;
	};
}
