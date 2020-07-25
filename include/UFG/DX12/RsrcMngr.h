#pragma once

#include "Rsrc.h"

#include "../core/core.h"

#include <unordered_set>

namespace Ubpa::UFG::DX12 {
	// manage per pass resources
	// CBV, SRV, UAV, RTV, DSV
	class RsrcMngr {
	public:
		~RsrcMngr();

		void Init(UDX12::GCmdList uGCmdList, UDX12::Device uDevice) {
			this->uGCmdList = uGCmdList;
			this->uDevice = uDevice;
			csuDynamicDH = new UDX12::DynamicSuballocMngr{
				*UDX12::DescriptorHeapMngr::Instance().GetCSUGpuDH(),
				256,
				"RsrcMngr::csuDynamicDH" };
		}

		void NewFrame();
		void DHReserve();
		void Clear();

		void AllocateHandle();

		void Construct(size_t rsrcNodeIdx);
		void Destruct(size_t rsrcNodeIdx);

		PassRsrcs RequestPassRsrcs(size_t passNodeIdx);

		RsrcMngr& RegisterImportedRsrc(size_t rsrcNodeIdx, SRsrcView view) {
			importeds[rsrcNodeIdx] = view;
			return *this;
		}

		RsrcMngr& RegisterTemporalRsrc(size_t rsrcNodeIdx, RsrcType type) {
			temporals[rsrcNodeIdx] = type;
			return *this;
		}

		RsrcMngr& RegisterPassRsrcs(size_t passNodeIdx, size_t rsrcNodeIdx, RsrcState state, RsrcImplDesc desc) {
			passNodeIdx2rsrcs[passNodeIdx].emplace_back(rsrcNodeIdx, state, desc);
			return *this;
		}

		RsrcMngr& RegisterRsrcHandle(
			size_t rsrcNodeIdx,
			RsrcImplDesc desc,
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
			D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = { 0 });

		// only support CBV, SRV, UAV
		RsrcMngr& RegisterRsrcTable(const std::vector<std::tuple<size_t, RsrcImplDesc>>& rsrcNodeIndices);

		bool IsImported(size_t rsrcNodeIdx) const noexcept {
			return importeds.find(rsrcNodeIdx) != importeds.end();
		}

	private:
		// CSU : CBV, CSU, UAV

		void CSUDHReserve(UINT num);
		void DsvDHReserve(UINT num);
		void RtvDHReserve(UINT num);

		UDX12::GCmdList uGCmdList;
		UDX12::Device uDevice;

		// type -> vector<view>
		std::vector<RsrcPtr> rsrcKeeper;
		std::unordered_map<RsrcType, std::vector<SRsrcView>> pool;

		// rsrcNodeIdx -> view
		std::unordered_map<size_t, SRsrcView> importeds;

		// rsrcNodeIdx -> type
		std::unordered_map<size_t, RsrcType> temporals;
		// passNodeIdx -> vector<(rsrcNodeIdx, RsrcState, RsrcImplDesc)>
		std::unordered_map<size_t, std::vector<std::tuple<size_t, RsrcState, RsrcImplDesc>>> passNodeIdx2rsrcs;
		// rsrcNodeIdx -> view
		std::unordered_map<size_t, SRsrcView> actives;

		struct DHHandles {
			struct CpuGpuInfo {
				D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{ 0 };
				D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{ 0 };
				bool init{ false };
			};
			struct CpuInfo {
				D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{ 0 };
				bool init{ false };
			};
			std::unordered_map<D3D12_CONSTANT_BUFFER_VIEW_DESC, CpuGpuInfo>         desc2info_cbv;
			std::unordered_map<D3D12_SHADER_RESOURCE_VIEW_DESC, CpuGpuInfo>         desc2info_srv;
			std::unordered_map<D3D12_UNORDERED_ACCESS_VIEW_DESC, CpuGpuInfo>         desc2info_uav;

			std::unordered_map<D3D12_RENDER_TARGET_VIEW_DESC, CpuInfo>   desc2info_rtv;
			std::unordered_map<D3D12_DEPTH_STENCIL_VIEW_DESC, CpuInfo>   desc2info_dsv;

			CpuGpuInfo null_info_srv;
			CpuGpuInfo null_info_uav;

			CpuInfo null_info_dsv;
			CpuInfo null_info_rtv;

			bool HaveNullSrv() const {
				return null_info_srv.cpuHandle.ptr != 0;
			}
			bool HaveNullUav() const {
				return null_info_uav.cpuHandle.ptr != 0;
			}
			bool HaveNullDsv() const {
				return null_info_dsv.cpuHandle.ptr != 0;
			}
			bool HaveNullRtv() const {
				return null_info_rtv.cpuHandle.ptr != 0;
			}
		};
		// rsrcNodeIdx -> type
		std::unordered_map<size_t, DHHandles> handleMap;
		
		UDX12::DynamicSuballocMngr* csuDynamicDH{ nullptr };

		UDX12::DescriptorHeapAllocation csuDH;
		std::vector<UINT> csuDHfree;
		std::unordered_set<UINT> csuDHused;

		UDX12::DescriptorHeapAllocation rtvDH;
		std::vector<UINT> rtvDHfree;
		std::unordered_set<UINT> rtvDHused;

		UDX12::DescriptorHeapAllocation dsvDH;
		std::vector<UINT> dsvDHfree;
		std::unordered_set<UINT> dsvDHused;
	};
}
