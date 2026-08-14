#pragma once

#include "GameDispatcher.h"

struct StoreOpen;
struct StorePwModify;
struct StoreClose;
struct StoreItemOut;
struct StoreItemIn;
struct StoreMoneyOut;
struct StoreMoneyIn;

void HandleStoreOpen(const GameContext& ctx, const StoreOpen& request);
void HandleStorePwModify(const GameContext& ctx, const StorePwModify& request);
void HandleStoreClose(const GameContext& ctx, const StoreClose& request);
void HandleStoreItemOut(const GameContext& ctx, const StoreItemOut& request);
void HandleStoreItemIn(const GameContext& ctx, const StoreItemIn& request);
void HandleStoreMoneyOut(const GameContext& ctx, const StoreMoneyOut& request);
void HandleStoreMoneyIn(const GameContext& ctx, const StoreMoneyIn& request);
