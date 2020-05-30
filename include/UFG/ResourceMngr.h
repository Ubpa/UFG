#pragma once

#include "Resource.h"

#include <unordered_map>
#include <functional>

namespace Ubpa::FG {
	class ResourceMngr {
	public:
		Resource Request(const void* type, size_t state);
		void Recycle(const Resource& resource);
		virtual void Transition(Resource& resource, size_t state) = 0;
	protected:
		virtual void* Create(const void* type, size_t state) const = 0;
		virtual void Init(const void* type, void* ptr, size_t state) const = 0;
	private:
		std::unordered_map<const void*, std::vector<void*>> frees;
	};
}
