#pragma once

#include "GameContext.h"

struct StoreCreate;
struct StoreOpen;
struct StorePwModify;
struct StoreClose;
struct StoreItemOut;
struct StoreItemIn;
struct StoreMoneyOut;
struct StoreMoneyIn;
struct Player;

void HandleStoreCreate(const GameContext& ctx, const StoreCreate& request, Player& player);
void HandleStoreOpen(const GameContext& ctx, const StoreOpen& request, Player& player);
void HandleStorePwModify(const GameContext& ctx, const StorePwModify& request, Player& player);
void HandleStoreClose(const GameContext& ctx, const StoreClose& request, Player& player);
void HandleStoreItemOut(const GameContext& ctx, const StoreItemOut& request, Player& player);
void HandleStoreItemIn(const GameContext& ctx, const StoreItemIn& request, Player& player);
void HandleStoreMoneyOut(const GameContext& ctx, const StoreMoneyOut& request, Player& player);
void HandleStoreMoneyIn(const GameContext& ctx, const StoreMoneyIn& request, Player& player);
