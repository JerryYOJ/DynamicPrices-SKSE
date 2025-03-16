#undef GetObject

class DynamicPrices : public RE::GFxFunctionHandler {
public:
	DynamicPrices(const RE::GFxValue&& old, std::unordered_map<RE::FormID, uint16_t>&& map, RE::BarterMenu* menu) : oldFunc(old), ItemLevelMap(map), thiz(menu) {}

	void Call(Params& a_params) override;

	static void Install();
	static void InstallLate();
private:
	static void PostCreate(RE::BarterMenu* thiz);
	static inline REL::Relocation<decltype(&RE::BarterMenu::PostCreate)> _PostCreate;

	RE::GFxValue oldFunc;
	std::unordered_map<RE::FormID, uint16_t>ItemLevelMap;
	RE::BarterMenu* thiz;

	static std::vector<std::pair<std::string, std::string >>CallbackMap;
};