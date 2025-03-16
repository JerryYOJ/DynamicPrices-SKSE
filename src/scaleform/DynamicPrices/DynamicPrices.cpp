#include "DynamicPrices.h"

#include "../../json.hpp"

std::vector<std::pair<std::string, std::string >> DynamicPrices::CallbackMap{};

void DynamicPrices::Install() {
	REL::Relocation<std::uintptr_t> Vtbl{ RE::VTABLE_BarterMenu[0] };
	_PostCreate = Vtbl.write_vfunc(0x2, &PostCreate);

	logger::info("Hooked bartermenu");
}

void DynamicPrices::InstallLate()
{
	auto cfg = std::ifstream("Data/SKSE/Plugins/DynamicPrices.json", std::ios::in);
	if (!cfg.is_open()) {
		logger::critical("Failed to open configuration file!");
		return;
	}

	try {
		auto json = nlohmann::json::parse(cfg);
		auto callbacks = json.value("NativeCallbacks", nlohmann::json::array());
		for (auto&& cb : callbacks) {
			if (cb.is_string()) {
				auto string = cb.get<std::string>();
				auto&& ex_pos = string.find('!');
				if (ex_pos != std::string::npos) {
					auto module = string.substr(0, ex_pos);
					auto func = string.substr(ex_pos + 1);

					CallbackMap.push_back({ module, func });

					logger::info("Loaded Callback {}.{}", module, func);
				}
			}
		}
	}
	catch (const nlohmann::json::parse_error& e) {
		logger::critical("JSON parse error: {}\nException id: {}\nByte position: {}", e.what(), e.id, e.byte);
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

	using _GetFormEditorID = const char* (*)(std::uint32_t);
	static auto tweaks = GetModuleHandle(L"po3_Tweaks");
	static auto GetFormEditorID = reinterpret_cast<_GetFormEditorID>(GetProcAddress(tweaks, "GetFormEditorID"));

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
			queued.push({ lev, 0 });
			do {
				auto iter = queued.top();
				queued.pop();

				for (const auto& entry : iter.list->entries) {
					auto form = entry.form;
					if (form) {
						auto ll = form->As<RE::TESLevItem>();
						if (ll) queued.push({ ll, std::max(iter.level, entry.level) });
						else {
							if (hashMap.contains(form->formID)) hashMap[form->formID] = entry.level < hashMap[form->formID] ? entry.level : hashMap[form->formID];
							else hashMap[form->formID] = entry.level;
						}
					}
				}
				logger::trace("[Iterating] {}", GetFormEditorID(iter.list->formID));
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

		if (res.GetBool()) { // Vendor Items
			auto&& formID = thiz->GetRuntimeData().itemList->GetSelectedItem()->data.objDesc->GetObject()->formID;

			uint16_t PCL = RE::PlayerCharacter::GetSingleton()->GetLevel();
			if (ItemLevelMap.contains(formID) && ItemLevelMap[formID] > PCL) {
				float mult = (float)ItemLevelMap[formID] / (float)PCL;

				RE::GFxValue n(RE::GFxValue::ValueType::kString);
				a_updateObj.GetMember("name", &n);
				std::string name = n.GetString();
				name += std::format(" (LVL:{})", ItemLevelMap[formID]);
				n.SetString(name);
				a_updateObj.SetMember("name", n);

				RE::GFxValue value(RE::GFxValue::ValueType::kNumber);
				a_updateObj.GetMember("value", &value);
				value.SetNumber(value.GetNumber() * mult);
				a_updateObj.SetMember("value", value);

				logger::info("[{}] Raised Price from {} to {}", thiz->GetRuntimeData().itemList->GetSelectedItem()->data.GetName(), value.GetNumber() / mult, value.GetNumber());
			}
		}
		else {

		}

	}

	oldFunc.Invoke("call", a_params.retVal, a_params.argsWithThisRef, a_params.argCount + 1);
}