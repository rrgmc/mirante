#pragma once

namespace mirante {

class Attention;
class SecretStore;
class ServerClients;
class StatusCache;

// The api library's long-lived services, made by main.cpp and handed to the
// window. A null member turns its feature off, as in most tests.
struct ApiServices
{
    SecretStore *secrets = nullptr;
    ServerClients *servers = nullptr;
    StatusCache *status = nullptr;
    Attention *attention = nullptr;
};

} // namespace mirante
