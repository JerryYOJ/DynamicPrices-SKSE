#include "DynamicPrices.h"

#include <future>
#include <filesystem>

#include "../../json.hpp"

std::vector<std::pair<std::string, std::string >> DynamicPrices::NativeCallbackMap{};
//std::vector<std::pair<std::string, std::string >> DynamicPrices::PapyrusCallbackMap{};

void DynamicPrices::Install() {
	REL::Relocation<std::uintptr_t> Vtbl{ RE::VTABLE_BarterMenu[0] };
	_PostCreate = Vtbl.write_vfunc(0x2, &PostCreate);

	logger::info("Hooked bartermenu");
}

void DynamicPrices::InstallLate()
{
	for (const auto& entry : std::filesystem::directory_iterator("Data/SKSE/DynamicPrices")) {
		auto cfg = std::ifstream(entry.path().native(), std::ios::in);
		if (cfg.is_open()) {
			try {
				auto json = nlohmann::json::parse(cfg);
				auto ncb = json.value("NativeCallbacks", nlohmann::json::array());
				for (auto&& cb : ncb) {
					if (cb.is_string()) {
						auto string = cb.get<std::string>();
						auto&& ex_pos = string.find('!');
						if (ex_pos != std::string::npos) {
							auto module = string.substr(0, ex_pos);
							auto func = string.substr(ex_pos + 1);

							NativeCallbackMap.push_back({ module, func });

							logger::info("Loaded NativeCallback {}.{}", module, func);
						}
					}
				}

				//auto pcb = json.value("PapyrusCallbacks", nlohmann::json::array());
				//for (auto&& cb : pcb) {
				//	if (cb.is_string()) {
				//		auto string = cb.get<std::string>();
				//		auto&& ex_pos = string.find('!');
				//		if (ex_pos != std::string::npos) {
				//			auto module = string.substr(0, ex_pos);
				//			auto func = string.substr(ex_pos + 1);

				//			PapyrusCallbackMap.push_back({ module, func });

				//			logger::info("Loaded PapyrusCallback {}.{}", module, func);
				//		}
				//	}
				//}
			}
			catch (const nlohmann::json::parse_error& e) {
				logger::critical("JSON parse error: {}\nException id: {}\nByte position: {}\nFilename: {}", e.what(), e.id, e.byte, entry.path().string());
			}
		}
	}

	return;
}

void DynamicPrices::PostCreate(RE::BarterMenu* thiz) {
	_PostCreate(thiz);

	static REL::Relocation<RE::RefHandle*> TraderRefhandle{ RELOCATION_ID(519283, 405823) };

	auto&& traderRef = *TraderRefhandle;
	auto&& trader = RE::TESObjectREFR::LookupByHandle(traderRef)->As<RE::Actor>();

	if (!trader) {
		logger::critical("[PostCreate] Couldnt find the trader of this trade attempt!");
		return;
	}

	std::unordered_map<RE::FormID, uint16_t> hashMap{};

	struct LevIter {
		RE::TESLevItem* list;
		uint16_t level;
	};

	RE::TESFaction* vendorfact = nullptr;
	for (int cycle = 1; cycle <= 10 && vendorfact == nullptr; cycle++, vendorfact = trader->GetVendorFaction()); //weird

	if (!vendorfact) {
		logger::critical("[PostCreate] Couldnt find the trader vendor faction!");
		return;
	}

	for (auto&& [obj, entry] : vendorfact->vendorData.merchantContainer->GetInventory()) {
		if (obj->As<RE::TESLevItem>() != nullptr) {
			auto&& lev = obj->As<RE::TESLevItem>();

			std::stack<LevIter> queued;
			queued.push({ lev, 1 });
			do {
				auto iter = queued.top();
				queued.pop();

				for (const auto& entry : iter.list->entries) {
					auto form = entry.form;
					if (form) {
						auto ll = form->As<RE::TESLevItem>();
						if (ll) queued.push({ ll, std::max(iter.level, entry.level) });
						else {
							uint16_t level = std::max(iter.level, entry.level);
							if (hashMap.contains(form->formID)) hashMap[form->formID] = level < hashMap[form->formID] ? level : hashMap[form->formID];
							else hashMap[form->formID] = level;
						}
					}
				}
			} while (!queued.empty());
		}
	}

	auto&& barter = thiz;
	auto&& root = barter->GetRuntimeData().root;

	RE::GFxValue oldf;
	root.GetMember("UpdateItemCardInfo", &oldf);

	RE::GFxValue newf;
	auto&& impl = RE::make_gptr<DynamicPrices>(std::move(oldf), std::move(hashMap), thiz);
	barter->uiMovie->CreateFunction(&newf, impl.get());

	root.SetMember("UpdateItemCardInfo", newf);
}

void DynamicPrices::Call(Params& a_params) {
	if (a_params.argCount >= 1) {
		auto&& root = a_params.thisPtr;

		auto&& a_updateObj = a_params.args[0];

		RE::GFxValue res(RE::GFxValue::ValueType::kBoolean);
		root->Invoke("isViewingVendorItems", &res);

		using _Callback = float(*)(RE::Actor*, RE::InventoryEntryData* objDesc, uint16_t a_level, RE::GFxValue& a_updateObj, bool is_buying);

		auto&& formID = thiz->GetRuntimeData().itemList->GetSelectedItem()->data.objDesc->GetObject()->formID;
		
		static REL::Relocation<RE::RefHandle*> TraderRefhandle{ RELOCATION_ID(519283, 405823) };
		auto traderRef = *TraderRefhandle;
		auto trader = RE::TESObjectREFR::LookupByHandle(traderRef)->As<RE::Actor>();

		if (ItemLevelMap.contains(formID)) {
			float mult = 1.0f;

			for (auto&& it : NativeCallbackMap) {
				auto mod = GetModuleHandle(it.first.c_str());
				auto cb = reinterpret_cast<_Callback>(GetProcAddress(mod, it.second.c_str()));
				if(cb) mult *= cb(trader, thiz->GetRuntimeData().itemList->GetSelectedItem()->data.objDesc, ItemLevelMap.at(formID), a_updateObj, res.GetBool());
				
				logger::debug("[Call] mod:{},{} cb:{},{}", it.first, (void*)mod, it.second, (void*)cb);
			}
			//for (auto&& it : PapyrusCallbackMap) {
			//	auto&& vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			//	auto args = RE::MakeFunctionArguments();

			//	std::promise<float> ret;
			//	std::future<float> fut = ret.get_future();

			//	auto callback = VmCallback::New([&ret](const RE::BSScript::Variable& a_var) {
			//		ret.set_value(a_var.GetFloat());
			//	});
			//	vm->DispatchStaticCall(it.first, it.second, args, callback);

			//	mult *= fut.get();

			//	logger::debug("[Call] mod:{} cb:{} fut:{}", it.first, it.second, fut.get());
			//}

			RE::GFxValue value(RE::GFxValue::ValueType::kNumber);
			a_updateObj.GetMember("value", &value);
			value.SetNumber(value.GetNumber() * mult);
			a_updateObj.SetMember("value", value);
		}
	}

	oldFunc.Invoke("call", a_params.retVal, a_params.argsWithThisRef, a_params.argCount + 1);
}