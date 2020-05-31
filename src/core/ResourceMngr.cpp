#include <UFG/ResourceMngr.h>

using namespace Ubpa;
using namespace Ubpa::FG;
using namespace std;

Resource ResourceMngr::Request(const void* raw_type, size_t raw_state, const void* impl_type) {
	auto& tuples = frees[raw_type];

	void* ptr_raw;
	size_t raw_orig_state;

	if (tuples.empty()) {
		ptr_raw = CreateRaw(raw_type, raw_state);
		raw_orig_state = raw_state;
	}
	else
		tie(ptr_raw, raw_orig_state) = tuples.back();

	void* ptr_impl = RequestImpl(ptr_raw, impl_type);
	Resource rsrc{ ptr_raw, ptr_impl };

	if (raw_orig_state != raw_state)
		Transition(ptr_raw, raw_orig_state, raw_state);

	return rsrc;
}

void ResourceMngr::Recycle(const void* raw_type, void* ptr_raw, size_t raw_state) {
	frees[raw_type].emplace_back(ptr_raw, raw_state);
}
