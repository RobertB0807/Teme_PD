#include <windows.h>
#include <cfgmgr32.h>
#include <setupapi.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static bool get_device_property_raw(
    HDEVINFO dev_info_set,
    SP_DEVINFO_DATA *dev_info_data,
    DWORD property,
    DWORD *reg_type,
    std::vector<BYTE> &buffer
) {
    DWORD needed = 0;
    *reg_type = 0;

    if (!SetupDiGetDeviceRegistryPropertyW(
            dev_info_set,
            dev_info_data,
            property,
            reg_type,
            nullptr,
            0,
            &needed)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return false;
        }
    }

    if (needed == 0) {
        return false;
    }

    buffer.resize(needed);
    if (!SetupDiGetDeviceRegistryPropertyW(
            dev_info_set,
            dev_info_data,
            property,
            reg_type,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            &needed)) {
        return false;
    }

    return true;
}

static bool get_device_property_string(
    HDEVINFO dev_info_set,
    SP_DEVINFO_DATA *dev_info_data,
    DWORD property,
    std::wstring &out
) {
    DWORD reg_type = 0;
    std::vector<BYTE> data;
    out.clear();

    if (!get_device_property_raw(dev_info_set, dev_info_data, property, &reg_type, data)) {
        return false;
    }

    if (reg_type == REG_SZ || reg_type == REG_EXPAND_SZ) {
        const wchar_t *value = reinterpret_cast<const wchar_t *>(data.data());
        out = value;
        return true;
    }

    if (reg_type == REG_MULTI_SZ) {
        const wchar_t *p = reinterpret_cast<const wchar_t *>(data.data());
        bool first = true;

        while (*p != L'\0') {
            if (!first) {
                out += L" | ";
            }
            out += p;
            p += wcslen(p) + 1;
            first = false;
        }

        return !out.empty();
    }

    return false;
}

static bool get_device_property_dword(
    HDEVINFO dev_info_set,
    SP_DEVINFO_DATA *dev_info_data,
    DWORD property,
    DWORD *value
) {
    DWORD reg_type = 0;
    std::vector<BYTE> data;

    if (!get_device_property_raw(dev_info_set, dev_info_data, property, &reg_type, data)) {
        return false;
    }

    if (reg_type != REG_DWORD || data.size() < sizeof(DWORD)) {
        return false;
    }

    *value = *reinterpret_cast<const DWORD *>(data.data());
    return true;
}

static std::wstring get_instance_id(HDEVINFO dev_info_set, SP_DEVINFO_DATA *dev_info_data) {
    DWORD needed = 0;
    std::wstring id;

    SetupDiGetDeviceInstanceIdW(dev_info_set, dev_info_data, nullptr, 0, &needed);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || needed == 0) {
        return L"";
    }

    std::vector<wchar_t> buffer(needed);
    if (!SetupDiGetDeviceInstanceIdW(
            dev_info_set,
            dev_info_data,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            &needed)) {
        return L"";
    }

    id.assign(buffer.data());
    return id;
}

static std::wstring get_best_name(HDEVINFO dev_info_set, SP_DEVINFO_DATA *dev_info_data) {
    std::wstring name;
    if (get_device_property_string(dev_info_set, dev_info_data, SPDRP_FRIENDLYNAME, name)) {
        return name;
    }
    if (get_device_property_string(dev_info_set, dev_info_data, SPDRP_DEVICEDESC, name)) {
        return name;
    }
    return L"(unknown device name)";
}

static void print_string_meta(
    HDEVINFO dev_info_set,
    SP_DEVINFO_DATA *dev_info_data,
    DWORD property,
    const wchar_t *label
) {
    std::wstring value;
    if (get_device_property_string(dev_info_set, dev_info_data, property, value)) {
        wprintf(L"%ls: %ls\n", label, value.c_str());
    } else {
        wprintf(L"%ls: (not available)\n", label);
    }
}

static void print_dword_meta(
    HDEVINFO dev_info_set,
    SP_DEVINFO_DATA *dev_info_data,
    DWORD property,
    const wchar_t *label
) {
    DWORD value = 0;
    if (get_device_property_dword(dev_info_set, dev_info_data, property, &value)) {
        wprintf(L"%ls: %lu (0x%08lX)\n", label, value, value);
    } else {
        wprintf(L"%ls: (not available)\n", label);
    }
}

static void print_device_list(HDEVINFO dev_info_set) {
    DWORD index = 0;
    SP_DEVINFO_DATA dev_info_data;
    dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

    wprintf(L"Connected devices (index -> name):\n");

    while (SetupDiEnumDeviceInfo(dev_info_set, index, &dev_info_data)) {
        std::wstring name = get_best_name(dev_info_set, &dev_info_data);
        wprintf(L"  [%lu] %ls\n", index, name.c_str());
        ++index;
    }

    wprintf(L"Total devices found: %lu\n", index);
}

static void print_device_metadata(HDEVINFO dev_info_set, SP_DEVINFO_DATA *dev_info_data) {
    wchar_t class_guid[64] = {0};
    ULONG status = 0;
    ULONG problem = 0;
    CONFIGRET cr;

    std::wstring instance_id = get_instance_id(dev_info_set, dev_info_data);
    StringFromGUID2(dev_info_data->ClassGuid, class_guid, 64);

    wprintf(L"\n===== Device Metadata =====\n");
    wprintf(L"Instance ID: %ls\n", instance_id.empty() ? L"(not available)" : instance_id.c_str());
    wprintf(L"Class GUID: %ls\n", class_guid[0] ? class_guid : L"(not available)");

    print_string_meta(dev_info_set, dev_info_data, SPDRP_FRIENDLYNAME, L"Friendly Name");
    print_string_meta(dev_info_set, dev_info_data, SPDRP_DEVICEDESC, L"Device Description");
    print_string_meta(dev_info_set, dev_info_data, SPDRP_CLASS, L"Class Name");
    print_string_meta(dev_info_set, dev_info_data, SPDRP_MFG, L"Manufacturer");
    print_string_meta(dev_info_set, dev_info_data, SPDRP_ENUMERATOR_NAME, L"Enumerator");
    print_string_meta(dev_info_set, dev_info_data, SPDRP_LOCATION_INFORMATION, L"Location");
    print_string_meta(dev_info_set, dev_info_data, SPDRP_SERVICE, L"Service");
    print_string_meta(dev_info_set, dev_info_data, SPDRP_DRIVER, L"Driver Key");
    print_string_meta(dev_info_set, dev_info_data, SPDRP_HARDWAREID, L"Hardware IDs");
    print_string_meta(dev_info_set, dev_info_data, SPDRP_COMPATIBLEIDS, L"Compatible IDs");
    print_dword_meta(dev_info_set, dev_info_data, SPDRP_CAPABILITIES, L"Capabilities");
    print_dword_meta(dev_info_set, dev_info_data, SPDRP_CONFIGFLAGS, L"Config Flags");

    cr = CM_Get_DevNode_Status(&status, &problem, dev_info_data->DevInst, 0);
    if (cr == CR_SUCCESS) {
        wprintf(L"DevNode Status: 0x%08lX\n", static_cast<unsigned long>(status));
        wprintf(L"DevNode Problem Code: %lu\n", static_cast<unsigned long>(problem));
    } else {
        wprintf(L"DevNode Status: (not available, CM error %lu)\n", static_cast<unsigned long>(cr));
    }
}

int wmain(int argc, wchar_t *argv[]) {
    HDEVINFO dev_info_set;
    SP_DEVINFO_DATA dev_info_data;
    DWORD target_index;
    wchar_t *end_ptr = nullptr;

    dev_info_set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (dev_info_set == INVALID_HANDLE_VALUE) {
        wprintf(L"SetupDiGetClassDevsW failed with error %lu\n", GetLastError());
        return 1;
    }

    if (argc != 2) {
        wprintf(L"Usage:\n");
        wprintf(L"  %ls <device_index>\n\n", argv[0]);
        print_device_list(dev_info_set);
        SetupDiDestroyDeviceInfoList(dev_info_set);
        return 1;
    }

    target_index = static_cast<DWORD>(wcstoul(argv[1], &end_ptr, 10));
    if (argv[1][0] == L'\0' || (end_ptr != nullptr && *end_ptr != L'\0')) {
        wprintf(L"Invalid device index: %ls\n", argv[1]);
        SetupDiDestroyDeviceInfoList(dev_info_set);
        return 1;
    }

    dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
    if (!SetupDiEnumDeviceInfo(dev_info_set, target_index, &dev_info_data)) {
        wprintf(L"Device index %lu not found.\n\n", target_index);
        print_device_list(dev_info_set);
        SetupDiDestroyDeviceInfoList(dev_info_set);
        return 1;
    }

    print_device_metadata(dev_info_set, &dev_info_data);
    SetupDiDestroyDeviceInfoList(dev_info_set);
    return 0;
}