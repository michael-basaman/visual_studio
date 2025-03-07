

#include "Provider.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <list>

Provider::Provider() {
    const wchar_t* providerKeyString = L"{E2E41B20-DCAF-406A-AD3F-2110A8172910}";
    const wchar_t* subLayerKeyString = L"{FC7F24D0-F50B-49EF-B366-F364022452D8}";

    bool providerKeyLoaded = loadGuidFromString(providerKeyString, providerKey);
    bool subLayerKeyLoaded = loadGuidFromString(subLayerKeyString, subLayerKey);

    if (providerKeyLoaded && subLayerKeyLoaded) {
        initialized = true;
        log_error("GUID's loaded", ERROR_SUCCESS);
    }
    else {
        if (!providerKeyLoaded) {
            log_error("Could not load providerKey", ERROR_SUCCESS);
        }

        if (!subLayerKeyLoaded) {
            log_error("Could not load subLayerKey", ERROR_SUCCESS);
        }

        initialized = false;
    }
}

bool Provider::loadGuidFromString(const wchar_t* guidString, GUID& guid) {
    HRESULT hr = CLSIDFromString(guidString, &guid);
    if (hr != S_OK) {
        return false;
    }
    return true;
}

DWORD Provider::Install(
)
{
    if (!initialized) {
        log_error("Install() not initialized", ERROR_SUCCESS);
        return FWP_E_INVALID_PARAMETER;
    }
    else {
        log_error("Install() start", ERROR_SUCCESS);
    }

    DWORD result = ERROR_SUCCESS;
    HANDLE engine = NULL;
    FWPM_SESSION0 session;
    FWPM_PROVIDER0 provider;
    FWPM_SUBLAYER0 subLayer;

    memset(&session, 0, sizeof(session));
    // The session name isn't required but may be useful for diagnostics.
    session.displayData.name = sessionName;
    // Set an infinite wait timeout, so we don't have to handle FWP_E_TIMEOUT
    // errors while waiting to acquire the transaction lock.
    session.txnWaitTimeoutInMSec = INFINITE;

    // The authentication service should always be RPC_C_AUTHN_DEFAULT.
    result = FwpmEngineOpen0(
        NULL,
        RPC_C_AUTHN_DEFAULT,
        NULL,
        &session,
        &engine
    );

    if (result != ERROR_SUCCESS) {
        FwpmEngineClose0(engine);

        log_error("Install() FwpmEngineOpen0", result);
        return result;
    }

    // We add the provider and sublayer from within a single transaction to make
    // it easy to clean up partial results in error paths.
    result = FwpmTransactionBegin0(engine, 0);

    if (result != ERROR_SUCCESS) {
        FwpmEngineClose0(engine);

        log_error("Install() FwpmTransactionBegin0", result);
        return result;
    }

    memset(&provider, 0, sizeof(provider));
    // The provider and sublayer keys are going to be used repeatedly when
    // adding filters and other objects. It's easiest to use well-known GUIDs
    // defined in a header somewhere, rather than having BFE generate the keys.
    provider.providerKey = providerKey;
    // For MUI compatibility, object names should be indirect strings. See
    // SHLoadIndirectString for details.
    provider.displayData.name = (PWSTR)providerName;
    // Since we always want the provider and sublayer to be present, it's
    // easiest to add them as persistent objects during install.  Alternatively,
    // we could add non-persistent objects every time our service starts.
    provider.flags = FWPM_PROVIDER_FLAG_PERSISTENT;

    result = FwpmProviderAdd0(engine, &provider, NULL);
    // Ignore FWP_E_ALREADY_EXISTS. This allows install to be re-run as needed
    // to repair a broken configuration.
    if (result != FWP_E_ALREADY_EXISTS && result != ERROR_SUCCESS) {
        FwpmEngineClose0(engine);

        log_error("Install() FwpmProviderAdd0", result);
        return result;
    }

    memset(&subLayer, 0, sizeof(subLayer));
    subLayer.subLayerKey = subLayerKey;
    subLayer.displayData.name = (PWSTR)subLayerName;
    subLayer.flags = FWPM_SUBLAYER_FLAG_PERSISTENT;
    // Link all our other objects to our provider. When multiple providers are
    // installed on a computer, this makes it easy to determine who added what.
    subLayer.providerKey = &providerKey;
    // We don't care what our sublayer weight is, so we pick a weight in the
    // middle and let BFE assign the closest available.
    subLayer.weight = 0x8000;

    result = FwpmSubLayerAdd0(engine, &subLayer, NULL);
    if (result != FWP_E_ALREADY_EXISTS && result != ERROR_SUCCESS) {
        FwpmEngineClose0(engine);

        log_error("Install() FwpmSubLayerAdd0", result);
        return result;
    }

    // Once all the adds have succeeded, we commit the transaction to persist
    // the new objects.
    result = FwpmTransactionCommit0(engine);

    if (result != ERROR_SUCCESS) {
        FwpmEngineClose0(engine);

        log_error("Install() FwpmTransactionCommit0", result);
        return result;
    }

    log_error("Install() installed", result);

    result = AddFilters(engine);

    if (result != ERROR_SUCCESS) {
        FwpmEngineClose0(engine);

        log_error("Install() AddFilters", result);
        return result;
    }

    // FwpmEngineClose0 accepts null engine handles, so we needn't precheck for
    // null. Also, when closing an engine handle, any transactions still in
    // progress are automatically aborted, so we needn't explicitly abort the
    // transaction in error paths.
    FwpmEngineClose0(engine);

    log_error("Install() success", result);

    return result;
}

DWORD Provider::Uninstall(
)
{
    if (!initialized) {
        log_error("Uninstall() not initialized", ERROR_SUCCESS);
        return FWP_E_INVALID_PARAMETER;
    }
    else {
        log_error("Uninstall() start", ERROR_SUCCESS);
    }

    DWORD result = ERROR_SUCCESS;
    HANDLE engine = NULL;
    FWPM_SESSION0 session;

    memset(&session, 0, sizeof(session));
    // The session name isn't required but may be useful for diagnostics.
    session.displayData.name = sessionName;
    // Set an infinite wait timeout, so we don't have to handle FWP_E_TIMEOUT
    // errors while waiting to acquire the transaction lock.
    session.txnWaitTimeoutInMSec = INFINITE;

    // The authentication service should always be RPC_C_AUTHN_DEFAULT.
    result = FwpmEngineOpen0(
        NULL,
        RPC_C_AUTHN_DEFAULT,
        NULL,
        &session,
        &engine
    );

    if (result != ERROR_SUCCESS) {
        FwpmEngineClose0(engine);

        log_error("Uninstall() FwpmEngineOpen0", result);
        return result;
    }

    // We delete the provider and sublayer from within a single transaction, so
    // that we always leave the system in a consistent state even in error
    // paths.
    result = FwpmTransactionBegin0(engine, 0);

    if (result != ERROR_SUCCESS) {
        FwpmEngineClose0(engine);

        log_error("Uninstall() FwpmTransactionBegin0", result);
        return result;
    }

    HANDLE enumHandle = NULL;
    FWPM_FILTER0** entries = NULL;
    UINT32 numEntriesRequested = INFINITE;
    UINT32 numEntriesReturned = 0;

    result = FwpmFilterCreateEnumHandle0(engine, NULL, &enumHandle);
    if (result != ERROR_SUCCESS) {
        FwpmEngineClose0(engine);

        log_error("Uninstall() FwpmFilterCreateEnumHandle0", result);
        return result;
    }

    // 3. Enumerate filters
    result = FwpmFilterEnum0(engine, enumHandle, numEntriesRequested, &entries, &numEntriesReturned);
    if (result != ERROR_SUCCESS) {
        // Handle error
        FwpmFilterDestroyEnumHandle0(engine, enumHandle);
        FwpmEngineClose0(engine);

        log_error("Uninstall() FwpmFilterEnum0", result);
        return result;
    }

    std::list<UINT64> filterIds;

    // 4. Process returned filters
    if (entries && numEntriesReturned > 0) {
        for (UINT32 i = 0; i < numEntriesReturned; ++i) {
            if (memcmp(&(entries[i]->subLayerKey), &subLayerKey, sizeof(GUID)) == 0) {
                filterIds.push_back(entries[i]->filterId);
            }
        }
    }

    // 5. Free memory
    if (entries) {
        void* ptr = (void*) entries;

        FwpmFreeMemory0(&ptr);
    }

    // 6. Destroy enumeration handle
    result = FwpmFilterDestroyEnumHandle0(engine, enumHandle);
    if (result != ERROR_SUCCESS) {
        FwpmEngineClose0(engine);

        log_error("Uninstall() FwpmFilterDestroyEnumHandle0", result);
        return result;
    }

    for (std::list<UINT64>::const_iterator filterId = filterIds.begin(); filterId != filterIds.end(); ++filterId) {
        result = FwpmFilterDeleteById0(engine, *filterId);

        if (result != ERROR_SUCCESS) {
            FwpmEngineClose0(engine);

            log_error("Uninstall() FwpmFilterDeleteById0", result);
            return result;
        }
    }

    // We have to delete the sublayer first since it references the provider. If
    // we tried to delete the provider first, it would fail with FWP_E_IN_USE.
    result = FwpmSubLayerDeleteByKey0(engine, &subLayerKey);
    if (result != FWP_E_SUBLAYER_NOT_FOUND && result != ERROR_SUCCESS) {
        FwpmEngineClose0(engine);

        log_error("Uninstall() FwpmSubLayerDeleteByKey0", result);
        return result;
    }

    result = FwpmProviderDeleteByKey0(engine, &providerKey);
    if (result != FWP_E_PROVIDER_NOT_FOUND && result != ERROR_SUCCESS) {
        FwpmEngineClose0(engine);

        log_error("Uninstall() FwpmProviderDeleteByKey0", result);
        return result;
    }

    // Once all the deletes have succeeded, we commit the transaction to
    // atomically delete all the objects.
    result = FwpmTransactionCommit0(engine);

    if (result != ERROR_SUCCESS) {
        FwpmEngineClose0(engine);

        log_error("Uninstall() FwpmTransactionCommit0", result);
        return result;
    }

    // FwpmEngineClose0 accepts null engine handles, so we needn't precheck for
    // null. Also, when closing an engine handle, any transactions still in
    // progress are automatically aborted, so we needn't explicitly abort the
    // transaction in error paths.
    FwpmEngineClose0(engine);

    log_error("Uninstall() success", result);

    return result;
}

UINT32 Provider::ip_string_to_uint32(const std::string& ip_string) {
    std::wstring stemp = std::wstring(ip_string.begin(), ip_string.end());

    sockaddr_in sa;
    int result = InetPton(AF_INET, stemp.c_str(), &(sa.sin_addr));
    if (result != 1) {
        std::stringstream ss;
        ss << "AddFilters() Error converting IP address: " << result;
        log_error(ss.str(), ERROR_SUCCESS);
        return 0;
    }
    return ntohl(sa.sin_addr.s_addr);
}

DWORD Provider::AddFilters(
    __in HANDLE engine
) {
    if (!initialized) {
        log_error("AddFilters() not initialized", ERROR_SUCCESS);
        return FWP_E_INVALID_PARAMETER;
    }
    else {
        log_error("AddFilters() start", ERROR_SUCCESS);
    }

    DWORD result = ERROR_SUCCESS;
    FWPM_FILTER_CONDITION0 conds[2];
    
    BOOL success;
    PSECURITY_DESCRIPTOR sd = NULL;
    FWP_BYTE_BLOB sdBlob;
    BOOL txnInProgress = FALSE;

    FWPM_FILTER_CONDITION0 anticirculatoryConditions[3];

    anticirculatoryConditions[0].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
    anticirculatoryConditions[0].matchType = FWP_MATCH_EQUAL;
    anticirculatoryConditions[0].conditionValue.type = FWP_UINT32;
    anticirculatoryConditions[0].conditionValue.uint32 = ip_string_to_uint32("129.159.75.153");

    anticirculatoryConditions[1].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
    anticirculatoryConditions[1].matchType = FWP_MATCH_EQUAL;
    anticirculatoryConditions[1].conditionValue.type = FWP_UINT16;
    anticirculatoryConditions[1].conditionValue.uint16 = 22;

    anticirculatoryConditions[2].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
    anticirculatoryConditions[2].matchType = FWP_MATCH_EQUAL;
    anticirculatoryConditions[2].conditionValue.type = FWP_UINT8;
    anticirculatoryConditions[2].conditionValue.uint8 = IPPROTO_TCP;

    FWPM_FILTER_CONDITION0 wsDiscoveryConditions[4];

    FWP_RANGE0 wsDiscoveryLocalAddressRange;
    wsDiscoveryLocalAddressRange.valueLow.type = FWP_UINT32;
    wsDiscoveryLocalAddressRange.valueLow.uint32 = ip_string_to_uint32("192.168.0.0");
    wsDiscoveryLocalAddressRange.valueHigh.type = FWP_UINT32;
    wsDiscoveryLocalAddressRange.valueHigh.uint32 = ip_string_to_uint32("192.168.255.255");

    FWP_RANGE0 wsDiscoveryRemoteAddressRange;
    wsDiscoveryRemoteAddressRange.valueLow.type = FWP_UINT32;
    wsDiscoveryRemoteAddressRange.valueLow.uint32 = ip_string_to_uint32("239.255.0.0");
    wsDiscoveryRemoteAddressRange.valueHigh.type = FWP_UINT32;
    wsDiscoveryRemoteAddressRange.valueHigh.uint32 = ip_string_to_uint32("239.255.255.255");

    wsDiscoveryConditions[0].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
    wsDiscoveryConditions[0].matchType = FWP_MATCH_RANGE;
    wsDiscoveryConditions[0].conditionValue.type = FWP_RANGE_TYPE;
    wsDiscoveryConditions[0].conditionValue.rangeValue = &wsDiscoveryLocalAddressRange;

    wsDiscoveryConditions[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
    wsDiscoveryConditions[1].matchType = FWP_MATCH_RANGE;
    wsDiscoveryConditions[1].conditionValue.type = FWP_RANGE_TYPE;
    wsDiscoveryConditions[1].conditionValue.rangeValue = &wsDiscoveryRemoteAddressRange;

    wsDiscoveryConditions[2].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
    wsDiscoveryConditions[2].matchType = FWP_MATCH_EQUAL;
    wsDiscoveryConditions[2].conditionValue.type = FWP_UINT16;
    wsDiscoveryConditions[2].conditionValue.uint16 = 3702;

    wsDiscoveryConditions[3].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
    wsDiscoveryConditions[3].matchType = FWP_MATCH_EQUAL;
    wsDiscoveryConditions[3].conditionValue.type = FWP_UINT8;
    wsDiscoveryConditions[3].conditionValue.uint8 = IPPROTO_UDP;

    FWPM_FILTER_CONDITION0 uPnPConditions[4];

    FWP_RANGE0 uPnPLocalAddressRange;
    uPnPLocalAddressRange.valueLow.type = FWP_UINT32;
    uPnPLocalAddressRange.valueLow.uint32 = ip_string_to_uint32("192.168.0.0");
    uPnPLocalAddressRange.valueHigh.type = FWP_UINT32;
    uPnPLocalAddressRange.valueHigh.uint32 = ip_string_to_uint32("192.168.255.255");

    FWP_RANGE0 uPnPRemoteAddressRange;
    uPnPRemoteAddressRange.valueLow.type = FWP_UINT32;
    uPnPRemoteAddressRange.valueLow.uint32 = ip_string_to_uint32("239.255.0.0");
    uPnPRemoteAddressRange.valueHigh.type = FWP_UINT32;
    uPnPRemoteAddressRange.valueHigh.uint32 = ip_string_to_uint32("239.255.255.255");

    uPnPConditions[0].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
    uPnPConditions[0].matchType = FWP_MATCH_RANGE;
    uPnPConditions[0].conditionValue.type = FWP_RANGE_TYPE;
    uPnPConditions[0].conditionValue.rangeValue = &uPnPLocalAddressRange;

    uPnPConditions[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
    uPnPConditions[1].matchType = FWP_MATCH_RANGE;
    uPnPConditions[1].conditionValue.type = FWP_RANGE_TYPE;
    uPnPConditions[1].conditionValue.rangeValue = &uPnPRemoteAddressRange;

    uPnPConditions[2].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
    uPnPConditions[2].matchType = FWP_MATCH_EQUAL;
    uPnPConditions[2].conditionValue.type = FWP_UINT16;
    uPnPConditions[2].conditionValue.uint16 = 1900;

    uPnPConditions[3].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
    uPnPConditions[3].matchType = FWP_MATCH_EQUAL;
    uPnPConditions[3].conditionValue.type = FWP_UINT8;
    uPnPConditions[3].conditionValue.uint8 = IPPROTO_UDP;

    FWPM_FILTER_CONDITION0 zeroConditions[5];

    zeroConditions[0].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
    zeroConditions[0].matchType = FWP_MATCH_EQUAL;
    zeroConditions[0].conditionValue.type = FWP_UINT32;
    zeroConditions[0].conditionValue.uint32 = ip_string_to_uint32("0.0.0.0");

    zeroConditions[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
    zeroConditions[1].matchType = FWP_MATCH_EQUAL;
    zeroConditions[1].conditionValue.type = FWP_UINT32;
    zeroConditions[1].conditionValue.uint32 = ip_string_to_uint32("255.255.255.255");

    zeroConditions[2].fieldKey = FWPM_CONDITION_IP_LOCAL_PORT;
    zeroConditions[2].matchType = FWP_MATCH_EQUAL;
    zeroConditions[2].conditionValue.type = FWP_UINT16;
    zeroConditions[2].conditionValue.uint16 = 68;

    zeroConditions[3].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
    zeroConditions[3].matchType = FWP_MATCH_EQUAL;
    zeroConditions[3].conditionValue.type = FWP_UINT16;
    zeroConditions[3].conditionValue.uint16 = 67;

    zeroConditions[4].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
    zeroConditions[4].matchType = FWP_MATCH_EQUAL;
    zeroConditions[4].conditionValue.type = FWP_UINT8;
    zeroConditions[4].conditionValue.uint8 = IPPROTO_UDP;

    FWPM_FILTER_CONDITION0 loopbackConditions[1];

    loopbackConditions[0].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
    loopbackConditions[0].matchType = FWP_MATCH_EQUAL;
    loopbackConditions[0].conditionValue.type = FWP_UINT32;
    loopbackConditions[0].conditionValue.uint32 = ip_string_to_uint32("127.0.0.1");

    FWPM_FILTER_CONDITION0 multicastConditions[2];

    FWP_RANGE0 multicastLocalAddressRange;
    multicastLocalAddressRange.valueLow.type = FWP_UINT32;
    multicastLocalAddressRange.valueLow.uint32 = ip_string_to_uint32("192.168.0.0");
    multicastLocalAddressRange.valueHigh.type = FWP_UINT32;
    multicastLocalAddressRange.valueHigh.uint32 = ip_string_to_uint32("192.168.255.255");

    FWP_RANGE0 multicastRemoteAddressRange;
    multicastRemoteAddressRange.valueLow.type = FWP_UINT32;
    multicastRemoteAddressRange.valueLow.uint32 = ip_string_to_uint32("224.0.0.0");
    multicastRemoteAddressRange.valueHigh.type = FWP_UINT32;
    multicastRemoteAddressRange.valueHigh.uint32 = ip_string_to_uint32("224.0.0.255");

    multicastConditions[0].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
    multicastConditions[0].matchType = FWP_MATCH_RANGE;
    multicastConditions[0].conditionValue.type = FWP_RANGE_TYPE;
    multicastConditions[0].conditionValue.rangeValue = &multicastLocalAddressRange;

    multicastConditions[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
    multicastConditions[1].matchType = FWP_MATCH_RANGE;
    multicastConditions[1].conditionValue.type = FWP_RANGE_TYPE;
    multicastConditions[1].conditionValue.rangeValue = &multicastRemoteAddressRange;

    FWPM_FILTER_CONDITION0 localConditions[2];

    FWP_RANGE0 localLocalAddressRange;
    localLocalAddressRange.valueLow.type = FWP_UINT32;
    localLocalAddressRange.valueLow.uint32 = ip_string_to_uint32("192.168.0.0");
    localLocalAddressRange.valueHigh.type = FWP_UINT32;
    localLocalAddressRange.valueHigh.uint32 = ip_string_to_uint32("192.168.255.255");

    FWP_RANGE0 localRemoteAddressRange;
    localRemoteAddressRange.valueLow.type = FWP_UINT32;
    localRemoteAddressRange.valueLow.uint32 = ip_string_to_uint32("192.168.0.0");
    localRemoteAddressRange.valueHigh.type = FWP_UINT32;
    localRemoteAddressRange.valueHigh.uint32 = ip_string_to_uint32("192.168.255.255");

    localConditions[0].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
    localConditions[0].matchType = FWP_MATCH_RANGE;
    localConditions[0].conditionValue.type = FWP_RANGE_TYPE;
    localConditions[0].conditionValue.rangeValue = &localLocalAddressRange;

    localConditions[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
    localConditions[1].matchType = FWP_MATCH_RANGE;
    localConditions[1].conditionValue.type = FWP_RANGE_TYPE;
    localConditions[1].conditionValue.rangeValue = &localRemoteAddressRange;

    FWPM_FILTER_CONDITION0 vpnTunnelConditions[1];

    FWP_RANGE0 localAddressRange;
    localAddressRange.valueLow.type = FWP_UINT32;
    localAddressRange.valueLow.uint32 = ip_string_to_uint32("10.8.0.0");
    localAddressRange.valueHigh.type = FWP_UINT32;
    localAddressRange.valueHigh.uint32 = ip_string_to_uint32("10.8.0.255");

    vpnTunnelConditions[0].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
    vpnTunnelConditions[0].matchType = FWP_MATCH_RANGE;
    vpnTunnelConditions[0].conditionValue.type = FWP_RANGE_TYPE;
    vpnTunnelConditions[0].conditionValue.rangeValue = &localAddressRange;

    FWPM_FILTER_CONDITION0 vpnConnectionCondition[3];

    vpnConnectionCondition[0].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
    vpnConnectionCondition[0].matchType = FWP_MATCH_EQUAL;
    vpnConnectionCondition[0].conditionValue.type = FWP_UINT32;
    vpnConnectionCondition[0].conditionValue.uint32 = ip_string_to_uint32("129.159.75.153");

    vpnConnectionCondition[1].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
    vpnConnectionCondition[1].matchType = FWP_MATCH_EQUAL;
    vpnConnectionCondition[1].conditionValue.type = FWP_UINT16;
    vpnConnectionCondition[1].conditionValue.uint16 = 1194;

    vpnConnectionCondition[2].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
    vpnConnectionCondition[2].matchType = FWP_MATCH_EQUAL;
    vpnConnectionCondition[2].conditionValue.type = FWP_UINT8;
    vpnConnectionCondition[2].conditionValue.uint8 = IPPROTO_TCP;

    FWPM_FILTER_CONDITION0 dnsPrimaryConditions[3];

    dnsPrimaryConditions[0].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
    dnsPrimaryConditions[0].matchType = FWP_MATCH_EQUAL;
    dnsPrimaryConditions[0].conditionValue.type = FWP_UINT32;
    dnsPrimaryConditions[0].conditionValue.uint32 = ip_string_to_uint32("208.67.222.222");

    dnsPrimaryConditions[1].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
    dnsPrimaryConditions[1].matchType = FWP_MATCH_EQUAL;
    dnsPrimaryConditions[1].conditionValue.type = FWP_UINT16;
    dnsPrimaryConditions[1].conditionValue.uint16 = 53;

    dnsPrimaryConditions[2].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
    dnsPrimaryConditions[2].matchType = FWP_MATCH_EQUAL;
    dnsPrimaryConditions[2].conditionValue.type = FWP_UINT8;
    dnsPrimaryConditions[2].conditionValue.uint8 = IPPROTO_UDP;

    FWPM_FILTER_CONDITION0 dnsSecondaryConditions[3];

    dnsSecondaryConditions[0].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
    dnsSecondaryConditions[0].matchType = FWP_MATCH_EQUAL;
    dnsSecondaryConditions[0].conditionValue.type = FWP_UINT32;
    dnsSecondaryConditions[0].conditionValue.uint32 = ip_string_to_uint32("208.67.220.220");

    dnsSecondaryConditions[1].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
    dnsSecondaryConditions[1].matchType = FWP_MATCH_EQUAL;
    dnsSecondaryConditions[1].conditionValue.type = FWP_UINT16;
    dnsSecondaryConditions[1].conditionValue.uint16 = 53;

    dnsSecondaryConditions[2].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
    dnsSecondaryConditions[2].matchType = FWP_MATCH_EQUAL;
    dnsSecondaryConditions[2].conditionValue.type = FWP_UINT8;
    dnsSecondaryConditions[2].conditionValue.uint8 = IPPROTO_UDP;

    FWPM_FILTER_CONDITION0 sshConditions[3];

    sshConditions[0].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
    sshConditions[0].matchType = FWP_MATCH_EQUAL;
    sshConditions[0].conditionValue.type = FWP_UINT32;
    sshConditions[0].conditionValue.uint32 = ip_string_to_uint32("129.153.229.173");

    sshConditions[1].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
    sshConditions[1].matchType = FWP_MATCH_EQUAL;
    sshConditions[1].conditionValue.type = FWP_UINT16;
    sshConditions[1].conditionValue.uint16 = 22;

    sshConditions[2].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
    sshConditions[2].matchType = FWP_MATCH_EQUAL;
    sshConditions[2].conditionValue.type = FWP_UINT8;
    sshConditions[2].conditionValue.uint8 = IPPROTO_TCP;

    FWPM_FILTER_CONDITION0 blockConditions[1];

    blockConditions[0].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
    //blockConditions[0].matchType = FWP_MATCH_EQUAL;
    blockConditions[0].matchType = FWP_MATCH_NOT_EQUAL;
    blockConditions[0].conditionValue.type = FWP_UINT32;
    blockConditions[0].conditionValue.uint32 = ip_string_to_uint32("129.153.229.173");

    result = FwpmTransactionBegin0(engine, 0);

    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmTransactionBegin0", result);
        return result;
    }

    txnInProgress = TRUE;

    // basamanConditions

    wchar_t anticirculatoryName[64] = L"VPNFirewall anticirculatory";
    FWPM_FILTER0 anticirculatoryFilter;
    memset(&anticirculatoryFilter, 0, sizeof(anticirculatoryFilter));
    anticirculatoryFilter.displayData.name = anticirculatoryName;
    anticirculatoryFilter.providerKey = &providerKey;
    anticirculatoryFilter.subLayerKey = subLayerKey;
    anticirculatoryFilter.filterCondition = anticirculatoryConditions;
    anticirculatoryFilter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
    anticirculatoryFilter.numFilterConditions = 3;
    anticirculatoryFilter.action.type = FWP_ACTION_PERMIT;
    anticirculatoryFilter.weight.type = FWP_UINT8;
    anticirculatoryFilter.weight.uint8 = 15;

    result = FwpmFilterAdd0(engine, &anticirculatoryFilter, NULL, NULL);

    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmFilterAdd0 0", result);
        return result;
    }

    wchar_t wsDiscoveryName[64] = L"VPNFirewall wsDiscovery";
    FWPM_FILTER0 wsDiscoveryFilter;
    memset(&wsDiscoveryFilter, 0, sizeof(wsDiscoveryFilter));
    wsDiscoveryFilter.displayData.name = wsDiscoveryName;
    wsDiscoveryFilter.providerKey = &providerKey;
    wsDiscoveryFilter.subLayerKey = subLayerKey;
    wsDiscoveryFilter.filterCondition = wsDiscoveryConditions;
    wsDiscoveryFilter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
    wsDiscoveryFilter.numFilterConditions = 4;
    wsDiscoveryFilter.action.type = FWP_ACTION_PERMIT;
    wsDiscoveryFilter.weight.type = FWP_UINT8;
    wsDiscoveryFilter.weight.uint8 = 15;

    result = FwpmFilterAdd0(engine, &wsDiscoveryFilter, NULL, NULL);

    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmFilterAdd0 1", result);
        return result;
    }

    wchar_t uPnPName[64] = L"VPNFirewall uPnP";
    FWPM_FILTER0 uPnPFilter;
    memset(&uPnPFilter, 0, sizeof(uPnPFilter));
    uPnPFilter.displayData.name = uPnPName;
    uPnPFilter.providerKey = &providerKey;
    uPnPFilter.subLayerKey = subLayerKey;
    uPnPFilter.filterCondition = uPnPConditions;
    uPnPFilter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
    uPnPFilter.numFilterConditions = 4;
    uPnPFilter.action.type = FWP_ACTION_PERMIT;
    uPnPFilter.weight.type = FWP_UINT8;
    uPnPFilter.weight.uint8 = 15;

    result = FwpmFilterAdd0(engine, &uPnPFilter, NULL, NULL);

    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmFilterAdd0 2", result);
        return result;
    }

    wchar_t zeroName[64] = L"VPNFirewall zero";
    FWPM_FILTER0 zeroFilter;
    memset(&zeroFilter, 0, sizeof(zeroFilter));
    zeroFilter.displayData.name = zeroName;
    zeroFilter.providerKey = &providerKey;
    zeroFilter.subLayerKey = subLayerKey;
    zeroFilter.filterCondition = zeroConditions;
    zeroFilter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
    zeroFilter.numFilterConditions = 5;
    zeroFilter.action.type = FWP_ACTION_PERMIT;
    zeroFilter.weight.type = FWP_UINT8;
    zeroFilter.weight.uint8 = 15;

    result = FwpmFilterAdd0(engine, &zeroFilter, NULL, NULL);

    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmFilterAdd0 3", result);
        return result;
    }

    wchar_t loopbackName[64] = L"VPNFirewall loopback";
    FWPM_FILTER0 loopbackFilter;
    memset(&loopbackFilter, 0, sizeof(loopbackFilter));
    loopbackFilter.displayData.name = loopbackName;
    loopbackFilter.providerKey = &providerKey;
    loopbackFilter.subLayerKey = subLayerKey;
    loopbackFilter.filterCondition = loopbackConditions;
    loopbackFilter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
    loopbackFilter.numFilterConditions = 1;
    loopbackFilter.action.type = FWP_ACTION_PERMIT;
    loopbackFilter.weight.type = FWP_UINT8;
    loopbackFilter.weight.uint8 = 15;

    result = FwpmFilterAdd0(engine, &loopbackFilter, NULL, NULL);

    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmFilterAdd0 4", result);
        return result;
    }

    wchar_t multicastName[64] = L"VPNFirewall multicast";
    FWPM_FILTER0 multicastFilter;
    memset(&multicastFilter, 0, sizeof(multicastFilter));
    multicastFilter.displayData.name = multicastName;
    multicastFilter.providerKey = &providerKey;
    multicastFilter.subLayerKey = subLayerKey;
    multicastFilter.filterCondition = multicastConditions;
    multicastFilter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
    multicastFilter.numFilterConditions = 2;
    multicastFilter.action.type = FWP_ACTION_PERMIT;
    multicastFilter.weight.type = FWP_UINT8;
    multicastFilter.weight.uint8 = 15;

    result = FwpmFilterAdd0(engine, &multicastFilter, NULL, NULL);

    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmFilterAdd0 5", result);
        return result;
    }

    wchar_t localName[64] = L"VPNFirewall local";
    FWPM_FILTER0 localFilter;
    memset(&localFilter, 0, sizeof(localFilter));
    localFilter.displayData.name = localName;
    localFilter.providerKey = &providerKey;
    localFilter.subLayerKey = subLayerKey;
    localFilter.filterCondition = localConditions;
    localFilter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
    localFilter.numFilterConditions = 2;
    localFilter.action.type = FWP_ACTION_PERMIT;
    localFilter.weight.type = FWP_UINT8;
    localFilter.weight.uint8 = 15;

    result = FwpmFilterAdd0(engine, &localFilter, NULL, NULL);

    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmFilterAdd0 6", result);
        return result;
    }

    wchar_t vpnTunnelName[64] = L"VPNFirewall vpnTunnel";
    FWPM_FILTER0 vpnTunnelFilter;
    memset(&vpnTunnelFilter, 0, sizeof(vpnTunnelFilter));
    vpnTunnelFilter.displayData.name = vpnTunnelName;
    vpnTunnelFilter.providerKey = &providerKey;
    vpnTunnelFilter.subLayerKey = subLayerKey;
    vpnTunnelFilter.filterCondition = vpnTunnelConditions;
    vpnTunnelFilter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
    vpnTunnelFilter.numFilterConditions = 1;
    vpnTunnelFilter.action.type = FWP_ACTION_PERMIT;
    vpnTunnelFilter.weight.type = FWP_UINT8;
    vpnTunnelFilter.weight.uint8 = 15;

    result = FwpmFilterAdd0(engine, &vpnTunnelFilter, NULL, NULL);

    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmFilterAdd0 7", result);
        return result;
    }

    wchar_t vpnConnectionName[64] = L"VPNFirewall vpnConnection";
    FWPM_FILTER0 vpnConnectionFilter;
    memset(&vpnConnectionFilter, 0, sizeof(vpnConnectionFilter));    
    vpnConnectionFilter.displayData.name = vpnConnectionName;
    vpnConnectionFilter.providerKey = &providerKey;
    vpnConnectionFilter.subLayerKey = subLayerKey;
    vpnConnectionFilter.filterCondition = vpnConnectionCondition;
    vpnConnectionFilter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
    vpnConnectionFilter.numFilterConditions = 3;
    vpnConnectionFilter.action.type = FWP_ACTION_PERMIT;
    vpnConnectionFilter.weight.type = FWP_UINT8;
    vpnConnectionFilter.weight.uint8 = 15;

    result = FwpmFilterAdd0(engine, &vpnConnectionFilter, NULL, NULL);
    
    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmFilterAdd0 8", result);
        return result;
    }

    wchar_t dnsPrimaryName[64] = L"VPNFirewall dnsPrimary";
    FWPM_FILTER0 dnsPrimaryFilter;
    memset(&dnsPrimaryFilter, 0, sizeof(dnsPrimaryFilter));
    dnsPrimaryFilter.displayData.name = dnsPrimaryName;
    dnsPrimaryFilter.providerKey = &providerKey;
    dnsPrimaryFilter.subLayerKey = subLayerKey;
    dnsPrimaryFilter.filterCondition = dnsPrimaryConditions;
    dnsPrimaryFilter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
    dnsPrimaryFilter.numFilterConditions = 3;
    dnsPrimaryFilter.action.type = FWP_ACTION_PERMIT;
    dnsPrimaryFilter.weight.type = FWP_UINT8;
    dnsPrimaryFilter.weight.uint8 = 15;

    result = FwpmFilterAdd0(engine, &dnsPrimaryFilter, NULL, NULL);

    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmFilterAdd0 9", result);
        return result;
    }

    wchar_t dnsSecondaryName[64] = L"VPNFirewall dnsSecondary";
    FWPM_FILTER0 dnsSecondaryFilter;
    memset(&dnsSecondaryFilter, 0, sizeof(dnsSecondaryFilter));
    dnsSecondaryFilter.displayData.name = dnsSecondaryName;
    dnsSecondaryFilter.providerKey = &providerKey;
    dnsSecondaryFilter.subLayerKey = subLayerKey;
    dnsSecondaryFilter.filterCondition = dnsSecondaryConditions;
    dnsSecondaryFilter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
    dnsSecondaryFilter.numFilterConditions = 3;
    dnsSecondaryFilter.action.type = FWP_ACTION_PERMIT;
    dnsSecondaryFilter.weight.type = FWP_UINT8;
    dnsSecondaryFilter.weight.uint8 = 15;

    result = FwpmFilterAdd0(engine, &dnsSecondaryFilter, NULL, NULL);

    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmFilterAdd0 10", result);
        return result;
    }

    wchar_t sshName[64] = L"VPNFirewall ssh";
    FWPM_FILTER0 sshFilter;
    memset(&sshFilter, 0, sizeof(sshFilter));
    sshFilter.displayData.name = sshName;
    sshFilter.providerKey = &providerKey;
    sshFilter.subLayerKey = subLayerKey;
    sshFilter.filterCondition = sshConditions;
    sshFilter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
    sshFilter.numFilterConditions = 3;
    sshFilter.action.type = FWP_ACTION_PERMIT;
    sshFilter.weight.type = FWP_UINT8;
    sshFilter.weight.uint8 = 15;

    result = FwpmFilterAdd0(engine, &sshFilter, NULL, NULL);

    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmFilterAdd0 11", result);
        return result;
    }

    wchar_t blockName[64] = L"VPNFirewall block";
    FWPM_FILTER0 blockFilter;
    memset(&blockFilter, 0, sizeof(blockFilter));
    blockFilter.displayData.name = blockName;
    blockFilter.providerKey = &providerKey;
    blockFilter.subLayerKey = subLayerKey;
    blockFilter.filterCondition = blockConditions;
    blockFilter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
    blockFilter.numFilterConditions = 1;
    blockFilter.action.type = FWP_ACTION_BLOCK;
    blockFilter.weight.type = FWP_UINT8;
    blockFilter.weight.uint8 = 14;

    result = FwpmFilterAdd0(engine, &blockFilter, NULL, NULL);

    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmFilterAdd0 12", result);
        return result;
    }

    // Once all the adds have succeeded, we commit the transaction to atomically
    // add all the new objects.
    result = FwpmTransactionCommit0(engine);
    
    if (result != ERROR_SUCCESS) {
        if (txnInProgress)
        {
            FwpmTransactionAbort0(engine);
        }
        LocalFree(sd);

        log_error("AddFilters() FwpmTransactionCommit0", result);
        return result;
    }

    txnInProgress = FALSE;

    LocalFree(sd);

    log_error("AddFilters() success", ERROR_SUCCESS);

    return result;
}

void Provider::log_error(const std::string& error, DWORD errorMessageID) {
    std::ofstream log_file("C:/Users/state/logs/vpnfirewall.txt", std::ios::app);
    if (log_file.is_open()) {
        log_file << error << ": " << GetErrorMessage(errorMessageID) << std::endl;
        log_file.close();
    }
}

std::string Provider::GetErrorMessage(DWORD errorMessageID) {
    if (errorMessageID == ERROR_SUCCESS) {
        return std::string();
    }

    LPSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    //Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}

void Provider::PrintMask16(const std::string& prefix) {
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 256; j++) {
            std::stringstream ss;
            ss << prefix;
            ss << ".";
            ss << i;
            ss << ".";
            ss << j;

            UINT32 addr = ip_string_to_uint32(ss.str());

            ss << " ";
            ss << addr;

            log_error(ss.str(), ERROR_SUCCESS);
        }
    }
}

HANDLE Provider::RandomHandle() {
    HCRYPTPROV hProv;
    DWORD dwLen = 8; // Number of bytes to generate
    BYTE pbBuffer[8];

    // Acquire a cryptographic service provider
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET)) {
        // Handle error
        return 0;
    }

    // Generate random bytes
    if (!CryptGenRandom(hProv, dwLen, pbBuffer)) {
        // Handle error
        CryptReleaseContext(hProv, 0);
        return 0;
    }

    // Use the generated random bytes as a handle (example)
    HANDLE randomHandle = (HANDLE)pbBuffer;

    // ... (Use the randomHandle)

    // Release the cryptographic service provider
    CryptReleaseContext(hProv, 0);

    return randomHandle;
}