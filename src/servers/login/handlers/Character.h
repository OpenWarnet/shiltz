#pragma once

#include "LoginDispatcher.h"

struct ServerSelect;
struct GenericCharacterPayload;
struct CreateCharacter;
struct SetCharacterMap;

void HandleGetCharacterList(const LoginContext& ctx, const ServerSelect& select);
void HandleDeleteCharacter(const LoginContext& ctx, const GenericCharacterPayload& request);
void HandleCancelDeleteCharacter(const LoginContext& ctx, const GenericCharacterPayload& request);
void HandleCreateCharacter(const LoginContext& ctx, const CreateCharacter& request);
void HandleUpdateCharacterLocation(const LoginContext& ctx, const SetCharacterMap& request);
