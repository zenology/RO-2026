// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

/**
 * Custom Script Commands
 * Place the body of custom script commands in this file.
 * Format:
 *	BUILDIN_FUNC(command_name)
 *	{
 *		<code>;
 *	}
 **/

//BUILDIN_FUNC(example)
//{
//	ShowInfo("buildin_example: It works!\n");
//	script_pushint(st,1);
//	return 0;
//}

#include "black_market_cashshop.hpp"

BUILDIN_FUNC(callblackmarketshop)
{
	const char *shopname = script_getstr(st, 2);
	const int32 flag = script_getnum(st, 3);
	const int32 rate = script_getnum(st, 4);
	TBL_PC *sd = nullptr;
	if (!script_rid2sd(sd)) {
		ShowError("buildin_callblackmarketshop: Shop [%s] has no attached player RID (rate %d).\n", shopname, rate);
		script_pushint(st, 0);
		return SCRIPT_CMD_FAILURE;
	}

	npc_data *nd = npc_name2id(shopname);
	if (!nd || nd->type != BL_NPC || nd->subtype != NPCTYPE_POINTSHOP) {
		ShowError("buildin_callblackmarketshop: Shop [%s] is not a pointshop (rate %d).\n", shopname, rate);
		script_pushint(st, 0);
		return SCRIPT_CMD_FAILURE;
	}
	if (std::strcmp(nd->u.shop.pointshop_str, "#CASHPOINTS") != 0) {
		ShowError("buildin_callblackmarketshop: Shop [%s] does not use #CASHPOINTS (rate %d).\n", shopname, rate);
		script_pushint(st, 0);
		return SCRIPT_CMD_FAILURE;
	}
	if (rate < -10 || rate > 30) {
		ShowError("buildin_callblackmarketshop: Shop [%s] has invalid daily rate %d.\n", shopname, rate);
		script_pushint(st, 0);
		return SCRIPT_CMD_FAILURE;
	}
	if (nd->u.shop.count > 0 && !nd->u.shop.shop_item) {
		ShowError("buildin_callblackmarketshop: Shop [%s] has no item catalog (rate %d).\n", shopname, rate);
		script_pushint(st, 0);
		return SCRIPT_CMD_FAILURE;
	}

	auto &cache = black_market_catalog_cache_by_shop[shopname];
	bool recapture = cache.item_ids.size() != nd->u.shop.count ||
		cache.base_prices.size() != nd->u.shop.count ||
		cache.last_applied_prices.size() != nd->u.shop.count;
	if (!recapture) {
		for (uint16 i = 0; i < nd->u.shop.count; ++i) {
			const auto &item = nd->u.shop.shop_item[i];
			if (cache.item_ids[i] != item.nameid || cache.last_applied_prices[i] != item.value) {
				recapture = true;
				break;
			}
		}
	}
	if (recapture) {
		black_market_catalog_cache replacement;
		replacement.item_ids.reserve(nd->u.shop.count);
		replacement.base_prices.reserve(nd->u.shop.count);
		replacement.last_applied_prices.reserve(nd->u.shop.count);
		for (uint16 i = 0; i < nd->u.shop.count; ++i) {
			const auto &item = nd->u.shop.shop_item[i];
			if (item.value == 0 || item.value > static_cast<uint32>(std::numeric_limits<int32_t>::max())) {
				ShowError("buildin_callblackmarketshop: Shop [%s] has invalid base price for item %u (rate %d).\n", shopname, item.nameid, rate);
				script_pushint(st, 0);
				return SCRIPT_CMD_FAILURE;
			}
			replacement.item_ids.push_back(item.nameid);
			replacement.base_prices.push_back(item.value);
			replacement.last_applied_prices.push_back(item.value);
		}
		cache = std::move(replacement);
	}

	std::vector<uint32> adjusted_prices;
	adjusted_prices.reserve(cache.base_prices.size());
	for (uint32 base_price : cache.base_prices) {
		int32 adjusted_price = 0;
		if (!black_market_adjusted_price(base_price, rate, adjusted_price)) {
			ShowError("buildin_callblackmarketshop: Shop [%s] cannot adjust base price %u at rate %d.\n", shopname, base_price, rate);
			script_pushint(st, 0);
			return SCRIPT_CMD_FAILURE;
		}
		adjusted_prices.push_back(static_cast<uint32>(adjusted_price));
	}
	for (uint16 i = 0; i < nd->u.shop.count; ++i)
		nd->u.shop.shop_item[i].value = adjusted_prices[i];
	cache.last_applied_prices = adjusted_prices;

	return buildin_callshop(st);
}
