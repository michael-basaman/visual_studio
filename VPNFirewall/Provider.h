#pragma once

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "fwpuclnt.lib")
#pragma comment(lib, "ws2_32.lib")

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <sddl.h>
#include <fwpmu.h>
#include <stdio.h>
#include <string>
#include <list>
#include <sstream>

#define VPNFIREWALL_SESSION_NAME L"SDK Examples"

/*
providerKey: E2E41B20-DCAF-406A-AD3F-2110A8172910
subLayerKey: FC7F24D0-F50B-49EF-B366-F364022452D8
*/

typedef struct FilterInfo {
    FilterInfo(const std::string& guid, const std::string& action, const std::string& name, UINT64 effectiveWeight, bool isDummy,
        const std::string& remote_address, const std::string& remote_port, const std::string& local_address, const std::string& local_port, const std::string& protocol) :
            guid(guid),
            action(action),
            name(name),
            effectiveWeight(effectiveWeight),
            isDummy(isDummy),
            remote_address(remote_address),
            remote_port(remote_port),
            local_address(local_address),
            local_port(local_port),
            protocol(protocol) {
        std::stringstream ss;
        ss << effectiveWeight;
        weight = ss.str();
    }

    std::string weight;
    std::string guid;
    std::string action;
    std::string name;
    UINT64 effectiveWeight;
    bool isDummy;

    std::string remote_address;
    std::string remote_port;
    std::string local_address;
    std::string local_port;
    std::string protocol;

} FilterInfo;

class Provider {
private:
    wchar_t sessionName[64] = L"VPNFirewall Session";

    PCWSTR providerName = L"VPNFirewall Provider";
    PCWSTR subLayerName = L"VPNFirewall SubLayer";

    bool loadGuidFromString(const wchar_t* guidString, GUID& guid);

    UINT32 ip_string_to_uint32(const std::string& ip_string);

    std::string uint32_to_ip(UINT32 ip_int);

    std::string range_to_ip(UINT32 start, UINT32 end);

    //void log_error(const std::string& error, DWORD errorMessageID);

    std::string GetErrorMessage(DWORD errorMessageID);

    GUID providerKey;
    GUID subLayerKey;

    bool initialized;

public:
    Provider();

    void PrintMask16(const std::string& prefix);

    std::list<FilterInfo> GetFilters();

    DWORD Install(
    );

    DWORD Uninstall(
    );

    DWORD AddFilters(
        __in HANDLE engine
    );

    void log_error(const std::string& error, DWORD errorMessageID);
};