#pragma once

#include "LoginDispatcher.h"

struct Login;

void HandleClLogin(const LoginContext& ctx, const Login& login);
void HandleClUserSystemSpecInfo(const LoginContext& ctx);
void HandleClGameguard(const LoginContext& ctx);
