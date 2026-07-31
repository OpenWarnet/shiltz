#pragma once

#include "LoginDispatcher.h"

struct ServerSelect;
struct GenericCharacterPayload;
struct CreateCharacter;
struct SetCharacterMap;

void HandleClGetCharinfo(const LoginContext& ctx, const ServerSelect& select);
void HandleClDeleteCharacter(const LoginContext& ctx, const GenericCharacterPayload& request);
void HandleClCharDeleteCancle(const LoginContext& ctx, const GenericCharacterPayload& request);
void HandleClCreateCharacter(const LoginContext& ctx, const CreateCharacter& request);
void HandleClCreateMapNum(const LoginContext& ctx, const SetCharacterMap& request);
