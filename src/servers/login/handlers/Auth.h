#pragma once

#include "LoginDispatcher.h"

struct Login;

void HandleLogin(const LoginContext& ctx, const Login& login);
void HandleUserSystemSpecInfo(const LoginContext& ctx);
void HandleGameguard(const LoginContext& ctx);
